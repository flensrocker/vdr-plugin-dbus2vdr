#ifndef __DBUS2VDR_CONNECTION_H
#define __DBUS2VDR_CONNECTION_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusObject;

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

  // must be called before "Start"
  void  AddObject(cDBusObject *Object);
};

#endif
