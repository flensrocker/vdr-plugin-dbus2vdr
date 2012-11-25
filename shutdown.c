#include "shutdown.h"
#include "common.h"
#include "helper.h"

#include <vdr/cutter.h>
#include <vdr/menu.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/shutdown.h>
#include <vdr/timers.h>


cString  cDBusShutdownActions::_shutdownHooksDir;
cString  cDBusShutdownActions::_shutdownHooksWrapper;

void cDBusShutdownActions::SetShutdownHooksDir(const char *Dir)
{
  _shutdownHooksDir = cString( Dir);
}

void cDBusShutdownActions::SetShutdownHooksWrapper(const char *Wrapper)
{
  _shutdownHooksWrapper = cString( Wrapper);
}

static void SendReply(DBusConnection *conn, DBusMessage *msg, int  returncode, const char *message, int  shexitcode = 0, const char *shparameter = "")
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &returncode))
     esyslog("dbus2vdr: SendReply: out of memory while appending the return-code");

  if (message == NULL)
     message = "";
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
     esyslog("dbus2vdr: SendReply: out of memory while appending the reply-message");

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &shexitcode))
     esyslog("dbus2vdr: SendReply: out of memory while appending the sh-exitcode");

  if (shparameter == NULL)
     shparameter = "";
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &shparameter))
     esyslog("dbus2vdr: SendReply: out of memory while appending the sh-parameter");

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
}

void cDBusShutdownActions::ConfirmShutdown(DBusConnection* conn, DBusMessage* msg)
{
  int ignoreuser = 0;
  if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_BOOLEAN, &ignoreuser, DBUS_TYPE_INVALID))
     ignoreuser = 0;

  // this is nearly a copy of vdr's cShutdownHandler::ConfirmShutdown
  // if the original changes take this into account

  if ((ignoreuser == 0) && !ShutdownHandler.IsUserInactive()) {
     SendReply(conn, msg, 901, "user is active");
     return;
     }

  if (cCutter::Active()) {
     SendReply(conn, msg, 902, "cutter is active");
     return;
     }

  time_t Now = time(NULL);
  cTimer *timer = Timers.GetNextActiveTimer();
  time_t Next = timer ? timer->StartTime() : 0;
  time_t Delta = timer ? Next - Now : 0;
  if (cRecordControls::Active() || (Next && Delta <= 0)) {
     // VPS recordings in timer end margin may cause Delta <= 0
     SendReply(conn, msg, 903, "recording is active");
     return;
     }
  else if (Next && Delta <= Setup.MinEventTimeout * 60) {
     // Timer within Min Event Timeout
     SendReply(conn, msg, 904, "recording is active in the near future");
     return;
     }

  if (cPluginManager::Active(NULL)) {
     SendReply(conn, msg, 905, "some plugin is active");
     return;
     }

  cPlugin *Plugin = cPluginManager::GetNextWakeupPlugin();
  time_t NextPlugin = Plugin ? Plugin->WakeupTime() : 0;
  Delta = NextPlugin ? NextPlugin - Now : 0;
  if (NextPlugin && Delta <= Setup.MinEventTimeout * 60) {
     // Plugin wakeup within Min Event Timeout
     cString buf = cString::sprintf("plugin %s wakes up in %ld min", Plugin->Name(), Delta / 60);
     SendReply(conn, msg, 906, *buf);
     return;
     }

  // insanity check: ask vdr again, if implementation of ConfirmShutdown has changed...
  if (cRemote::Enabled() && !ShutdownHandler.ConfirmShutdown(false)) {
     SendReply(conn, msg, 550, "vdr is not ready for shutdown");
     return;
     }

  if (((*_shutdownHooksDir) != NULL) && (strlen(*_shutdownHooksDir) > 0)
   && ((*_shutdownHooksWrapper) != NULL) && (strlen(*_shutdownHooksWrapper) > 0)) {
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
     cString cmd = cString::sprintf("%s %s \"%s\"", *_shutdownHooksWrapper, *_shutdownHooksDir, *params);
     if (access(*_shutdownHooksWrapper, X_OK) != 0)
        cmd = cString::sprintf("/bin/sh %s %s \"%s\"", *_shutdownHooksWrapper, *_shutdownHooksDir, *params);
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
              SendReply(conn, msg, 992, *abort_message, ret);
              return;
              }
           }
        SendReply(conn, msg, 999, "shutdown-hook returned a non-zero exit code", ret);
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
                 SendReply(conn, msg, 991, *s_try_again);
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
        SendReply(conn, msg, 990, *shutdowncmd, 0, *params);
        return;
        }
     }

  SendReply(conn, msg, 250, "vdr is ready for shutdown");
}

void cDBusShutdownActions::ManualStart(DBusConnection* conn, DBusMessage* msg)
{
  int manual = 0;
  time_t Delta = Setup.NextWakeupTime ? Setup.NextWakeupTime - cDBusDispatcherShutdown::StartupTime : 0;
  if (!Setup.NextWakeupTime || (abs(Delta) > 600)) // 600 comes from vdr's MANUALSTART constant in vdr.c
     manual = 1;

  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &manual))
     esyslog("dbus2vdr: %s.ManualStart: out of memory while appending the return value", DBUS_VDR_SHUTDOWN_INTERFACE);
  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: %s.ManualStart: out of memory while sending the reply", DBUS_VDR_SHUTDOWN_INTERFACE);
  dbus_message_unref(reply);
}

void cDBusShutdownActions::SetUserInactive(DBusConnection* conn, DBusMessage* msg)
{
  ShutdownHandler.SetUserInactive();
  SendReply(conn, msg, 250, "vdr is set to non-interactive mode");
}


time_t cDBusDispatcherShutdown::StartupTime;

cDBusDispatcherShutdown::cDBusDispatcherShutdown(void)
:cDBusMessageDispatcher(DBUS_VDR_SHUTDOWN_INTERFACE)
{
  AddPath("/Shutdown");
  AddAction("ConfirmShutdown" , cDBusShutdownActions::ConfirmShutdown);
  AddAction("ManualStart" , cDBusShutdownActions::ManualStart);
  AddAction("SetUserInactive" , cDBusShutdownActions::SetUserInactive);
}

cDBusDispatcherShutdown::~cDBusDispatcherShutdown(void)
{
}

bool          cDBusDispatcherShutdown::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Shutdown") != 0)
     return false;
  Data =
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
  return true;
}
