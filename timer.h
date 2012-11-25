#ifndef __DBUS2VDR_TIMER_H
#define __DBUS2VDR_TIMER_H

#include "message.h"


class cDBusDispatcherTimer : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherTimer(void);
  virtual ~cDBusDispatcherTimer(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
