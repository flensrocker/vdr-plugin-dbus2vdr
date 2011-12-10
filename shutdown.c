#include "shutdown.h"
#include "common.h"
#include "helper.h"

#include <vdr/shutdown.h>


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
  if (ShutdownHandler.IsUserInactive() && ShutdownHandler.ConfirmShutdown(false))
     cDBusHelper::SendReply(_conn, _msg, 250, "vdr is ready for shutdown");
  else
     cDBusHelper::SendReply(_conn, _msg, 550, "vdr is not ready for shutdown");
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
