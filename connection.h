#ifndef __DBUS2VDR_CONNECTION_H
#define __DBUS2VDR_CONNECTION_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusObject;
class cDBusTcpAddress;

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

  static gboolean  do_monitor_file(gpointer user_data);
  static void      on_monitor_file(GFileMonitor *monitor, GFile *first, GFile *second, GFileMonitorEvent event, gpointer user_data);

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

  gchar           *_filename;
  GFileMonitor    *_file_monitor;
  cDBusTcpAddress *_bus_address;

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
  cDBusConnection(const char *Busname, const char *Filename);
  virtual ~cDBusConnection(void);

  GDBusConnection *GetConnection(void) const { return _connection; };

  // must be called before "Start"
  void  AddObject(cDBusObject *Object);

  void  Start(void);

  // "Signal" will be deleted by cDBusConnection
  void  EmitSignal(cDBusSignal *Signal);
  void  CallMethod(cDBusMethodCall *Call);
};

#endif
