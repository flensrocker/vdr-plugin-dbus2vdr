#include "monitor.h"
#include "common.h"
#include "helper.h"
#include "plugin.h"

#include <vdr/tools.h>


cMutex cDBusMonitor::_mutex;
cDBusMonitor *cDBusMonitor::_monitor = NULL;


cDBusMonitor::cDBusMonitor(void)
{
  _conn = NULL;
}

cDBusMonitor::~cDBusMonitor(void)
{
  if (_conn != NULL)
     dbus_connection_unref(_conn);
  _conn = NULL;
}

void cDBusMonitor::StartMonitor(void)
{
  cMutexLock lock(&_mutex);
  if (_monitor != NULL)
     return;
  _monitor = new cDBusMonitor;
  if (_monitor)
     _monitor->Start();
}

void cDBusMonitor::StopMonitor(void)
{
  cMutexLock lock(&_mutex);
  if (_monitor == NULL)
     return;
  _monitor->Cancel(5);
  delete _monitor;
  _monitor = NULL;
}

bool cDBusMonitor::SendSignal(DBusMessage *msg)
{
  cMutexLock lock(&_mutex);
  if ((_monitor == NULL) || (_monitor->_conn == NULL))
     return false;

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_monitor->_conn, msg, &serial)) { 
     esyslog("dbus2vdr: out of memory while sending signal"); 
     return false;
     }
  dbus_connection_flush(_monitor->_conn);
  dbus_message_unref(msg);
  return true;
}

void cDBusMonitor::Action(void)
{
  DBusError err;
  dbus_error_init(&err);
  _conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
     esyslog("dbus2vdr: connection error: %s", err.message);
     dbus_error_free(&err);
     }
  if (_conn == NULL)
     return;

  int ret = dbus_bus_request_name(_conn, DBUS_VDR_BUSNAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (dbus_error_is_set(&err)) {
     esyslog("dbus2vdr: name error: %s", err.message);
     dbus_error_free(&err);
     }
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
     esyslog("dbus2vdr: not primary owner for bus %s", DBUS_VDR_BUSNAME);
     return;
     }

  isyslog("dbus2vdr: monitor started on bus %s", DBUS_VDR_BUSNAME);
  while (true) {
        dbus_connection_read_write(_conn, 1000);
        if (!Running())
           break;
        DBusMessage *msg = dbus_connection_pop_message(_conn);
        if (msg == NULL)
           continue;
        const char *object = dbus_message_get_path(msg);
        const char *interface = dbus_message_get_interface(msg);
        const char *member = dbus_message_get_member(msg);
        if ((object == NULL) || (interface == NULL) || (member == NULL)) {
           dbus_message_unref(msg);
           continue;
           }
        
        isyslog("dbus2vdr: new message, object %s, interface %s, member %s", object, interface, member);
        if ((strcmp(interface, "org.freedesktop.DBus") == 0)
         && (strcmp(object, "/org/freedesktop/DBus") == 0)
         && (strcmp(member, "NameAcquired") == 0)) {
          const char *name = NULL;
          DBusMessageIter args;
          if (!dbus_message_iter_init(msg, &args))
             esyslog("dbus2vdr: NameAcquired: message misses an argument for the name");
          else {
             if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name) < 0)
                esyslog("dbus2vdr: NameAcquired: 'name' argument is not a string");
             }
           if (name != NULL)
              isyslog("dbus2vdr: NameAcquired: get ownership of name %s", name);
           cDBusHelper::SendReply(_conn, msg, "");
           dbus_message_unref(msg);
           continue;
           }

        if (strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
           isyslog("dbus2vdr: introspect object %s with %s", object, member);
           cString data( "");
           if (!cDBusMessageDispatcher::Introspect(msg, data))
              esyslog("dbus2vdr: can't introspect object %s", object);
           cDBusHelper::SendReply(_conn, msg, *data);
           dbus_message_unref(msg);
           continue;
           }

        if (!cDBusMessageDispatcher::Dispatch(_conn, msg)) {
           isyslog("dbus2vdr: don't know what to do...");
           cDBusHelper::SendReply(_conn, msg, -1, "unknown message");
           dbus_message_unref(msg);
           }
        }
  cDBusMessageDispatcher::Stop();
  isyslog("dbus2vdr: monitor stopped on bus %s", DBUS_VDR_BUSNAME);
}
