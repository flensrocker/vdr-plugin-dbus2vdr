#include "connection.h"
#include "helper.h"
#include "object.h"


cDBusConnection::cDBusSignal::cDBusSignal(const char *DestinationBusname, const char *ObjectPath, const char *Interface, const char *Signal, GVariant *Parameters)
{
  _destination_busname = g_strdup(DestinationBusname);
  _object_path = g_strdup(ObjectPath);
  _interface = g_strdup(Interface);
  _signal = g_strdup(Signal);
  if (Parameters != NULL)
     _parameters = g_variant_ref(Parameters);
  else
     _parameters = NULL;
}

cDBusConnection::cDBusSignal::~cDBusSignal(void)
{
  g_free(_destination_busname);
  g_free(_object_path);
  g_free(_interface);
  g_free(_signal);
  if (_parameters != NULL)
     g_variant_unref(_parameters);
}


cDBusConnection::cDBusMethodCall::cDBusMethodCall(const char *DestinationBusname, const char *ObjectPath, const char *Interface, const char *Method, GVariant *Parameters)
{
  _destination_busname = g_strdup(DestinationBusname);
  _object_path = g_strdup(ObjectPath);
  _interface = g_strdup(Interface);
  _method = g_strdup(Method);
  if (Parameters != NULL)
     _parameters = g_variant_ref(Parameters);
  else
     _parameters = NULL;
}

cDBusConnection::cDBusMethodCall::~cDBusMethodCall(void)
{
  g_free(_destination_busname);
  g_free(_object_path);
  g_free(_interface);
  g_free(_method);
  if (_parameters != NULL)
     g_variant_unref(_parameters);
}


cDBusConnection::cDBusConnection(const char *Busname, GBusType  Type)
{
  Init(Busname);
  _bus_type = Type;
}

cDBusConnection::~cDBusConnection(void)
{
  Disconnect();
  if (_thread != NULL)
     g_thread_join(_thread);

  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }

  if (_thread != NULL) {
     g_thread_unref(_thread);
     _thread = NULL;
     }
  g_mutex_clear(&_mutex);
}

void  cDBusConnection::Init(const char *Busname)
{
  _thread = NULL;
  g_mutex_init(&_mutex);

  _busname = g_strdup(Busname);
  _bus_type = G_BUS_TYPE_NONE;
  _context = NULL;
  _loop = NULL;
  _connection = NULL;
  _owner_id = 0;
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;
}

const char  *cDBusConnection::Name(void) const
{
  if (_bus_type == G_BUS_TYPE_SYSTEM)
     return "SystemBus";
  if (_bus_type == G_BUS_TYPE_SESSION)
     return "SessionBus";
  return "UnknownBus";
}

void  cDBusConnection::AddObject(cDBusObject *Object)
{
  if (Object != NULL) {
     Object->SetConnection(this);
     _objects.Add(Object);
     }
}

void  cDBusConnection::Start(void)
{
  if (_thread != NULL) {
     esyslog("dbus2vdr: %s: connection started multiple times!", Name());
     return;
     }
  _thread = g_thread_new("connection", do_action, this);
}

void  cDBusConnection::EmitSignal(cDBusSignal *Signal)
{
  g_mutex_lock(&_mutex);
  bool addHandler = (_signals.Count() == 0);
  _signals.Add(Signal);
  if (addHandler) {
     GSource *source = g_idle_source_new();
     g_source_set_priority(source, G_PRIORITY_DEFAULT);
     g_source_set_callback(source, do_emit_signal, this, NULL);
     g_source_attach(source, _context);
     }
  g_mutex_unlock(&_mutex);
}

void  cDBusConnection::CallMethod(cDBusMethodCall *Call)
{
  g_mutex_lock(&_mutex);
  bool addHandler = (_method_calls.Count() == 0);
  _method_calls.Add(Call);
  if (addHandler) {
     GSource *source = g_idle_source_new();
     g_source_set_priority(source, G_PRIORITY_DEFAULT);
     g_source_set_callback(source, do_call_method, this, NULL);
     g_source_attach(source, _context);
     }
  g_mutex_unlock(&_mutex);
}

gpointer  cDBusConnection::do_action(gpointer data)
{
  if (data != NULL) {
     cDBusConnection *conn = (cDBusConnection*)data;
     isyslog("dbus2vdr: %s: connection thread started", conn->Name());
     conn->Action();
     isyslog("dbus2vdr: %s: connection thread stopped", conn->Name());
     }
  return NULL;
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
  dsyslog("dbus2vdr: %s: Connect", Name());
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source, do_connect, this, NULL);
  g_source_attach(source, _context);
}

void  cDBusConnection::Disconnect(void)
{
  dsyslog("dbus2vdr: %s: Disconnect", Name());
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

  dsyslog("dbus2vdr: %s: RegisterObjects", Name());
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Register();
}

void  cDBusConnection::UnregisterObjects(void)
{
  dsyslog("dbus2vdr: %s: UnregisterObjects", Name());
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Unregister();
}

void  cDBusConnection::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_name_acquired %s", conn->Name(), name);
}

void  cDBusConnection::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_name_lost %s", conn->Name(), name);
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
  dsyslog("dbus2vdr: %s: on_bus_get", conn->Name());
  if (conn->_bus_type != G_BUS_TYPE_NONE)
     conn->_connection = g_bus_get_finish(res, NULL);

  if (conn->_connection != NULL) {
     isyslog("dbus2vdr: %s: connected with unique name %s", conn->Name(), g_dbus_connection_get_unique_name(conn->_connection));
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

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: do_reconnect", conn->Name());
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
     esyslog("dbus2vdr: %s: can't connect bus without address", conn->Name());
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
  dsyslog("dbus2vdr: %s: do_connect", conn->Name());
  if (conn->_connect_status == 0) {
     conn->_connect_status = 2;
     if (conn->_bus_type != G_BUS_TYPE_NONE)
        g_bus_get(conn->_bus_type, NULL, on_bus_get, user_data);
     else {
        esyslog("dbus2vdr: %s: can't connect bus without address", conn->Name());
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
  dsyslog("dbus2vdr: %s: do_disconnect", conn->Name());
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

  if (conn->_loop != NULL)
     g_main_loop_quit(conn->_loop);
  return FALSE;
}

void  cDBusConnection::on_flush(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_flush", conn->Name());
  g_dbus_connection_flush_finish(conn->_connection, res, NULL);
  if (conn->_disconnect_status == 0)
     return;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source, do_disconnect, user_data, NULL);
  g_source_attach(source, conn->_context);
}

gboolean  cDBusConnection::do_flush(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: do_flush", conn->Name());
  g_dbus_connection_flush(conn->_connection, NULL, on_flush, user_data);

  return FALSE;
}

gboolean  cDBusConnection::do_emit_signal(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;

  // we're about to disconnect, so forget the pending signals
  if (conn->_disconnect_status > 0)
     return FALSE;

  // we're not successfully connected, so try again later
  if (conn->_connect_status < 3)
     return TRUE;

  g_mutex_lock(&conn->_mutex);
  GError *err = NULL;
  for (cDBusSignal *s = conn->_signals.First(); s; s = conn->_signals.Next(s)) {
      gchar *p = g_variant_print(s->_parameters, TRUE);
      dsyslog("dbus2vdr: %s: emit signal %s %s %s %s", conn->Name(), s->_object_path, s->_interface, s->_signal, p);
      g_free(p);
      g_dbus_connection_emit_signal(conn->_connection, s->_destination_busname, s->_object_path, s->_interface, s->_signal, s->_parameters, &err);
      if (err != NULL) {
         esyslog("dbus2vdr: %s: g_dbus_connection_emit_signal reports: %s", conn->Name(), err->message);
         g_error_free(err);
         err = NULL;
         }
      }
  conn->_signals.Clear();
  g_mutex_unlock(&conn->_mutex);
  return FALSE;
}

gboolean  cDBusConnection::do_call_method(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;

  // we're about to disconnect, so forget the pending signals
  if (conn->_disconnect_status > 0)
     return FALSE;

  // we're not successfully connected, so try again later
  if (conn->_connect_status < 3)
     return TRUE;

  g_mutex_lock(&conn->_mutex);
  for (cDBusMethodCall *c = conn->_method_calls.First(); c; c = conn->_method_calls.Next(c)) {
      gchar *p = g_variant_print(c->_parameters, TRUE);
      dsyslog("dbus2vdr: %s: call method %s %s %s %s", conn->Name(), c->_object_path, c->_interface, c->_method, p);
      g_free(p);
      g_dbus_connection_call(conn->_connection, c->_destination_busname, c->_object_path, c->_interface, c->_method, c->_parameters, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
      }
  conn->_method_calls.Clear();
  g_mutex_unlock(&conn->_mutex);
  return FALSE;
}
