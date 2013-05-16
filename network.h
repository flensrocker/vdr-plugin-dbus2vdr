#ifndef __DBUS2VDR_NETWORK_H
#define __DBUS2VDR_NETWORK_H

#include <gio/gio.h>


class cDBusNetwork
{
private:
  static void  on_connect(GObject *source_object, GAsyncResult *res, gpointer user_data);
  static void  on_name_acquired(GDBusConnection *connection, const gchar *name, gpointer user_data);
  static void  on_name_lost(GDBusConnection *connection, const gchar *name, gpointer user_data);
  static void  on_closed(GDBusConnection *connection, gboolean remote_peer_vanished, GError *error, gpointer user_data);

  gchar        *_address;
  GMainContext *_context;
  int           _status;
  GDBusAuthObserver *_auth_obs;
  GDBusConnection *_connection;
  guint _owner_id;
  gulong  _closed_handler;

public:
  cDBusNetwork(const char *Address, GMainContext *Context);
  virtual ~cDBusNetwork(void);

  const char *Name(void) const { return "NetworkHandler"; }
  const char *Address(void) const { return _address; }

  // "Start" is async
  bool  Start(void);
  // "Stop" blocks
  void  Stop(void);
};

#endif
