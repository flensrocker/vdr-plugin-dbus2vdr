#include "vdr.h"
#include "common.h"
#include "helper.h"
#include "monitor.h"

#include <vdr/recording.h>


static const char *GetStatusName(cDBusDispatcherVdr::eVdrStatus Status)
{
  switch (Status) {
   case cDBusDispatcherVdr::statusStart:
     return "Start";
   case cDBusDispatcherVdr::statusReady:
     return "Ready";
   case cDBusDispatcherVdr::statusStop:
     return "Stop";
   default:
     return "Unknown";
    }
  return "Unknown";
}

class cDBusVdrActions
{
public:
  static void Status(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);

    const char *status = GetStatusName(cDBusDispatcherVdr::GetStatus());
    cDBusHelper::AddArg(replyArgs, DBUS_TYPE_STRING, &status);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.Status: out of memory while sending the reply", DBUS_VDR_VDR_INTERFACE);
    dbus_message_unref(reply);
  }
};


cDBusDispatcherVdr::eVdrStatus cDBusDispatcherVdr::_status = cDBusDispatcherVdr::statusUnknown;

cDBusDispatcherVdr::eVdrStatus cDBusDispatcherVdr::GetStatus(void)
{
  return _status;
}

void  cDBusDispatcherVdr::SetStatus(cDBusDispatcherVdr::eVdrStatus Status)
{
  if (_status == Status)
     return;

  _status = Status;

  const char *signal = GetStatusName(_status);
  DBusMessage *msg = dbus_message_new_signal("/vdr", DBUS_VDR_VDR_INTERFACE, signal);
  if (msg == NULL) {
     esyslog("dbus2vdr: can't create %s signal", signal);
     return;
     }

  DBusMessageIter args;
  dbus_message_iter_init_append(msg, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &InstanceId)) {
     esyslog("dbus2vdr: can't append instance-id %d", InstanceId);
     dbus_message_unref(msg);
     return;
     }

  if (!cDBusMonitor::SendSignal(msg, busSystem))
     dbus_message_unref(msg);
}

cDBusDispatcherVdr::cDBusDispatcherVdr(void)
:cDBusMessageDispatcher(busSystem, DBUS_VDR_VDR_INTERFACE)
{
  AddPath("/vdr");
  AddAction("Status", cDBusVdrActions::Status);
}

cDBusDispatcherVdr::~cDBusDispatcherVdr(void)
{
}

bool          cDBusDispatcherVdr::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/vdr") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_VDR_INTERFACE"\">\n"
  "    <method name=\"Status\">\n"
  "      <arg name=\"status\"       type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <signal name=\"Start\">\n"
  "      <arg name=\"instanceid\"  type=\"i\"/>\n"
  "    </signal>\n"
  "    <signal name=\"Ready\">\n"
  "      <arg name=\"instanceid\"  type=\"i\"/>\n"
  "    </signal>\n"
  "    <signal name=\"Stop\">\n"
  "      <arg name=\"instanceid\"  type=\"i\"/>\n"
  "    </signal>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
