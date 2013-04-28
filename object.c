#include "object.h"


const GDBusInterfaceVTable cDBusObject::_interface_vtable =
{
  cDBusObject::handle_method_call,
  NULL,
  NULL
};

void  cDBusObject::handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusObject *obj = (cDBusObject*)user_data;
  obj->HandleMethodCall(connection, sender, object_path, interface_name, method_name, parameters, invocation);
}

cDBusObject::cDBusObject(const char *Path)
{
  _path = g_strdup(Path);
  _registration_id = 0;
  _connection = NULL;

  GError *err = NULL;
  _introspection_data = g_dbus_node_info_new_for_xml(XmlNodeInfo(), &err);
  if (err != NULL) {
     esyslog("dbus2vdr: g_dbus_node_info_new_for_xml reports: %s", err->message);
     g_error_free(err);
     }
}

cDBusObject::~cDBusObject(void)
{
  if (_introspection_data != NULL) {
     g_dbus_node_info_unref(_introspection_data);
     _introspection_data = NULL;
     }

  if (_path != NULL) {
     g_free(_path);
     _path = NULL;
     }
}

void  cDBusObject::Register(void)
{
  if ((_connection == NULL) || (_connection->GetConnection() == NULL))
     return;

  if (_registration_id != 0)
     Unregister();
  _registration_id = g_dbus_connection_register_object(_connection->GetConnection(), Path(), _introspection_data->interfaces[0], &_interface_vtable, this, NULL, NULL);
}

void  cDBusObject::Unregister(void)
{
  if (_registration_id != 0) {
     if ((_connection != NULL) && (_connection->GetConnection() != NULL))
        g_dbus_connection_unregister_object(_connection->GetConnection(), _registration_id);
     _registration_id = 0;
     }
}

const gchar  *cDBusObject::XmlNodeInfo(void) const
{
  return "";
}

void  cDBusObject::HandleMethodCall(GDBusConnection       *connection,
                                    const gchar           *sender,
                                    const gchar           *object_path,
                                    const gchar           *interface_name,
                                    const gchar           *method_name,
                                    GVariant              *parameters,
                                    GDBusMethodInvocation *invocation)
{
  gchar *message = g_strdup_printf("method '%s.%s' on object '%s' not implemented yet", interface_name, method_name, object_path);
  g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED, message);
  g_free(message);
}


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
  StopMessageHandler();

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

bool  cDBusConnection::StartMessageHandler(void)
{
  return Start();
}

void  cDBusConnection::StopMessageHandler(void)
{
  Cancel(-1);
  Disconnect();
  Cancel(5);
}

void  cDBusConnection::Action(void)
{
  _context = g_main_context_new();
  g_main_context_push_thread_default(_context);

  _loop = g_main_loop_new(_context, FALSE);
  Connect();

  g_main_loop_run(_loop);

  g_main_context_pop_thread_default(_context);
  g_main_context_unref(_context);
  _context = NULL;
}

void  cDBusConnection::Connect(void)
{
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
}

void  cDBusConnection::RegisterObjects(void)
{
  if (_connection == NULL)
     return;

  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Register();
}

void  cDBusConnection::UnregisterObjects(void)
{
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Unregister();
}

void  cDBusConnection::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  //cDBusConnection *conn = (cDBusConnection*)user_data;
}

void  cDBusConnection::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

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

  cDBusConnection *conn = (cDBusConnection*)user_data;
  conn->_connection = g_bus_get_finish(res, NULL);
  if (conn->_connection != NULL) {
     conn->_connect_status = 3;
     g_dbus_connection_set_exit_on_close(conn->_connection, FALSE);
     conn->RegisterObjects();
     conn->_owner_id = g_bus_own_name_on_connection(conn->_connection,
                                                    conn->_busname,
                                                    G_BUS_NAME_OWNER_FLAGS_NONE,
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

  cDBusConnection *conn = (cDBusConnection*)user_data;
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

  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (conn->_connection != NULL)
     g_dbus_connection_flush(conn->_connection, NULL, on_flush, user_data);

  return FALSE;
}
