#include "monitor.h"
#include "common.h"
#include "helper.h"
#include "plugin.h"

#include <dbus/dbus.h>

#include <vdr/tools.h>


cDBusMonitor *cDBusMonitor::_monitor = NULL;


cDBusMonitor::cDBusMonitor(void)
{
}

cDBusMonitor::~cDBusMonitor(void)
{
}

void cDBusMonitor::StartMonitor(void)
{
  if (_monitor != NULL)
     return;
  _monitor = new cDBusMonitor;
  if (_monitor)
     _monitor->Start();
}

void cDBusMonitor::StopMonitor(void)
{
  if (_monitor == NULL)
     return;
  _monitor->Cancel(5);
  delete _monitor;
  _monitor = NULL;
}

void cDBusMonitor::Action(void)
{
  DBusError err;
  dbus_error_init(&err);
  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
     esyslog("dbus2vdr: connection error: %s", err.message);
     dbus_error_free(&err);
     }
  if (conn == NULL)
     return;

  int ret = dbus_bus_request_name(conn, DBUS_VDR_BUSNAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
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
        dbus_connection_read_write(conn, 1000);
        if (!Running())
           break;
        DBusMessage *msg = dbus_connection_pop_message(conn);
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
        if (strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
           isyslog("dbus2vdr: introspect object %s with %s", dbus_message_get_path(msg), dbus_message_get_member(msg));
           cString data( "");
           if (!cDBusMessageDispatcher::Introspect(msg, data))
              esyslog("dbus2vdr: can't introspect object %s", dbus_message_get_path(msg));
           cDBusHelper::SendReply(conn, msg, *data);
           dbus_message_unref(msg);
           continue;
           }

        if (!cDBusMessageDispatcher::Dispatch(conn, msg)) {
           isyslog("dbus2vdr: don't know what to do...");
           cDBusHelper::SendReply(conn, msg, -1, "unknown message");
           dbus_message_unref(msg);
           }
        }
  dbus_connection_unref(conn);
}
