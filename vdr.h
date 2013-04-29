#ifndef __DBUS2VDR_VDR_H
#define __DBUS2VDR_VDR_H

#include "message.h"


class cDBusDispatcherVdr : public cDBusMessageDispatcher
{
public:
  enum eVdrStatus { statusUnknown = -1, statusStart = 0, statusReady = 1, statusStop = 2};

  static void       SetStatus(eVdrStatus Status);
  static eVdrStatus GetStatus(void);

  cDBusDispatcherVdr(void);
  virtual ~cDBusDispatcherVdr(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);

private:
  static eVdrStatus  _status;
};

#endif
