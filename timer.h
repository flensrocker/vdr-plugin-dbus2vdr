#ifndef __DBUS2VDR_TIMER_H
#define __DBUS2VDR_TIMER_H

#include "object.h"


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
