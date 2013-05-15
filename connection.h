#ifndef __DBUS2VDR_CONNECTION_H
#define __DBUS2VDR_CONNECTION_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusObject;
class cDBusTcpAddress;

class cMainLoop
{
private:
  GThread         *_thread;
  GMainLoop       *_loop;

  static gpointer  do_loop(gpointer data)
  {
    dsyslog("dbus2vdr: mainloop started");
    if (data != NULL) {
       cMainLoop *mainloop = (cMainLoop*)data;
       mainloop->_loop = g_main_loop_new(NULL, FALSE);
       if (mainloop->_loop != NULL)
          g_main_loop_run(mainloop->_loop);
       }
    dsyslog("dbus2vdr: mainloop stopped");
    return NULL;
  };

public:
  cMainLoop(void)
  {
    _loop = NULL;
    _thread = g_thread_new("mainloop", do_loop, this);
  };

  virtual ~cMainLoop(void)
  {
    if (_loop != NULL)
       g_main_loop_quit(_loop);
    if (_thread != NULL) {
       g_thread_join(_thread);
       g_thread_unref(_thread);
       _thread = NULL;
	      }
    if (_loop != NULL) {
       g_main_loop_unref(_loop);
       _loop = NULL;
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
    //TODO: add callback

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

  static gpointer  do_action(gpointer data);

  GThread         *_thread;
  GMutex           _mutex;

  gchar           *_busname;
  GBusType         _bus_type;

  GMainContext    *_context;
  GMainLoop       *_loop;
  GDBusConnection *_connection;
  guint            _owner_id;
  gboolean         _reconnect;
  guint            _connect_status;
  guint            _disconnect_status;

  cList<cDBusObject>     _objects;
  cList<cDBusSignal>     _signals;
  cList<cDBusMethodCall> _method_calls;

  void  Init(const char *Busname);
  void  Connect(void);
  void  Disconnect(void);
  void  RegisterObjects(void);
  void  UnregisterObjects(void);

protected:
  virtual void  Action(void);

public:
  cDBusConnection(const char *Busname, GBusType  Type);
  virtual ~cDBusConnection(void);

  GDBusConnection *GetConnection(void) const { return _connection; };
  const char      *Name(void) const;

  // must be called before "Start"
  void  AddObject(cDBusObject *Object);

  void  Start(void);

  // "Signal" will be deleted by cDBusConnection
  void  EmitSignal(cDBusSignal *Signal);
  void  CallMethod(cDBusMethodCall *Call);
};

#endif
