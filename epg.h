#ifndef __DBUS2VDR_EPG_H
#define __DBUS2VDR_EPG_H

#include "message.h"


class cDBusDispatcherEpg : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherEpg(void);
  virtual ~cDBusDispatcherEpg(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
