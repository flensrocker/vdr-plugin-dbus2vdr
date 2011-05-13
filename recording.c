#include "recording.h"
#include "common.h"
#include "helper.h"

#include <vdr/recording.h>


cDBusMessageRecording::cDBusMessageRecording(cDBusMessageRecording::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageRecording::~cDBusMessageRecording(void)
{
}

void cDBusMessageRecording::Process(void)
{
  switch (_action) {
    case dmrUpdate:
      Update();
      break;
    }
}

void cDBusMessageRecording::Update(void)
{
  Recordings.Update(false);
  cDBusHelper::SendReply(_conn, _msg, 250, "update of recordings triggered");
}


cDBusDispatcherRecording::cDBusDispatcherRecording(void)
:cDBusMessageDispatcher(DBUS_VDR_RECORDING_INTERFACE)
{
}

cDBusDispatcherRecording::~cDBusDispatcherRecording(void)
{
}

cDBusMessage *cDBusDispatcherRecording::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Recording") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_RECORDING_INTERFACE, "Update"))
     return new cDBusMessageRecording(cDBusMessageRecording::dmrUpdate, conn, msg);

  return NULL;
}

bool          cDBusDispatcherRecording::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Recording") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_RECORDING_INTERFACE"\">\n"
  "    <method name=\"Update\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
