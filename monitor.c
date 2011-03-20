#include "monitor.h"
#include "common.h"
#include "plugin.h"

#include <vdr/tools.h>

#include <dbus/dbus.h>


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
  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
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
        isyslog("dbus2vdr: new message, object %s, interface %s, member %s", dbus_message_get_path(msg), dbus_message_get_interface(msg), dbus_message_get_member(msg));
        if (dbus_message_is_method_call(msg, DBUS_VDR_PLUGIN_INTERFACE, "SVDRPCommand"))
           cDBus2vdrPlugin::SVDRPCommand(msg, conn);
        else if (dbus_message_is_method_call(msg, DBUS_VDR_PLUGIN_INTERFACE, "Service"))
           cDBus2vdrPlugin::Service(msg, conn);
        dbus_message_unref(msg);
        }

  dbus_connection_close(conn);
}
