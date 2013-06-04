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

class cDBusNetworkClient : public cListObject
{
private:
  gchar *_name;
  gchar *_host;
  gchar *_address;
  int    _port;

public:
  cDBusNetworkClient(const char *Name, const char *Host, const char *Address, int Port);
  virtual ~cDBusNetworkClient(void);

  const char  *Name() const { return _name; }
  const char  *Host() const { return _host; }
  const char  *Address() const { return _address; }
  int          Port() const { return _port; }

  virtual int Compare(const cListObject &ListObject) const;
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
  cString             _avahi_browser_id;
  cList<cDBusNetworkClient> _clients;

public:
  cDBusNetwork(const char *Busname, const char *Filename, GMainContext *Context);
  virtual ~cDBusNetwork(void);

  const char *Name(void) const { return "NetworkHandler"; }
  const char *AvahiBrowserId(void) const { return *_avahi_browser_id; }

  // "Start" is async
  void  Start(void);
  // "Stop" blocks
  void  Stop(void);
  
  void  AddClient(cDBusNetworkClient *Client);
  void  RemoveClient(const char *Name);
};

#endif
