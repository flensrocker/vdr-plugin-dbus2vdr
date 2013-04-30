#include "connection.h"
#include "object.h"


cDBusConnection::cDBusConnection(const char *Busname, GBusType  Type)
{
  Init(Busname);
  _bus_type = Type;
}

cDBusConnection::cDBusConnection(const char *Busname, const char *Address)
{
  Init(Busname);
  _bus_address = g_strdup(Address);
}

cDBusConnection::~cDBusConnection(void)
{
  Cancel(-1);
  Disconnect();
  Cancel(5);

  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }

  if (_bus_address != NULL) {
     g_free(_bus_address);
     _bus_address = NULL;
     }
}

void  cDBusConnection::Init(const char *Busname)
{
  _busname = g_strdup(Busname);
  _bus_type = G_BUS_TYPE_NONE;
  _bus_address = NULL;
  _context = NULL;
  _loop = NULL;
  _connection = NULL;
  _owner_id = 0;
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;
}

void  cDBusConnection::AddObject(cDBusObject *Object)
{
  if (Object != NULL) {
     Object->SetConnection(this);
     _objects.Add(Object);
     }
}

void  cDBusConnection::Action(void)
{
  _context = g_main_context_new();
  g_main_context_push_thread_default(_context);

  _loop = g_main_loop_new(_context, FALSE);
  Connect();

  g_main_loop_run(_loop);

  g_main_context_pop_thread_default(_context);

  g_main_loop_unref(_loop);
  _loop = NULL;

  g_main_context_unref(_context);
  _context = NULL;
}

void  cDBusConnection::Connect(void)
{
  dsyslog("dbus2vdr: Connect");
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source,
                        do_connect,
                        this,
                        NULL);
  g_source_attach(source, _context);
}

void  cDBusConnection::Disconnect(void)
{
  dsyslog("dbus2vdr: Disconnect");
  _reconnect = FALSE;
  _disconnect_status = 1;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  /* on_flush will call do_disconnect */
  if (_connect_status >= 3)
     g_source_set_callback(source, do_flush, this, NULL);
  else
     g_source_set_callback(source, do_disconnect, this, NULL);
  g_source_attach(source, _context);
}

void  cDBusConnection::RegisterObjects(void)
{
  if (_connection == NULL)
     return;

  dsyslog("dbus2vdr: RegisterObjects");
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Register();
}

void  cDBusConnection::UnregisterObjects(void)
{
  dsyslog("dbus2vdr: UnregisterObjects");
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Unregister();
}

void  cDBusConnection::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  dsyslog("dbus2vdr: on_name_acquired");
  //cDBusConnection *conn = (cDBusConnection*)user_data;
}

void  cDBusConnection::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  dsyslog("dbus2vdr: on_name_lost");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  conn->UnregisterObjects();

  g_bus_unown_name(conn->_owner_id);
  conn->_owner_id = 0;

  g_object_unref(conn->_connection);
  conn->_connection = NULL;
  if (conn->_connect_status == 4)
     conn->_connect_status = 0;
  else
     conn->_connect_status = 1;

  if (conn->_reconnect)
     conn->Connect();
}

void  cDBusConnection::on_bus_get(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  dsyslog("dbus2vdr: on_bus_get");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  conn->_connection = g_bus_get_finish(res, NULL);
  if (conn->_connection != NULL) {
     conn->_connect_status = 3;
     g_dbus_connection_set_exit_on_close(conn->_connection, FALSE);
     conn->RegisterObjects();
     conn->_owner_id = g_bus_own_name_on_connection(conn->_connection,
                                                    conn->_busname,
                                                    G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                                    on_name_acquired,
                                                    on_name_lost,
                                                    user_data,
                                                    NULL);
     }
  else
     conn->_connect_status = 1;
}

gboolean  cDBusConnection::do_reconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  dsyslog("dbus2vdr: do_reconnect");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (!conn->_reconnect)
     return FALSE;

  if (conn->_connect_status == 3) {
     conn->_connect_status = 4;
     return FALSE;
     }

  if (conn->_connect_status == 1) {
     conn->_connect_status = 2;
     if (conn->_bus_type != G_BUS_TYPE_NONE) {
        g_bus_get(conn->_bus_type, NULL, on_bus_get, user_data);
        return TRUE;
        }
     if (conn->_bus_address != NULL) {
        g_dbus_connection_new_for_address(conn->_bus_address,
                                          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                          NULL,
                                          NULL,
                                          on_bus_get,
                                          user_data);
        return TRUE;
        }
     esyslog("dbus2vdr: can't connect bus without address");
     return FALSE;
     }
  else if (conn->_connect_status == 2)
     return TRUE;

  return FALSE;
}

gboolean  cDBusConnection::do_connect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  dsyslog("dbus2vdr: do_connect");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (conn->_connect_status == 0) {
     conn->_connect_status = 2;
     if (conn->_bus_type != G_BUS_TYPE_NONE)
        g_bus_get(conn->_bus_type, NULL, on_bus_get, user_data);
     else if (conn->_bus_address != NULL)
        g_dbus_connection_new_for_address(conn->_bus_address, G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, on_bus_get, user_data);
     else {
        esyslog("dbus2vdr: can't connect bus without address");
        return FALSE;
        }

     GSource *source = g_timeout_source_new(500);
     g_source_set_callback(source, do_reconnect, user_data, NULL);
     g_source_attach(source, conn->_context);
     }

  return FALSE;
}

gboolean  cDBusConnection::do_disconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  dsyslog("dbus2vdr: do_disconnect");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (conn->_connection != NULL)
     conn->UnregisterObjects();

  if (conn->_owner_id > 0) {
     g_bus_unown_name(conn->_owner_id);
     conn->_owner_id = 0;
     }

  if (conn->_connection != NULL) {
     g_object_unref(conn->_connection);
     conn->_connection = NULL;
     conn->_connect_status = 0;
     }

  g_main_loop_quit(conn->_loop);
  return FALSE;
}

void  cDBusConnection::on_flush(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  dsyslog("dbus2vdr: on_flush");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  g_dbus_connection_flush_finish(conn->_connection, res, NULL);
  if (conn->_disconnect_status == 0)
     return;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source,
                        do_disconnect,
                        user_data,
                        NULL);
  g_source_attach(source, conn->_context);
}

gboolean  cDBusConnection::do_flush(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  dsyslog("dbus2vdr: do_flush");
  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (conn->_connection != NULL)
     g_dbus_connection_flush(conn->_connection, NULL, on_flush, user_data);

  return FALSE;
}
