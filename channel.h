#ifndef __DBUS2VDR_CHANNEL_H
#define __DBUS2VDR_CHANNEL_H

#include "message.h"


class cDBusDispatcherChannel : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherChannel(void);
  virtual ~cDBusDispatcherChannel(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
