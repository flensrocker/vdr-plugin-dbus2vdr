#include "vdr.h"
#include "common.h"
#include "connection.h"
#include "helper.h"
#include "monitor.h"

#include <vdr/recording.h>


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
  };

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
  const char *signal = cDBusVdrHelper::GetStatusName(_status);

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
