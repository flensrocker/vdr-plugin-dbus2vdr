#ifndef __DBUS2VDR_EPG_H
#define __DBUS2VDR_EPG_H

#include "message.h"
#include "object.h"


class cDBusDispatcherEpg : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherEpg(void);
  virtual ~cDBusDispatcherEpg(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};


class cDBusEpg : public cDBusObject
{
public:
  cDBusEpg(void);
  virtual ~cDBusEpg(void);
};

#endif
