#include "network.h"

#include <vdr/tools.h>


void  cDBusNetwork::on_connect(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: on_connect", net->Name());

  GError *err = NULL;
  net->_connection = g_dbus_connection_new_for_address_finish(res, &err);
  if ((net->_connection == NULL) || (err != NULL)) {
     esyslog("dbus2vdr: %s: can't get connection for address %s", net->Name(), net->_address);
     if (err != NULL) {
        esyslog("dbus2vdr: %s: error: %s", net->Name(), err->message);
        g_error_free(err);
        err = NULL;
        }
     g_object_unref(net->_auth_obs);
     net->_auth_obs = NULL;
     if (net->_connection != NULL) {
        g_object_unref(net->_connection);
        net->_connection = NULL;
        }
     return;
     }
  g_dbus_connection_set_exit_on_close(net->_connection, FALSE);
  net->_closed_handler = g_signal_connect(net->_connection, "closed", G_CALLBACK(on_closed), user_data);
  net->_owner_id = g_bus_own_name_on_connection(net->_connection, "de.tvdr.vdr", G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                                on_name_acquired, on_name_lost, user_data, NULL);
  net->_status = 1;
  isyslog("dbus2vdr: %s: connected", net->Name());
}

void  cDBusNetwork::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: on_name_acquired %s", net->Name(), name);
  
  net->_status = 2;
}

void  cDBusNetwork::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: on_name_lost %s", net->Name(), name);

  if (net->_closed_handler != 0) {
     g_signal_handler_disconnect(net->_connection, net->_closed_handler);
     net->_closed_handler = 0;
     }
  if (net->_connection != NULL) {
     g_object_unref(net->_connection);
     net->_connection = NULL;
     }
  if (net->_auth_obs != NULL) {
     g_object_unref(net->_auth_obs);
     net->_auth_obs = NULL;
     }
  net->_status = 0;
}

void  cDBusNetwork::on_closed(GDBusConnection *connection, gboolean remote_peer_vanished, GError *error, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: on_closed with error: %s", net->Name(), error ? error->message : "(null)");

  if (net->_closed_handler != 0) {
     g_signal_handler_disconnect(net->_connection, net->_closed_handler);
     net->_closed_handler = 0;
     }
  if (net->_connection != NULL) {
     g_object_unref(net->_connection);
     net->_connection = NULL;
     }
  if (net->_auth_obs != NULL) {
     g_object_unref(net->_auth_obs);
     net->_auth_obs = NULL;
     }
  net->_status = 0;
}

cDBusNetwork::cDBusNetwork(const char *Address, GMainContext *Context)
{
  _address = g_strdup(Address);
  _context = Context;
  _status = 0;
  _auth_obs = NULL;
  _connection = NULL;
  _owner_id = 0;
  _closed_handler = 0;
}

cDBusNetwork::~cDBusNetwork(void)
{
  Stop();

  if (_closed_handler != 0) {
     g_signal_handler_disconnect(_connection, _closed_handler);
     _closed_handler = 0;
     }
  if (_connection != NULL) {
     g_object_unref(_connection);
     _connection = NULL;
     }
  if (_auth_obs != NULL) {
     g_object_unref(_auth_obs);
     _auth_obs = NULL;
     }
  if (_address != NULL) {
     g_free(_address);
     _address = NULL;
     }
  dsyslog("dbus2vdr: %s: ~cDBusNetwork", Name());
}

bool  cDBusNetwork::Start(void)
{
  if (_status > 0)
     return true;
  isyslog("dbus2vdr: %s: starting", Name());
  _status = 0;
  bool ret = true;
  GError *err = NULL;
  if (!g_dbus_is_supported_address(_address, &err) || (err != NULL)) {
     esyslog("dbus2vdr: %s: address %s is not supported", Name(), _address);
     if (err != NULL) {
        esyslog("dbus2vdr: %s: error: %s", Name(), err->message);
        g_error_free(err);
        err = NULL;
        }
     return false;
     }
  err = NULL;
  if (_auth_obs != NULL) {
     g_object_unref(_auth_obs);
     _auth_obs = NULL;
     }
  _auth_obs = g_dbus_auth_observer_new();
  if (_closed_handler != 0) {
     g_signal_handler_disconnect(_connection, _closed_handler);
     _closed_handler = 0;
     }
  if (_connection != NULL) {
     g_object_unref(_connection);
     _connection = NULL;
     }
  g_dbus_connection_new_for_address(_address, G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL /*_auth_obs*/, NULL, on_connect, this);
  isyslog("dbus2vdr: %s: started", Name());
  return ret;
}

void  cDBusNetwork::Stop(void)
{
  if (_status == 0)
     return;
  isyslog("dbus2vdr: %s: stopping", Name());
  _status = 3;
  if (_owner_id != 0) {
     g_bus_unown_name(_owner_id);
     _owner_id = 0;
     }
  GError *err = NULL;
  if (_connection != NULL) {
     if (!g_dbus_connection_flush_sync(_connection, NULL, &err) || (err != NULL)) {
        esyslog("dbus2vdr: %s: can't flush connection for address %s", Name(), _address);
        if (err != NULL) {
           esyslog("dbus2vdr: %s: error: %s", Name(), err->message);
           g_error_free(err);
           err = NULL;
           }
        }
     if (!g_dbus_connection_close_sync(_connection, NULL, &err) || (err != NULL)) {
        esyslog("dbus2vdr: %s: can't close connection for address %s", Name(), _address);
        if (err != NULL) {
           esyslog("dbus2vdr: %s: error: %s", Name(), err->message);
           g_error_free(err);
           err = NULL;
           }
        }
     g_object_unref(_connection);
     _connection = NULL;
     }
  if (_auth_obs != NULL) {
     g_object_unref(_auth_obs);
     _auth_obs = NULL;
     }
  _status = 0;
  isyslog("dbus2vdr: %s: stopped", Name());
}
