#include "object.h"

#include <vdr/tools.h>


const GDBusInterfaceVTable cDBusObject::_interface_vtable =
{
  cDBusObject::handle_method_call,
  NULL,
  NULL
};

cDBusObject::cDBusObject(const char *Busname, const char *Path)
{
  g_type_init();

  _busname = g_strdup(Busname);
  _path = g_strdup(Path);
  _context = NULL;
  _loop = NULL;
  _introspection_data = NULL;
  _connection = NULL;
  _owner_id = 0;
  _registration_id = 0;
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;
}

cDBusObject::~cDBusObject(void)
{
  StopMessageHandler();

  if (_introspection_data != NULL) {
     g_dbus_node_info_unref(_introspection_data);
     _introspection_data = NULL;
     }

  if (_path != NULL) {
     g_free(_path);
     _path = NULL;
     }

  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }
}

bool  cDBusObject::StartMessageHandler(void)
{
  if (_introspection_data == NULL)
     return false;
  return Start();
}

void  cDBusObject::StopMessageHandler(void)
{
  Cancel(-1);
  Disconnect();
  Cancel(5);
}

void  cDBusObject::Action(void)
{
  if (_introspection_data == NULL) {
     GError *err = NULL;
     _introspection_data = g_dbus_node_info_new_for_xml(NodeInfo(), &err);
     if (err != NULL) {
        esyslog("dbus2vdr: g_dbus_node_info_new_for_xml reports: %s", err->message);
        g_error_free(err);
        return;
        }
     }

  _context = g_main_context_new();
  g_main_context_push_thread_default(_context);

  _loop = g_main_loop_new(_context, FALSE);
  Connect();

  g_main_loop_run(_loop);

  g_main_context_pop_thread_default(_context);
  g_main_context_unref(_context);
  _context = NULL;
}

void  cDBusObject::HandleMethodCall(GDBusConnection       *connection,
                                    const gchar           *sender,
                                    const gchar           *object_path,
                                    const gchar           *interface_name,
                                    const gchar           *method_name,
                                    GVariant              *parameters,
                                    GDBusMethodInvocation *invocation)
{
  g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED, "method not implemented yet");
}

void  cDBusObject::Connect(void)
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

void  cDBusObject::Disconnect(void)
{
}

void  cDBusObject::RegisterObject(void)
{
  if ((_connection == NULL) || (_introspection_data == NULL))
     return;

  if (_registration_id != 0)
     UnregisterObject();
  _registration_id = g_dbus_connection_register_object(_connection, Path(), _introspection_data->interfaces[0], &_interface_vtable, this, NULL, NULL);
}

void  cDBusObject::UnregisterObject(void)
{
  if (_registration_id != 0) {
     if (_connection != NULL)
        g_dbus_connection_unregister_object(_connection, _registration_id);
     _registration_id = 0;
     }
}

void  cDBusObject::handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusObject *obj = (cDBusObject*)user_data;
  obj->HandleMethodCall(connection, sender, object_path, interface_name, method_name, parameters, invocation);
}

void  cDBusObject::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  //cDBusObject *obj = (cDBusObject*)user_data;
}

void  cDBusObject::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusObject *obj = (cDBusObject*)user_data;
  obj->UnregisterObject();

  g_bus_unown_name(obj->_owner_id);
  obj->_owner_id = 0;

  g_object_unref(obj->_connection);
  obj->_connection = NULL;
  if (obj->_connect_status == 4)
     obj->_connect_status = 0;
  else
     obj->_connect_status = 1;

  if (obj->_reconnect)
     obj->Connect();
}

void  cDBusObject::on_bus_get(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusObject *obj = (cDBusObject*)user_data;
  obj->_connection = g_bus_get_finish(res, NULL);
  if (obj->_connection != NULL) {
     obj->_connect_status = 3;
     g_dbus_connection_set_exit_on_close(obj->_connection, FALSE);
     obj->RegisterObject();
     obj->_owner_id = g_bus_own_name_on_connection(obj->_connection,
                                                   obj->Busname(),
                                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                                   on_name_acquired,
                                                   on_name_lost,
                                                   user_data,
                                                   NULL);
     }
  else
     obj->_connect_status = 1;
}

gboolean  cDBusObject::do_reconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusObject *obj = (cDBusObject*)user_data;
  if (obj->_connect_status == 3) {
     obj->_connect_status = 4;
     return FALSE;
     }

  if (obj->_connect_status == 1) {
     obj->_connect_status = 2;
     g_bus_get(G_BUS_TYPE_SYSTEM, NULL, on_bus_get, user_data);
/*
     g_dbus_connection_new_for_address("unix:path=/var/run/dbus/system_bus_socket",
                                       G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                       NULL,
                                       NULL,
                                       on_bus_get,
                                       user_data);
*/
     return TRUE;
     }
  else if (obj->_connect_status == 2)
     return TRUE;

  return FALSE;
}

gboolean  cDBusObject::do_connect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusObject *obj = (cDBusObject*)user_data;
  if (obj->_connect_status == 0) {
     obj->_connect_status = 2;
     g_bus_get(G_BUS_TYPE_SYSTEM, NULL, on_bus_get, user_data);
/*
     g_dbus_connection_new_for_address("unix:path=/var/run/dbus/system_bus_socket",
                                       G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                       NULL,
                                       NULL,
                                       on_bus_get,
                                       user_data);
*/
     GSource *source = g_timeout_source_new(500);
     g_source_set_callback(source,
                           do_reconnect,
                           user_data,
                           NULL);
     g_source_attach(source, obj->_context);
     }

  return FALSE;
}

gboolean  cDBusObject::do_disconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusObject *obj = (cDBusObject*)user_data;
  if (obj->_connection != NULL)
     obj->UnregisterObject();

  if (obj->_owner_id > 0) {
     g_bus_unown_name(obj->_owner_id);
     obj->_owner_id = 0;
     }

  if (obj->_connection != NULL) {
     g_object_unref(obj->_connection);
     obj->_connection = NULL;
     obj->_connect_status = 0;
     }

  g_main_loop_quit(obj->_loop);
  return FALSE;
}

void  cDBusObject::on_flush(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusObject *obj = (cDBusObject*)user_data;
  g_dbus_connection_flush_finish(obj->_connection, res, NULL);
  if (obj->_disconnect_status == 0)
     return;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source,
                        do_disconnect,
                        user_data,
                        NULL);
  g_source_attach(source, obj->_context);
}

gboolean  cDBusObject::do_flush(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusObject *obj = (cDBusObject*)user_data;
  if (obj->_connection != NULL)
     g_dbus_connection_flush(obj->_connection, NULL, on_flush, user_data);

  return FALSE;
}
