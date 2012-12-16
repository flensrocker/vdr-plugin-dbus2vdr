#ifndef __DBUS2VDR_BUS_H
#define __DBUS2VDR_BUS_H

#include <dbus/dbus.h>

#include <vdr/tools.h>

// abstraction of a DBus message bus

class cDBusBus
{
private:
  cString    _busname;
  DBusConnection  *_conn;

protected:
  DBusError  _err;

  virtual DBusConnection *GetConnection(void) = 0;

public:
  cDBusBus(const char *busname);
  virtual ~cDBusBus(void);

  DBusConnection*  Connect(void);
  virtual void  Disconnect(void);
};

class cDBusSystemBus : public cDBusBus
{
protected:
  virtual DBusConnection *GetConnection(void);

public:
  cDBusSystemBus(const char *busname);
  virtual ~cDBusSystemBus(void);
};

class cDBusCustomBus : public cDBusBus
{
private:
  cString   _address;

protected:
  virtual DBusConnection *GetConnection(void);

public:
  cDBusCustomBus(const char *busname, const char *address);
  virtual ~cDBusCustomBus(void);
};

#endif
