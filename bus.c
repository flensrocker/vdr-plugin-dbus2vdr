#include "bus.h"


cDBusBus::cDBusBus(const char *busname)
 :_busname(busname)
 ,_conn(NULL)
{
  dbus_error_init(&_err);
  if (!dbus_threads_init_default())
     esyslog("dbus2vdr: dbus_threads_init_default returns an error - not good!");
}

cDBusBus::~cDBusBus(void)
{
  Disconnect();
}

DBusConnection*  cDBusBus::Connect(void)
{
  DBusConnection *conn = GetConnection();
  if ((conn == NULL) || dbus_error_is_set(&_err)) {
     if (dbus_error_is_set(&_err)) {
        esyslog("dbus2vdr: connection error: %s", _err.message);
        dbus_error_free(&_err);
        }
     return NULL;
     }

  Disconnect();

  dbus_connection_set_exit_on_disconnect(conn, false);
  const char *busname = *_busname;
  int ret = dbus_bus_request_name(conn, busname, DBUS_NAME_FLAG_REPLACE_EXISTING, &_err);
  if (dbus_error_is_set(&_err)) {
     esyslog("dbus2vdr: name error: %s", _err.message);
     dbus_error_free(&_err);
     return NULL;
     }
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
     esyslog("dbus2vdr: not primary owner for bus %s", busname);
     return NULL;
     }

  _conn = conn;
  return _conn;
}

void  cDBusBus::Disconnect(void)
{
  if (_conn != NULL)
     dbus_connection_unref(_conn);
  _conn = NULL;
}


cDBusSystemBus::cDBusSystemBus(const char *busname)
 :cDBusBus(busname)
{
}

cDBusSystemBus::~cDBusSystemBus(void)
{
}

DBusConnection*  cDBusSystemBus::GetConnection(void)
{
  return dbus_bus_get(DBUS_BUS_SYSTEM, &_err);
}


cDBusCustomBus::cDBusCustomBus(const char *busname, const char *address)
 :cDBusBus(busname)
 ,_address(address)
{
}

cDBusCustomBus::~cDBusCustomBus(void)
{
}

DBusConnection*  cDBusCustomBus::GetConnection(void)
{
  return dbus_connection_open(*_address, &_err);
}
