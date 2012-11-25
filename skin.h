#ifndef __DBUS2VDR_SKIN_H
#define __DBUS2VDR_SKIN_H

#include "message.h"


class cDBusDispatcherSkin : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherSkin(void);
  virtual ~cDBusDispatcherSkin(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
