#include "shutdown.h"
#include "common.h"
#include "helper.h"

#include <vdr/cutter.h>
#include <vdr/menu.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/shutdown.h>
#include <vdr/timers.h>


class cDBusShutdownHelper
{
public:
  static const char *_xmlNodeInfo;

  static void SendReply(GDBusMethodInvocation *Invocation, int ReplyCode, const char *ReplyMessage, int ShExitCode, const char *ShParameter)
  {
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(isis)"));
    g_variant_builder_add(builder, "i", ReplyCode);
    g_variant_builder_add(builder, "s", ReplyMessage);
    g_variant_builder_add(builder, "i", ShExitCode);
    g_variant_builder_add(builder, "s", ShParameter);
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  };

  static void ConfirmShutdown(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gboolean ignoreuser = FALSE;
    g_variant_get(Parameters, "(b)", &ignoreuser);

    // this is nearly a copy of vdr's cShutdownHandler::ConfirmShutdown
    // if the original changes take this into account

    if (!ignoreuser && !ShutdownHandler.IsUserInactive()) {
       SendReply(Invocation, 901, "user is active", 0, "");
       return;
       }

    if (cCutter::Active()) {
       SendReply(Invocation, 902, "cutter is active", 0, "");
       return;
       }

    time_t Now = time(NULL);
    cTimer *timer = Timers.GetNextActiveTimer();
    time_t Next = timer ? timer->StartTime() : 0;
    time_t Delta = timer ? Next - Now : 0;
    if (cRecordControls::Active() || (Next && Delta <= 0)) {
       // VPS recordings in timer end margin may cause Delta <= 0
       SendReply(Invocation, 903, "recording is active", 0, "");
       return;
       }
    else if (Next && Delta <= Setup.MinEventTimeout * 60) {
       // Timer within Min Event Timeout
       SendReply(Invocation, 904, "recording is active in the near future", 0, "");
       return;
       }

    if (cPluginManager::Active(NULL)) {
       SendReply(Invocation, 905, "some plugin is active", 0, "");
       return;
       }

    cPlugin *Plugin = cPluginManager::GetNextWakeupPlugin();
    time_t NextPlugin = Plugin ? Plugin->WakeupTime() : 0;
    Delta = NextPlugin ? NextPlugin - Now : 0;
    if (NextPlugin && Delta <= Setup.MinEventTimeout * 60) {
       // Plugin wakeup within Min Event Timeout
       cString buf = cString::sprintf("plugin %s wakes up in %ld min", Plugin->Name(), Delta / 60);
       SendReply(Invocation, 906, *buf, 0, "");
       return;
       }

    // insanity check: ask vdr again, if implementation of ConfirmShutdown has changed...
    if (cRemote::Enabled() && !ShutdownHandler.ConfirmShutdown(false)) {
       SendReply(Invocation, 550, "vdr is not ready for shutdown", 0, "");
       return;
       }

    if (((*cDBusShutdown::_shutdownHooksDir) != NULL) && (strlen(*cDBusShutdown::_shutdownHooksDir) > 0)
     && ((*cDBusShutdown::_shutdownHooksWrapper) != NULL) && (strlen(*cDBusShutdown::_shutdownHooksWrapper) > 0)) {
       if (NextPlugin && (!Next || Next > NextPlugin)) {
          Next = NextPlugin;
          timer = NULL;
          }
       Delta = Next ? Next - Now : 0;

       char *tmp;
       if (Next && timer)
          tmp = strdup(*cString::sprintf("%ld %ld %d \"%s\" %d", Next, Delta, timer->Channel()->Number(), *strescape(timer->File(), "\\\"$"), false));
       else if (Next && Plugin)
          tmp = strdup(*cString::sprintf("%ld %ld %d \"%s\" %d", Next, Delta, 0, Plugin->Name(), false));
       else
          tmp = strdup(*cString::sprintf("%ld %ld %d \"%s\" %d", Next, Delta, 0, "", false));
       cString params = strescape(tmp, "\\\"");

       cString shutdowncmd;
       cString cmd = cString::sprintf("%s %s \"%s\"", *cDBusShutdown::_shutdownHooksWrapper, *cDBusShutdown::_shutdownHooksDir, *params);
       if (access(*cDBusShutdown::_shutdownHooksWrapper, X_OK) != 0)
          cmd = cString::sprintf("/bin/sh %s %s \"%s\"", *cDBusShutdown::_shutdownHooksWrapper, *cDBusShutdown::_shutdownHooksDir, *params);
       isyslog("dbus2vdr: calling shutdown-hook-wrapper %s", *cmd);
       tmp = NULL;
       cExitPipe p;
       int ret = -1;
       if (p.Open(*cmd, "r")) {
          int l = 0;
          int c;
          while ((c = fgetc(p)) != EOF) {
                if (l % 20 == 0) {
                   if (char *NewBuffer = (char *)realloc(tmp, l + 21))
                      tmp = NewBuffer;
                   else {
                      esyslog("dbus2vdr: out of memory");
                      break;
                      }
                   }
                tmp[l++] = char(c);
                }
          if (tmp)
             tmp[l] = 0;
          ret = p.Close();
          }
       else
          esyslog("dbus2vdr: can't open pipe for command '%s'", *cmd);

       cString result(stripspace(tmp), true); // for automatic free
       isyslog("dbus2vdr: result(%d) = %s", ret, *result);
       if (ret != 0) {
          if (*result) {
             static const char *message = "ABORT_MESSAGE=\"";
             if ((strlen(*result) > strlen(message)) && startswith(*result, message)) {
                cString abort_message = tmp + strlen(message);
                abort_message.Truncate(-1);
                SendReply(Invocation, 992, *abort_message, ret, "");
                return;
                }
             }
          SendReply(Invocation, 999, "shutdown-hook returned a non-zero exit code", ret, "");
          return;
          }

       if (*result) {
          static const char *message = "TRY_AGAIN=\"";
          if ((strlen(*result) > strlen(message)) && startswith(*result, message)) {
             cString s_try_again = tmp + strlen(message);
             s_try_again.Truncate(-1);
             if ((strlen(*s_try_again) > 0) && isnumber(*s_try_again)) {
                int try_again = strtol(s_try_again, NULL, 10);
                if (try_again > 0) {
                   SendReply(Invocation, 991, *s_try_again, 0, "");
                   return;
                   }
                }
             }
          }

       if (*result) {
          static const char *message = "SHUTDOWNCMD=\"";
          if ((strlen(*result) > strlen(message)) && startswith(*result, message)) {
             shutdowncmd = tmp + strlen(message);
             shutdowncmd.Truncate(-1);
             }
          }
       if (*shutdowncmd && (strlen(*shutdowncmd) > 0)) {
          SendReply(Invocation, 990, *shutdowncmd, 0, *params);
          return;
          }
       }

    SendReply(Invocation, 250, "vdr is ready for shutdown", 0, "");
  };

  static void ManualStart(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gboolean manual = FALSE;
    time_t Delta = Setup.NextWakeupTime ? Setup.NextWakeupTime - cDBusShutdown::StartupTime : 0;
    if (!Setup.NextWakeupTime || (abs(Delta) > 600)) // 600 comes from vdr's MANUALSTART constant in vdr.c
       manual = TRUE;
    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(b)", manual));
  };

  static void SetUserInactive(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    ShutdownHandler.SetUserInactive();
    cDBusHelper::SendReply(Invocation, 250, "vdr is set to non-interactive mode");
  };
};

const char *cDBusShutdownHelper::_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_SHUTDOWN_INTERFACE"\">\n"
    "    <method name=\"ConfirmShutdown\">\n"
    "      <arg name=\"ignoreuser\"   type=\"b\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"shexitcode\"   type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"shparameter\"  type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"ManualStart\">\n"
    "      <arg name=\"manual\"       type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"SetUserInactive\">\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";


time_t   cDBusShutdown::StartupTime;
cString  cDBusShutdown::_shutdownHooksDir;
cString  cDBusShutdown::_shutdownHooksWrapper;

void cDBusShutdown::SetShutdownHooksDir(const char *Dir)
{
  _shutdownHooksDir = cString( Dir);
}

void cDBusShutdown::SetShutdownHooksWrapper(const char *Wrapper)
{
  _shutdownHooksWrapper = cString( Wrapper);
}

cDBusShutdown::cDBusShutdown(void)
:cDBusObject("/Shutdown", cDBusShutdownHelper::_xmlNodeInfo)
{
  AddMethod("ConfirmShutdown" , cDBusShutdownHelper::ConfirmShutdown);
  AddMethod("ManualStart" , cDBusShutdownHelper::ManualStart);
  AddMethod("SetUserInactive" , cDBusShutdownHelper::SetUserInactive);
}

cDBusShutdown::~cDBusShutdown(void)
{
}
