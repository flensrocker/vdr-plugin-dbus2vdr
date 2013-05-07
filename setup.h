#ifndef __DBUS2VDR_SETUP_H
#define __DBUS2VDR_SETUP_H

#include "message.h"
#include "object.h"


class cDBusDispatcherSetup : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherSetup(void);
  virtual ~cDBusDispatcherSetup(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};


class cDBusSetup : public cDBusObject
{
public:
  cDBusSetup(void);
  virtual ~cDBusSetup(void);
};

#endif
