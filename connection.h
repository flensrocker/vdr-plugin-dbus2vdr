#ifndef __DBUS2VDR_CONNECTION_H
#define __DBUS2VDR_CONNECTION_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusObject;
class cDBusTcpAddress;

class cDBusMainLoop
{
private:
  GMainContext    *_context;
  GMainLoop       *_loop;
  GThread         *_thread;
  
  GMutex           _started_mutex;
  GCond            _started_cond;
  bool             _started;

  static gpointer  do_loop(gpointer data)
  {
    dsyslog("dbus2vdr: mainloop started");
    if (data != NULL) {
       cDBusMainLoop *mainloop = (cDBusMainLoop*)data;
       g_mutex_lock(&mainloop->_started_mutex);
       mainloop->_started = true;
       g_cond_signal(&mainloop->_started_cond);
       g_mutex_unlock(&mainloop->_started_mutex);
       mainloop->_loop = g_main_loop_new(mainloop->_context, FALSE);
       if (mainloop->_loop != NULL) {
          g_main_loop_run(mainloop->_loop);
          g_main_loop_unref(mainloop->_loop);
          mainloop->_loop = NULL;
          }
       }
    dsyslog("dbus2vdr: mainloop stopped");
    return NULL;
  };

public:
  cDBusMainLoop(GMainContext *Context)
  {
    _context = Context;
    _loop = NULL;
    _started = false;
    g_mutex_init(&_started_mutex);
    g_cond_init(&_started_cond);
    g_mutex_lock(&_started_mutex);
    _thread = g_thread_new("mainloop", do_loop, this);
    while (!_started)
          g_cond_wait(&_started_cond, &_started_mutex);
    g_mutex_unlock(&_started_mutex);
    g_mutex_clear(&_started_mutex);
    g_cond_clear(&_started_cond);
  };

  virtual ~cDBusMainLoop(void)
  {
    if (_loop != NULL)
       g_main_loop_quit(_loop);
    if (_thread != NULL) {
       g_thread_join(_thread);
       g_thread_unref(_thread);
       _thread = NULL;
	      }
  };
};

class cDBusConnection
{
public:
  class cDBusSignal : public cListObject
  {
  friend class cDBusConnection;

  private:
    gchar    *_destination_busname;
    gchar    *_object_path;
    gchar    *_interface;
    gchar    *_signal;
    GVariant *_parameters;

  public:
    cDBusSignal(const char *DestinationBusname, const char *ObjectPath, const char *Interface, const char *Signal, GVariant *Parameters);
    virtual ~cDBusSignal(void);
  };

  class cDBusMethodCall : public cListObject
  {
  friend class cDBusConnection;

  private:
    gchar    *_destination_busname;
    gchar    *_object_path;
    gchar    *_interface;
    gchar    *_method;
    GVariant *_parameters;
    //TODO: add callback for reply message

  public:
    cDBusMethodCall(const char *DestinationBusname, const char *ObjectPath, const char *Interface, const char *Method, GVariant *Parameters);
    virtual ~cDBusMethodCall(void);
  };

private:
  // wrapper functions for GMainLoop calls
  static void      on_name_acquired(GDBusConnection *connection,
                                    const gchar     *name,
                                    gpointer         user_data);
  static void      on_name_lost(GDBusConnection *connection,
                                const gchar     *name,
                                gpointer         user_data);
  static void      on_bus_get(GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data);
  static gboolean  do_reconnect(gpointer user_data);
  static gboolean  do_connect(gpointer user_data);
  static gboolean  do_disconnect(gpointer user_data);

  static void      on_flush(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data);
  static gboolean  do_flush(gpointer user_data);

  static gboolean  do_emit_signal(gpointer user_data);
  static gboolean  do_call_method(gpointer user_data);

  gchar           *_busname;
  GBusType         _bus_type;

  GMainContext    *_context;
  GDBusConnection *_connection;
  guint            _owner_id;
  gboolean         _auto_reconnect;
  gboolean         _reconnect;
  guint            _connect_status;

  GMutex           _disconnect_mutex;
  GCond            _disconnect_cond;
  guint            _disconnect_status;

  GMutex           _flush_mutex;
  GCond            _flush_cond;

  cList<cDBusObject>     _objects;
  cList<cDBusSignal>     _signals;
  cList<cDBusMethodCall> _method_calls;

  void  RegisterObjects(void);
  void  UnregisterObjects(void);

public:
  cDBusConnection(const char *Busname, GBusType  Type, GMainContext *Context);
  virtual ~cDBusConnection(void);

  GDBusConnection *GetConnection(void) const { return _connection; };
  const char      *Name(void) const;

  // must be called before "Connect"
  void  AddObject(cDBusObject *Object);

  // "Connect" is async
  void  Connect(gboolean AutoReconnect);
  // "Disconnect" blocks
  void  Disconnect(void);

  // "Signal" and "Call" objects will be deleted by cDBusConnection
  void  EmitSignal(cDBusSignal *Signal);
  void  CallMethod(cDBusMethodCall *Call);
  // "Flush" blocks
  void  Flush(void);
};

#endif
