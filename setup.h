#ifndef __DBUS2VDR_SETUP_H
#define __DBUS2VDR_SETUP_H

#include "message.h"


class cDBusDispatcherSetup : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherSetup(void);
  virtual ~cDBusDispatcherSetup(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
