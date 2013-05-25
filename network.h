#ifndef __DBUS2VDR_NETWORK_H
#define __DBUS2VDR_NETWORK_H

#include <gio/gio.h>

#include <vdr/plugin.h>

#include "connection.h"


class cDBusNetworkAddress
{
private:
  cString _address;

public:
  const cString Host;
  const int     Port;

  cDBusNetworkAddress(const char *host, int port)
   :Host(host),Port(port) {};
  virtual ~cDBusNetworkAddress(void) {};

  const char *Address(void);

  static cDBusNetworkAddress *LoadFromFile(const char *Filename);
};

class cDBusNetwork
{
private:
  static gboolean  do_connect(gpointer user_data);
  static gboolean  do_disconnect(gpointer user_data);
  static void      on_file_changed(GFileMonitor *monitor, GFile *first, GFile *second, GFileMonitorEvent event, gpointer user_data);
  static void      on_name_acquired(cDBusConnection *Connection, gpointer UserData);
  static void      on_name_lost(cDBusConnection *Connection, gpointer UserData);

  gchar               *_busname;
  gchar               *_filename;
  GMainContext        *_context;
  GFileMonitor        *_monitor;
  GMainContext        *_monitor_context;
  cDBusMainLoop       *_monitor_loop;
  gulong               _signal_handler_id;
  cDBusNetworkAddress *_address;
  cDBusConnection     *_connection;

  cPlugin            *_avahi4vdr;
  cString             _avahi_name;
  cString             _avahi_id;

public:
  cDBusNetwork(const char *Busname, const char *Filename, GMainContext *Context);
  virtual ~cDBusNetwork(void);

  const char *Name(void) const { return "NetworkHandler"; }

  // "Start" is async
  void  Start(void);
  // "Stop" blocks
  void  Stop(void);
};

#endif
