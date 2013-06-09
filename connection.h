#ifndef __DBUS2VDR_CONNECTION_H
#define __DBUS2VDR_CONNECTION_H

#include <gio/gio.h>

#include <vdr/tools.h>


class cDBusObject;
class cDBusTcpAddress;
class cDBusConnection;

typedef void (*cDBusNameAcquiredFunc)(cDBusConnection *Connection, gpointer UserData);
typedef void (*cDBusNameLostFunc)(cDBusConnection *Connection, gpointer UserData);

typedef void (*cDBusOnConnectFunc)(cDBusConnection *Connection, gpointer UserData);
typedef void (*cDBusOnDisconnectFunc)(cDBusConnection *Connection, gpointer UserData);

typedef void (*cDBusMethodReplyFunc)(GVariant *Reply, gpointer UserData);
typedef void (*cDBusOnSignalFunc)(const gchar *SenderName, const gchar *ObjectPath, const gchar *Interface, const gchar *Signal, GVariant *Parameters, gpointer UserData);

class cDBusSignal : public cListObject
{
friend class cDBusConnection;

private:
  cDBusConnection *_connection;
  guint     _subscription_id;
  cDBusOnSignalFunc _on_signal;
  gpointer  _on_signal_user_data;
  gchar    *_busname;
  gchar    *_object_path;
  gchar    *_interface;
  gchar    *_signal;
  GVariant *_parameters;

public:
  cDBusSignal(const char *Busname, const char *ObjectPath, const char *Interface, const char *Signal, GVariant *Parameters, cDBusOnSignalFunc OnSignal, gpointer UserData);
  virtual ~cDBusSignal(void);

  const char  *Busname(void) const { return _busname; }
  const char  *ObjectPath(void) const { return _object_path; }
  const char  *Interface(void) const { return _interface; }
  const char  *Signal(void) const { return _signal; }
  GVariant    *Parameters(void) const { return _parameters; }
};

class cDBusMethodCall : public cListObject
{
friend class cDBusConnection;

private:
  cDBusConnection *_connection;
  gchar    *_busname;
  gchar    *_object_path;
  gchar    *_interface;
  gchar    *_method;
  GVariant *_parameters;
  cDBusMethodReplyFunc _on_reply;
  gpointer  _on_reply_user_data;

public:
  cDBusMethodCall(const char *Busname, const char *ObjectPath, const char *Interface, const char *Method, GVariant *Parameters, cDBusMethodReplyFunc OnReply, gpointer UserData);
  virtual ~cDBusMethodCall(void);
};

class cDBusConnection
{
private:
  // wrapper functions for GMainLoop calls
  static void      on_name_acquired(GDBusConnection *connection,
                                    const gchar     *name,
                                    gpointer         user_data);
  static void      on_name_lost(GDBusConnection *connection,
                                const gchar     *name,
                                gpointer         user_data);
  static void      on_name_lost_close(GObject *source_object,
                                      GAsyncResult *res,
                                      gpointer user_data);
  static void      on_bus_get(GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data);
  static gboolean  do_reconnect(gpointer user_data);
  static gboolean  do_connect(gpointer user_data);
  static gboolean  do_disconnect(gpointer user_data);
  static void      on_disconnect_close(GObject *source_object,
                                       GAsyncResult *res,
                                       gpointer user_data);

  static void      on_flush(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data);
  static gboolean  do_flush(gpointer user_data);

  static gboolean  do_emit_signal(gpointer user_data);
  static gboolean  do_call_method(gpointer user_data);
  static void      do_call_reply(GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data);
  static void      on_signal(GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data);

  gchar           *_busname;
  GBusType         _bus_type;
  gchar           *_bus_address;
  gchar           *_name;

  cDBusNameAcquiredFunc  _on_name_acquired;
  cDBusNameLostFunc      _on_name_lost;
  gpointer               _on_name_user_data;
  
  cDBusOnConnectFunc     _on_connect;
  cDBusOnDisconnectFunc  _on_disconnect;
  gpointer               _on_connect_user_data;

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
  cList<cDBusSignal>     _subscriptions;

  void  RegisterObjects(void);
  void  UnregisterObjects(void);

public:
  cDBusConnection(const char *Busname, GBusType  Type, GMainContext *Context);
  cDBusConnection(const char *Busname, const char *Name, const char *Address, GMainContext *Context);
  virtual ~cDBusConnection(void);

  GDBusConnection *GetConnection(void) const { return _connection; };
  const char      *Name(void) const;

  // must be called before "Connect"
  void  AddObject(cDBusObject *Object);
  void  SetNameCallbacks(cDBusNameAcquiredFunc OnNameAcquired, cDBusNameLostFunc OnNameLost, gpointer OnNameUserData)
  {
    _on_name_acquired = OnNameAcquired;
    _on_name_lost = OnNameLost;
    _on_name_user_data = OnNameUserData;
  };
  void  SetConnectCallbacks(cDBusOnConnectFunc OnConnect, cDBusOnDisconnectFunc OnDisconnect, gpointer OnConnectUserData)
  {
    _on_connect = OnConnect;
    _on_disconnect = OnDisconnect;
    _on_connect_user_data = OnConnectUserData;
  };

  // "Connect" is async
  void  Connect(gboolean AutoReconnect);
  // "Disconnect" blocks
  void  Disconnect(void);

  // "Signal" and "Call" objects will be deleted by cDBusConnection
  void  EmitSignal(cDBusSignal *Signal);
  void  CallMethod(cDBusMethodCall *Call);
  void  Subscribe(cDBusSignal *Signal);
  void  Unsubscribe(cDBusSignal *Signal);
  // "Flush" blocks
  bool  Flush(void);
};

#endif
