#ifndef __DBUS2VDR_TIMER_H
#define __DBUS2VDR_TIMER_H

#include "message.h"


class cDBusDispatcherTimerConst : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherTimerConst(eBusType type);
  virtual ~cDBusDispatcherTimerConst(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

class cDBusDispatcherTimer : public cDBusDispatcherTimerConst
{
public:
  cDBusDispatcherTimer(void);
  virtual ~cDBusDispatcherTimer(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
