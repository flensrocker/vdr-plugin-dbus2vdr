#include "connection.h"
#include "helper.h"
#include "object.h"

#include <vdr/plugin.h>


class cConnectionWorkerData
{
public:
  static GThreadPool *_thread_pool;

  cList<cDBusSignal> *_signals;

  cConnectionWorkerData(cList<cDBusSignal> *Signals)
  {
    _signals = Signals;
  };

  ~cConnectionWorkerData(void)
  {
    if (_signals != NULL) {
       delete _signals;
       _signals = NULL;
       }
  };
};

GThreadPool *cConnectionWorkerData::_thread_pool = NULL;


void  cDBusConnection::FreeThreadPool(void)
{
  if (cConnectionWorkerData::_thread_pool != NULL) {
     g_thread_pool_free(cConnectionWorkerData::_thread_pool, FALSE, TRUE);
     cConnectionWorkerData::_thread_pool = NULL;
     isyslog("dbus2vdr: thread-pool for handling signal-emits stopped");
     }
}

cDBusConnection::cDBusConnection(const char *Busname, GBusType  Type, GMainContext *Context)
{
  _busname = g_strdup(Busname);
  _bus_type = Type;
  _bus_address = NULL;
  _name = NULL;

  _on_name_acquired = NULL;
  _on_name_lost = NULL;
  _on_name_user_data = NULL;

  _on_connect = NULL;
  _on_disconnect = NULL;
  _on_connect_user_data = NULL;

  _context = Context;
  _connection = NULL;
  _owner_id = 0;
  _auto_reconnect = TRUE;
  _reconnect = TRUE;
  _connect_status = 0;
  _disconnect_status = 0;

  g_mutex_init(&_disconnect_mutex);
  g_cond_init(&_disconnect_cond);
  g_mutex_init(&_flush_mutex);
  g_cond_init(&_flush_cond);
}

cDBusConnection::cDBusConnection(const char *Busname, const char *Name, const char *Address, GMainContext *Context)
{
  _busname = g_strdup(Busname);
  _bus_type = G_BUS_TYPE_NONE;
  _bus_address = g_strdup(Address);
  _name = g_strdup(Name);

  _on_name_acquired = NULL;
  _on_name_lost = NULL;
  _on_name_user_data = NULL;

  _on_connect = NULL;
  _on_disconnect = NULL;
  _on_connect_user_data = NULL;

  _context = Context;
  _connection = NULL;
  _owner_id = 0;
  _auto_reconnect = FALSE;
  _reconnect = FALSE;
  _connect_status = 0;
  _disconnect_status = 0;

  g_mutex_init(&_disconnect_mutex);
  g_cond_init(&_disconnect_cond);
  g_mutex_init(&_flush_mutex);
  g_cond_init(&_flush_cond);
}

cDBusConnection::~cDBusConnection(void)
{
  cDBusSignal *s;
  while ((s = _subscriptions.First()) != NULL)
        Unsubscribe(s);
  Flush();
  Disconnect();

  if (_bus_address != NULL) {
     g_free(_bus_address);
     _bus_address = NULL;
     }

  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }

  g_mutex_clear(&_disconnect_mutex);
  g_cond_clear(&_disconnect_cond);
  g_mutex_clear(&_flush_mutex);
  g_cond_clear(&_flush_cond);
  dsyslog("dbus2vdr: %s: ~cDBusConnection", Name());

  if (_name != NULL) {
     g_free(_name);
     _name = NULL;
     }
}

const char  *cDBusConnection::Name(void) const
{
  if (_name != NULL)
     return _name;
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

void  cDBusConnection::EmitSignal(cDBusSignal *Signal)
{
  g_mutex_lock(&_flush_mutex);
  bool addHandler = (_signals.Count() == 0);
  Signal->_connection = this;
  _signals.Add(Signal);
  if (addHandler) {
     GSource *source = g_idle_source_new();
     g_source_set_priority(source, G_PRIORITY_DEFAULT);
     g_source_set_callback(source, do_emit_signal, this, NULL);
     g_source_attach(source, _context);
     }
  g_mutex_unlock(&_flush_mutex);
}

void  cDBusConnection::CallMethod(cDBusMethodCall *Call)
{
  g_mutex_lock(&_flush_mutex);
  bool addHandler = (_method_calls.Count() == 0);
  Call->_connection = this;
  _method_calls.Add(Call);
  if (addHandler) {
     GSource *source = g_idle_source_new();
     g_source_set_priority(source, G_PRIORITY_DEFAULT);
     g_source_set_callback(source, do_call_method, this, NULL);
     g_source_attach(source, _context);
     }
  g_mutex_unlock(&_flush_mutex);
}

void  cDBusConnection::Subscribe(cDBusSignal *Signal)
{
  g_mutex_lock(&_flush_mutex);
  Signal->_connection = this;
  Signal->_subscription_id = g_dbus_connection_signal_subscribe(_connection, Signal->Busname(), Signal->Interface(), Signal->Signal(), Signal->ObjectPath(), NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_signal, Signal, NULL);
  _subscriptions.Add(Signal);
  g_mutex_unlock(&_flush_mutex);
}

void  cDBusConnection::Unsubscribe(cDBusSignal *Signal)
{
  g_mutex_lock(&_flush_mutex);
  if (Signal->_subscription_id != 0)
     g_dbus_connection_signal_unsubscribe(_connection, Signal->_subscription_id);
  _subscriptions.Del(Signal);
  g_mutex_unlock(&_flush_mutex);
}

guint cDBusConnection::Watch(cDBusWatcher *Watcher)
{
  g_mutex_lock(&_flush_mutex);
  guint id = Watcher->Watch(this);
  _watchers.Add(Watcher);
  g_mutex_unlock(&_flush_mutex);
  return id;
}

void  cDBusConnection::Unwatch(guint Id)
{
  g_mutex_lock(&_flush_mutex);
  for (cDBusWatcher *w = _watchers.First(); w ; w = _watchers.Next(w)) {
      if (w->Id() == Id) {
         w->Unwatch();
         _watchers.Del( w, true);
         break;
         }
      }
  g_mutex_unlock(&_flush_mutex);
}

bool  cDBusConnection::Flush(void)
{
  dsyslog("dbus2vdr: %s: Flush", Name());
  if (_connect_status == 0)
     return false;
  g_mutex_lock(&_flush_mutex);
  while ((_signals.Count() > 0) || (_method_calls.Count() > 0))
        g_cond_wait(&_flush_cond, &_flush_mutex);
  g_mutex_unlock(&_flush_mutex);
  return true;
}

void  cDBusConnection::Connect(gboolean AutoReconnect)
{
  dsyslog("dbus2vdr: %s: Connect", Name());
  _reconnect = AutoReconnect;
  _connect_status = 0;

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source, do_connect, this, NULL);
  g_source_attach(source, _context);
}

void  cDBusConnection::Disconnect(void)
{
  dsyslog("dbus2vdr: %s: Disconnect", Name());
  if (_connect_status == 0)
     return;
  g_mutex_lock(&_disconnect_mutex);
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

  // wait until really disconnected
  while (_disconnect_status < 2)
        g_cond_wait(&_disconnect_cond, &_disconnect_mutex);
  _disconnect_status = 0;
  g_mutex_unlock(&_disconnect_mutex);
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
  if (_connection == NULL)
     return;

  dsyslog("dbus2vdr: %s: UnregisterObjects", Name());
  for (cDBusObject *obj = _objects.First(); obj; obj = _objects.Next(obj))
      obj->Unregister();
}

void  cDBusConnection::RegisterWatchers(void)
{
  if (_connection == NULL)
     return;

  dsyslog("dbus2vdr: %s: RegisterWatchers", Name());
  for (cDBusWatcher *w = _watchers.First(); w; w = _watchers.Next(w))
      w->Watch(this);
}

void  cDBusConnection::UnregisterWatchers(void)
{
  if (_connection == NULL)
     return;

  dsyslog("dbus2vdr: %s: UnregisterWatchers", Name());
  for (cDBusWatcher *w = _watchers.First(); w; w = _watchers.Next(w))
      w->Unwatch();
}

void  cDBusConnection::on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_name_acquired %s", conn->Name(), name);
  if (conn->_on_name_acquired != NULL)
     conn->_on_name_acquired(conn, conn->_on_name_user_data);
}

void  cDBusConnection::on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_name_lost %s", conn->Name(), name);
  if (conn->_on_disconnect != NULL)
     conn->_on_disconnect(conn, conn->_on_connect_user_data);

  conn->UnregisterWatchers();

  if (conn->_busname != NULL)
     conn->UnregisterObjects();

  if (conn->_owner_id > 0) {
     g_bus_unown_name(conn->_owner_id);
     conn->_owner_id = 0;
     }

  if (conn->_bus_address != NULL)
     g_dbus_connection_close(conn->_connection, NULL, on_name_lost_close, user_data);
  else
     on_name_lost_close(NULL, NULL, user_data);
}

void  cDBusConnection::on_name_lost_close(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_name_lost_close", conn->Name());
  if (conn->_connection != NULL) {
     if ((res != NULL) && (conn->_bus_address != NULL))
        g_dbus_connection_close_finish(conn->_connection, res, NULL);
     g_object_unref(conn->_connection);
     conn->_connection = NULL;
     }

  if (conn->_connect_status == 4)
     conn->_connect_status = 0;
  else
     conn->_connect_status = 1;

  if (conn->_reconnect)
     conn->Connect(TRUE);

  if (conn->_on_name_lost != NULL)
     conn->_on_name_lost(conn, conn->_on_name_user_data);
}

void  cDBusConnection::on_bus_get(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_bus_get", conn->Name());
  if (conn->_bus_type != G_BUS_TYPE_NONE)
     conn->_connection = g_bus_get_finish(res, NULL);
  else if (conn->_bus_address != NULL) {
     GError *err = NULL;
     conn->_connection = g_dbus_connection_new_for_address_finish(res, &err);
     if ((conn->_connection == NULL) || (err != NULL)) {
        esyslog("dbus2vdr: %s: can't get connection for address %s", conn->Name(), conn->_bus_address);
        if (err != NULL) {
           esyslog("dbus2vdr: %s: error: %s", conn->Name(), err->message);
           g_error_free(err);
           err = NULL;
           }
        if (conn->_connection != NULL) {
           g_object_unref(conn->_connection);
           conn->_connection = NULL;
           }
        }
     }

  if (conn->_connection != NULL) {
     isyslog("dbus2vdr: %s: connected with unique name %s", conn->Name(), g_dbus_connection_get_unique_name(conn->_connection));
     conn->_connect_status = 3;
     g_dbus_connection_set_exit_on_close(conn->_connection, FALSE);
     if (conn->_on_connect != NULL)
        conn->_on_connect(conn, conn->_on_connect_user_data);
     if (conn->_busname != NULL) {
        conn->RegisterObjects();
        conn->RegisterWatchers();
        conn->_owner_id = g_bus_own_name_on_connection(conn->_connection,
                                                       conn->_busname,
                                                       G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                                       on_name_acquired,
                                                       on_name_lost,
                                                       user_data,
                                                       NULL);
        }
     }
  else
     conn->_connect_status = 1;
}

gboolean  cDBusConnection::do_reconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  if (!conn->_reconnect)
     return FALSE;

  if (conn->_connect_status == 3) {
     conn->_connect_status = 4;
     return FALSE;
     }

  dsyslog("dbus2vdr: %s: do_reconnect", conn->Name());
  if (conn->_connect_status == 1) {
     conn->_connect_status = 2;
     if (conn->_bus_type != G_BUS_TYPE_NONE) {
        g_bus_get(conn->_bus_type, NULL, on_bus_get, user_data);
        return TRUE;
        }
     if (conn->_bus_address != NULL) {
        g_dbus_connection_new_for_address(conn->_bus_address, (GDBusConnectionFlags)(G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION | G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT), NULL, NULL, on_bus_get, user_data);
        return TRUE;
        }
     esyslog("dbus2vdr: %s: can't connect without address", conn->Name());
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
     else if (conn->_bus_address != NULL)
        g_dbus_connection_new_for_address(conn->_bus_address, (GDBusConnectionFlags)(G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION | G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT), NULL, NULL, on_bus_get, user_data);
     else {
        esyslog("dbus2vdr: %s: can't connect bus without address", conn->Name());
        return FALSE;
        }

     GSource *source = g_timeout_source_new_seconds(1);
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

  conn->UnregisterWatchers();

  if (conn->_busname != NULL)
     conn->UnregisterObjects();

  if (conn->_owner_id > 0) {
     g_bus_unown_name(conn->_owner_id);
     conn->_owner_id = 0;
     }

  if (conn->_bus_address != NULL)
     g_dbus_connection_close(conn->_connection, NULL, on_disconnect_close, user_data);
  else
     on_disconnect_close(NULL, NULL, user_data);

  return FALSE;
}

void  cDBusConnection::on_disconnect_close(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_disconnect_close", conn->Name());
  if (conn->_connection != NULL) {
     if ((res != NULL) && (conn->_bus_address != NULL))
        g_dbus_connection_close_finish(conn->_connection, res, NULL);
     g_object_unref(conn->_connection);
     conn->_connection = NULL;
     conn->_connect_status = 0;
     }

  g_mutex_lock(&conn->_disconnect_mutex);
  conn->_disconnect_status = 2;
  g_cond_signal(&conn->_disconnect_cond);
  g_mutex_unlock(&conn->_disconnect_mutex);
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

void  cDBusConnection::on_flush(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusConnection *conn = (cDBusConnection*)user_data;
  dsyslog("dbus2vdr: %s: on_flush", conn->Name());
  g_dbus_connection_flush_finish(conn->_connection, res, NULL);

  g_mutex_lock(&conn->_disconnect_mutex);
  if (conn->_disconnect_status == 0) {
     g_mutex_unlock(&conn->_disconnect_mutex);
     return;
     }
  g_mutex_unlock(&conn->_disconnect_mutex);

  GSource *source = g_idle_source_new();
  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_set_callback(source, do_disconnect, user_data, NULL);
  g_source_attach(source, conn->_context);
}

gboolean  cDBusConnection::do_emit_signal(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;

  // we're about to disconnect, so forget the pending signals
  g_mutex_lock(&conn->_disconnect_mutex);
  if (conn->_disconnect_status > 0) {
     g_mutex_unlock(&conn->_disconnect_mutex);
     return FALSE;
     }
  g_mutex_unlock(&conn->_disconnect_mutex);

  // we're not successfully connected, so try again later
  if (conn->_connect_status < 3)
     return TRUE;

  cList<cDBusSignal>  *signals = NULL;
  g_mutex_lock(&conn->_flush_mutex);
  if (conn->_signals.Count() > 0) {
     signals = new cList<cDBusSignal>();
     do {
        cDBusSignal *s = conn->_signals.First();
        if (s == NULL)
           break;
        conn->_signals.Del(s, false);
        signals->Add(s);
        } while (true);
     }
  g_cond_signal(&conn->_flush_cond);
  g_mutex_unlock(&conn->_flush_mutex);

  if (signals != NULL) {
     cConnectionWorkerData *workerData = new cConnectionWorkerData(signals);
     if (cConnectionWorkerData::_thread_pool == NULL) {
        GError *err = NULL;
        cConnectionWorkerData::_thread_pool = g_thread_pool_new(do_work, NULL, 10, FALSE, &err);
        if (err != NULL) {
           esyslog("dbus2vdr: g_thread_pool_new reports: %s", err->message);
           g_error_free(err);
           if (cConnectionWorkerData::_thread_pool != NULL)
              g_thread_pool_free(cConnectionWorkerData::_thread_pool, TRUE, FALSE);
           cConnectionWorkerData::_thread_pool = NULL;
           do_work(workerData, NULL);
           return FALSE;
           }
        isyslog("dbus2vdr: thread-pool for handling signal-emits started");
        }
     g_thread_pool_push(cConnectionWorkerData::_thread_pool, workerData, NULL);
     }

  return FALSE;
}

gboolean  cDBusConnection::do_call_method(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusConnection *conn = (cDBusConnection*)user_data;

  // we're about to disconnect, so forget the pending signals
  g_mutex_lock(&conn->_disconnect_mutex);
  if (conn->_disconnect_status > 0) {
     g_mutex_unlock(&conn->_disconnect_mutex);
     return FALSE;
     }
  g_mutex_unlock(&conn->_disconnect_mutex);

  // we're not successfully connected, so try again later
  if (conn->_connect_status < 3)
     return TRUE;

  g_mutex_lock(&conn->_flush_mutex);
  cDBusMethodCall *c = conn->_method_calls.First();
  while (c) {
        if (SysLogLevel > 2) {
           gchar *p = NULL;
           if (c->_parameters != NULL)
              p = g_variant_print(c->_parameters, TRUE);
           dsyslog("dbus2vdr: %s: call method %s %s %s %s", conn->Name(), c->_object_path, c->_interface, c->_method, p);
           g_free(p);
           }
        g_dbus_connection_call(conn->_connection, c->_busname, c->_object_path, c->_interface, c->_method, c->_parameters, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, (c->_on_reply == NULL ? NULL : do_call_reply), c);
        if (c->_on_reply != NULL) {
           cDBusMethodCall *next = conn->_method_calls.Next(c);
           // will be deleted in do_call_reply
           conn->_method_calls.Del( c, false);
           c = next;
           }
        else
           c = conn->_method_calls.Next(c);
        }
  conn->_method_calls.Clear();
  g_cond_signal(&conn->_flush_cond);
  g_mutex_unlock(&conn->_flush_mutex);
  return FALSE;
}

void  cDBusConnection::do_call_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusMethodCall *call = (cDBusMethodCall*)user_data;
  GError *err = NULL;
  GVariant *reply = g_dbus_connection_call_finish(call->_connection->GetConnection(), res, &err);
  if (err != NULL) {
     esyslog("dbus2vdr: %s: error: %s", call->_connection->Name(), err->message);
     g_error_free(err);
     err = NULL;
     }
  call->_on_reply(reply, call->_on_reply_user_data);
  delete call;
}

void  cDBusConnection::on_signal(GDBusConnection *connection, const gchar *sender_name, const gchar *object_path, const gchar *interface_name, const gchar *signal_name, GVariant *parameters, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusSignal *signal = (cDBusSignal*)user_data;
  if (signal->_on_signal != NULL)
     signal->_on_signal(sender_name, object_path, interface_name, signal_name, parameters, signal->_on_signal_user_data);
}

void  cDBusConnection::do_work(gpointer data, gpointer user_data)
{
  if (data == NULL)
     return;

  cConnectionWorkerData *workerData = (cConnectionWorkerData*)data;
  GError *err = NULL;
  for (cDBusSignal *s = workerData->_signals->First(); s; s = workerData->_signals->Next(s)) {
      if (SysLogLevel > 2) {
         gchar *p = NULL;
         if (s->_parameters != NULL)
            p = g_variant_print(s->_parameters, TRUE);
         dsyslog("dbus2vdr: %s: emit signal %s %s %s %s", s->_connection->Name(), s->_object_path, s->_interface, s->_signal, p);
         g_free(p);
         }
      g_dbus_connection_emit_signal(s->_connection->_connection, s->_busname, s->_object_path, s->_interface, s->_signal, s->_parameters, &err);
      if (err != NULL) {
         esyslog("dbus2vdr: %s: g_dbus_connection_emit_signal reports: %s", s->_connection->Name(), err->message);
         g_error_free(err);
         err = NULL;
         }
      }
  delete workerData;
}

static void  sNotifyCaller(const char *caller, const char *event, guint id, const char *data)
{
  if ((caller == NULL) || (event == NULL))
     return;
  cPlugin *plugin = cPluginManager::GetPlugin(caller);
  if (plugin == NULL)
     return;
  cString call = cString::sprintf("event=%s,id=%d%s%s", event, id, (data != NULL ? "," : ""), (data != NULL ? data : ""));
  plugin->Service("dbus2vdr-event", (void*)(*call));
  isyslog("dbus2vdr: notified %s on event %s", caller, *call);
}


cDBusSignal::cDBusSignal(const char *Busname, const char *ObjectPath, const char *Interface, const char *Signal, GVariant *Parameters, cDBusOnSignalFunc OnSignal, gpointer UserData)
{
  _subscription_id = 0;
  _on_signal = OnSignal;
  _on_signal_user_data = UserData;
  _busname = g_strdup(Busname);
  _object_path = g_strdup(ObjectPath);
  _interface = g_strdup(Interface);
  _signal = g_strdup(Signal);
  if (Parameters != NULL)
     _parameters = g_variant_ref(Parameters);
  else
     _parameters = NULL;
}

cDBusSignal::~cDBusSignal(void)
{
  g_free(_busname);
  g_free(_object_path);
  g_free(_interface);
  g_free(_signal);
  if (_parameters != NULL)
     g_variant_unref(_parameters);
}


cDBusMethodCall::cDBusMethodCall(const char *Busname, const char *ObjectPath, const char *Interface, const char *Method, GVariant *Parameters, cDBusMethodReplyFunc OnReply, gpointer UserData)
{
  _busname = g_strdup(Busname);
  _object_path = g_strdup(ObjectPath);
  _interface = g_strdup(Interface);
  _method = g_strdup(Method);
  if (Parameters != NULL)
     _parameters = g_variant_ref(Parameters);
  else
     _parameters = NULL;
  _on_reply = OnReply;
  _on_reply_user_data = UserData;
}

cDBusMethodCall::~cDBusMethodCall(void)
{
  g_free(_busname);
  g_free(_object_path);
  g_free(_interface);
  g_free(_method);
  if (_parameters != NULL)
     g_variant_unref(_parameters);
}


void  cDBusWatcher::on_busname_appeared(GDBusConnection *connection, const gchar *name, const gchar *name_owner, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusWatcher *watcher = (cDBusWatcher*)user_data;
  cString esc_name = strescape(name_owner, "\\,");
  cString data = cString::sprintf("owner=%s", *esc_name);
  sNotifyCaller(watcher->Caller(), "watched-name-appeared", watcher->Id(), *data);
}

void  cDBusWatcher::on_busname_vanished(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusWatcher *watcher = (cDBusWatcher*)user_data;
  sNotifyCaller(watcher->Caller(), "watched-name-vanished", watcher->Id(), NULL);
}

cDBusWatcher::cDBusWatcher(const char *Caller, const char *Name)
{
  static guint ids = 0;
  _connection = NULL;
  _caller = g_strdup(Caller);
  _name = g_strdup(Name);
  _id = ++ids;
  _watch_id = 0;
}

cDBusWatcher::~cDBusWatcher(void)
{
  g_free(_caller);
  g_free(_name);
}

guint cDBusWatcher::Watch(cDBusConnection *Connection)
{
  if (_watch_id > 0)
     return _id;
  _connection = Connection;
  if ((_connection == NULL) || (_connection->GetConnection() == NULL))
     _watch_id = 0;
  else
     _watch_id = g_bus_watch_name_on_connection(_connection->GetConnection(), _name, G_BUS_NAME_WATCHER_FLAGS_NONE, on_busname_appeared, on_busname_vanished, this, NULL);
  return _id;
}

void  cDBusWatcher::Unwatch(void)
{
  if (_watch_id > 0) {
     g_bus_unwatch_name(_watch_id);
     _watch_id = 0;
     }
}
