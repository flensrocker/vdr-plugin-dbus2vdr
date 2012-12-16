#include "bus.h"
#include "helper.h"


cDBusBus::cDBusBus(const char *name, const char *busname)
 :_name(name)
 ,_busname(busname)
 ,_conn(NULL)
{
  dbus_error_init(&_err);
  if (!dbus_threads_init_default())
     esyslog("dbus2vdr: bus %s: dbus_threads_init_default returns an error - not good!", name);
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
        esyslog("dbus2vdr: bus %s: connection error: %s", *_name, _err.message);
        dbus_error_free(&_err);
        }
     return NULL;
     }

  Disconnect();

  dbus_connection_set_exit_on_disconnect(conn, false);
  const char *busname = *_busname;
  int ret = dbus_bus_request_name(conn, busname, DBUS_NAME_FLAG_REPLACE_EXISTING, &_err);
  if (dbus_error_is_set(&_err)) {
     esyslog("dbus2vdr: bus %s: name error: %s", *_name, _err.message);
     dbus_error_free(&_err);
     return NULL;
     }
  if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
     esyslog("dbus2vdr: bus %s: not primary owner for bus %s", *_name, busname);
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
 :cDBusBus("system", busname)
{
}

cDBusSystemBus::~cDBusSystemBus(void)
{
}

DBusConnection*  cDBusSystemBus::GetConnection(void)
{
  return dbus_bus_get(DBUS_BUS_SYSTEM, &_err);
}


cDBusNetworkBus::cDBusNetworkBus(const char *busname)
 :cDBusBus("network", busname)
 ,_address(NULL)
{
}

cDBusNetworkBus::~cDBusNetworkBus(void)
{
  if (_address != NULL)
     delete _address;
  _address = NULL;
}

DBusConnection*  cDBusNetworkBus::GetConnection(void)
{
  if (_address != NULL)
     delete _address;
  _address = cDBusHelper::GetNetworkAddress();
  if (_address == NULL)
     return NULL;
  isyslog("dbus2vdr: try to connect to network bus on address %s", _address->Address());
  DBusConnection *conn = dbus_connection_open(_address->Address(), &_err);
  if (conn != NULL) {
     if (!dbus_bus_register(conn, &_err)) {
        if (dbus_error_is_set(&_err)) {
           esyslog("dbus2vdr: bus %s: register error: %s", Name(), _err.message);
           dbus_error_free(&_err);
           }
        dbus_connection_unref(conn);
        conn = NULL;
        }
     }
  return conn;
}