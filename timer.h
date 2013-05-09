#ifndef __DBUS2VDR_TIMER_H
#define __DBUS2VDR_TIMER_H

#include "message.h"
#include "object.h"


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


class cDBusTimers;

class cDBusTimersConst : public cDBusObject
{
friend class cDBusTimers;

private:
  cDBusTimersConst(const char *NodeInfo);

public:
  cDBusTimersConst(void);
  virtual ~cDBusTimersConst(void);
};

class cDBusTimers : public cDBusTimersConst
{
public:
  cDBusTimers(void);
  virtual ~cDBusTimers(void);
};

#endif
