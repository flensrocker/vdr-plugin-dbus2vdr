#ifndef __DBUS2VDR_OBJECT_H
#define __DBUS2VDR_OBJECT_H

#include <gio/gio.h>

#include <vdr/thread.h>


class cDBusObject : public cThread
{
private:
  // wrapper fundtions for GMainLoop calls
  static void      handle_method_call(GDBusConnection       *connection,
                                      const gchar           *sender,
                                      const gchar           *object_path,
                                      const gchar           *interface_name,
                                      const gchar           *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation,
                                      gpointer               user_data);
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

  static const GDBusInterfaceVTable _interface_vtable;
  
  gchar           *_busname;
  gchar           *_path;
  GMainContext    *_context;
  GMainLoop       *_loop;
  GDBusNodeInfo   *_introspection_data;
  GDBusConnection *_connection;
  guint            _owner_id;
  guint            _registration_id;
  gboolean         _reconnect;
  guint            _connect_status;
  guint            _disconnect_status;

  void  Connect(void);
  void  Disconnect(void);
  void  RegisterObject(void);
  void  UnregisterObject(void);

protected:
  virtual const gchar  *NodeInfo(void) = 0;
  const gchar  *Busname(void) const { return _busname; };
  const gchar  *Path(void) const { return _path; };

  virtual void  Action(void);
  virtual void  HandleMethodCall(GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *method_name,
                                 GVariant              *parameters,
                                 GDBusMethodInvocation *invocation);

public:
  cDBusObject(const char *Busname, const char *Path);
  virtual ~cDBusObject(void);

  bool  StartMessageHandler(void);
  void  StopMessageHandler(void);
};

#endif
