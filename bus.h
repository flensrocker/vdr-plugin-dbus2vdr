#ifndef __DBUS2VDR_BUS_H
#define __DBUS2VDR_BUS_H

#include <dbus/dbus.h>

#include <vdr/plugin.h>
#include <vdr/tools.h>

// abstraction of a DBus message bus

class cDBusBus
{
private:
  cString          _name;
  cString          _busname;
  DBusConnection  *_conn;

protected:
  DBusError  _err;

  virtual DBusConnection *GetConnection(void) = 0;
  virtual void OnConnect(void) {}
  virtual void OnDisconnect(DBusConnection *conn) {}

public:
  cDBusBus(const char *name, const char *busname);
  virtual ~cDBusBus(void);

  const char *Name(void) const { return *_name; }
  const char *Busname(void) const { return *_busname; }

  DBusConnection*  Connect(void);
  bool  Disconnect(void);
};

class cDBusSystemBus : public cDBusBus
{
protected:
  virtual DBusConnection *GetConnection(void);
  virtual void OnDisconnect(DBusConnection *conn);

public:
  cDBusSystemBus(const char *busname);
  virtual ~cDBusSystemBus(void);
};

class cDBusTcpAddress
{
private:
  cString _address;

public:
  const cString Host;
  const int     Port;

  cDBusTcpAddress(const char *host, int port)
   :Host(host),Port(port) {}

  const char *Address(void)
  {
    if (*_address == NULL) {
       char *host = dbus_address_escape_value(*Host);
       _address = cString::sprintf("tcp:host=%s,port=%d", host, Port);
       dbus_free(host);
       }
    return *_address;
  }
};

class cDBusNetworkBus : public cDBusBus
{
private:
  cDBusTcpAddress  *_address;
  cPlugin          *_avahi4vdr;
  cString           _avahi_name;
  cString           _avahi_id;

protected:
  virtual DBusConnection *GetConnection(void);
  virtual void OnConnect(void);
  virtual void OnDisconnect(DBusConnection *conn);

public:
  cDBusNetworkBus(const char *busname);
  virtual ~cDBusNetworkBus(void);
};

#endif
