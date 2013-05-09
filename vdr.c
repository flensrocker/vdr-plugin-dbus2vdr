#include "vdr.h"
#include "common.h"
#include "connection.h"
#include "helper.h"
#include "monitor.h"

#include <vdr/recording.h>


static const char *GetStatusName(cDBusVdr::eVdrStatus Status)
{
  switch (Status) {
   case cDBusVdr::statusStart:
     return "Start";
   case cDBusVdr::statusReady:
     return "Ready";
   case cDBusVdr::statusStop:
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

    const char *status = GetStatusName(cDBusVdr::GetStatus());
    cDBusHelper::AddArg(replyArgs, DBUS_TYPE_STRING, &status);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.Status: out of memory while sending the reply", DBUS_VDR_VDR_INTERFACE);
    dbus_message_unref(reply);
  }
};


void  cDBusDispatcherVdr::SendStatus(cDBusVdr::eVdrStatus Status)
{
  const char *signal = GetStatusName(Status);
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


namespace cDBusVdrHelper
{
  static const char *_xmlNodeInfo = 
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

  static void Status(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(s)", GetStatusName(cDBusVdr::GetStatus())));
  };
}

cDBusVdr::eVdrStatus  cDBusVdr::_status = cDBusVdr::statusUnknown;
cVector<cDBusVdr*>    cDBusVdr::_objects;
cMutex                cDBusVdr::_objectsMutex;

bool  cDBusVdr::SetStatus(cDBusVdr::eVdrStatus Status)
{
  if (_status == Status)
     return false;

  _status = Status;
  const char *signal = GetStatusName(_status);

  _objectsMutex.Lock();
  for (int i = 0; i < _objects.Size(); i++)
      _objects[i]->Connection()->EmitSignal(new cDBusConnection::cDBusSignal(NULL, "/vdr", DBUS_VDR_VDR_INTERFACE, signal, g_variant_new("(i)", InstanceId)));
  _objectsMutex.Unlock();
  return true;
}

cDBusVdr::eVdrStatus cDBusVdr::GetStatus(void)
{
  return _status;
}

cDBusVdr::cDBusVdr(void)
:cDBusObject("/vdr", cDBusVdrHelper::_xmlNodeInfo)
{
  AddMethod("Status", cDBusVdrHelper::Status);
  _objectsMutex.Lock();
  _objects.Append(this);
  _objectsMutex.Unlock();
}

cDBusVdr::~cDBusVdr(void)
{
  _objectsMutex.Lock();
  int i = 0;
  while (i < _objects.Size()) {
        if (_objects[i] == this) {
           _objects.Remove(i);
           break;
           }
        i++;
        }
  _objectsMutex.Unlock();
}
