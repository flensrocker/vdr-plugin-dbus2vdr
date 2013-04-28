#ifndef __DBUS2VDR_OBJECT_H
#define __DBUS2VDR_OBJECT_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusConnection;

class cDBusObject : public cListObject
{
private:
  static void      handle_method_call(GDBusConnection       *connection,
                                      const gchar           *sender,
                                      const gchar           *object_path,
                                      const gchar           *interface_name,
                                      const gchar           *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation,
                                      gpointer               user_data);

  gchar           *_path;
  GDBusNodeInfo   *_introspection_data;
  guint            _registration_id;
  cDBusConnection *_connection;

  static const GDBusInterfaceVTable _interface_vtable;
  
protected:

public:
  cDBusObject(const char *Path);
  virtual ~cDBusObject(void);

  const gchar  *Path(void) const { return _path; };

  void  SetConnection(cDBusConnection *Connection) { _connection = Connection; };
  void  Register(void);
  void  Unregister(void);

  virtual const gchar  *XmlNodeInfo(void) const;
  virtual void  HandleMethodCall(GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *method_name,
                                 GVariant              *parameters,
                                 GDBusMethodInvocation *invocation);
};


class cDBusConnection : public cThread
{
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

  gchar           *_busname;
  GBusType         _bus_type;
  gchar           *_bus_address;
  GMainContext    *_context;
  GMainLoop       *_loop;
  GDBusConnection *_connection;
  guint            _owner_id;
  gboolean         _reconnect;
  guint            _connect_status;
  guint            _disconnect_status;

  cList<cDBusObject> _objects;

  void  Init(const char *Busname);
  void  Connect(void);
  void  Disconnect(void);
  void  RegisterObjects(void);
  void  UnregisterObjects(void);

protected:
  virtual void  Action(void);

public:
  cDBusConnection(const char *Busname, GBusType  Type);
  cDBusConnection(const char *Busname, const char *Address);
  virtual ~cDBusConnection(void);

  GDBusConnection *GetConnection(void) const { return _connection; };

  void  AddObject(cDBusObject *Object);
  bool  StartMessageHandler(void);
  void  StopMessageHandler(void);
};

#endif
