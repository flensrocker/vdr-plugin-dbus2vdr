#include "shutdown.h"
#include "common.h"
#include "helper.h"

#include <vdr/cutter.h>
#include <vdr/menu.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/shutdown.h>
#include <vdr/timers.h>


cDBusMessageShutdown::cDBusMessageShutdown(cDBusMessageShutdown::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageShutdown::~cDBusMessageShutdown(void)
{
}

void cDBusMessageShutdown::Process(void)
{
  switch (_action) {
    case dmsConfirmShutdown:
      ConfirmShutdown();
      break;
    }
}

void cDBusMessageShutdown::ConfirmShutdown(void)
{
  // this is nearly a copy of vdr's cShutdownHandler::ConfirmShutdown
  // if the original changes take this into account

  if (!ShutdownHandler.IsUserInactive()) {
     cDBusHelper::SendReply(_conn, _msg, 901, "user is active");
     return;
     }

  if (cCutter::Active()) {
     cDBusHelper::SendReply(_conn, _msg, 902, "cutter is active");
     return;
     }

  cTimer *timer = Timers.GetNextActiveTimer();
  time_t Next = timer ? timer->StartTime() : 0;
  time_t Delta = timer ? Next - time(NULL) : 0;
  if (cRecordControls::Active() || (Next && Delta <= 0)) {
     // VPS recordings in timer end margin may cause Delta <= 0
     cDBusHelper::SendReply(_conn, _msg, 903, "recording is active");
     return;
     }
  else if (Next && Delta <= Setup.MinEventTimeout * 60) {
     // Timer within Min Event Timeout
     cDBusHelper::SendReply(_conn, _msg, 904, "recording is active in the near future");
     return;
     }

  if (cPluginManager::Active(NULL)) {
     cDBusHelper::SendReply(_conn, _msg, 905, "some plugin is active");
     return;
     }

  cPlugin *Plugin = cPluginManager::GetNextWakeupPlugin();
  Next = Plugin ? Plugin->WakeupTime() : 0;
  Delta = Next ? Next - time(NULL) : 0;
  if (Next && Delta <= Setup.MinEventTimeout * 60) {
     // Plugin wakeup within Min Event Timeout
     cString buf = cString::sprintf("plugin %s wakes up in %ld min", Plugin->Name(), Delta / 60);
     cDBusHelper::SendReply(_conn, _msg, 906, *buf);
     return;
     }

  // insanity check: ask vdr again, if implementation of ConfirmShutdown has changed...
  if (cRemote::Enabled() && !ShutdownHandler.ConfirmShutdown(false)) {
     cDBusHelper::SendReply(_conn, _msg, 550, "vdr is not ready for shutdown");
     return;
     }

  cDBusHelper::SendReply(_conn, _msg, 250, "vdr is ready for shutdown");
}


cDBusDispatcherShutdown::cDBusDispatcherShutdown(void)
:cDBusMessageDispatcher(DBUS_VDR_SHUTDOWN_INTERFACE)
{
}

cDBusDispatcherShutdown::~cDBusDispatcherShutdown(void)
{
}

cDBusMessage *cDBusDispatcherShutdown::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Shutdown") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_SHUTDOWN_INTERFACE, "ConfirmShutdown"))
     return new cDBusMessageShutdown(cDBusMessageShutdown::dmsConfirmShutdown, conn, msg);

  return NULL;
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
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
