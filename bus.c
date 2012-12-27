#include "bus.h"
#include "common.h"
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
  OnConnect();
  return _conn;
}

bool  cDBusBus::Disconnect(void)
{
  if (_conn == NULL)
     return false;
  dbus_connection_unref(_conn);
  _conn = NULL;
  return true;
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
 ,_avahi4vdr(NULL)
{
  _avahi4vdr = cPluginManager::GetPlugin("avahi4vdr");
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
  _avahi_name = strescape(*cString::sprintf("dbus2vdr on %s", *_address->Host), "\\,");
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

void cDBusNetworkBus::OnConnect(void)
{
  if ((_address != NULL) && (_avahi4vdr != NULL)) {
     int replyCode = 0;
     cString parameter = cString::sprintf("caller=dbus2vdr,name=%s,type=_dbus._tcp,port=%d,subtype=_vdr_dbus2vdr._sub._dbus._tcp", *_avahi_name, _address->Port);
     _avahi_id = _avahi4vdr->SVDRPCommand("CreateService", *parameter, replyCode);
     dsyslog("dbus2vdr: network bus created avahi service (id %s)", *_avahi_id);
     }
}

bool cDBusNetworkBus::Disconnect(void)
{
  if ((_avahi4vdr != NULL) && (*_avahi_id != NULL)) {
     int replyCode = 0;
     _avahi4vdr->SVDRPCommand("DeleteService", *_avahi_id, replyCode);
     dsyslog("dbus2vdr: network bus deleted avahi service (id %s)", *_avahi_id);
     _avahi_id = NULL;
     }
  return cDBusBus::Disconnect();
}
