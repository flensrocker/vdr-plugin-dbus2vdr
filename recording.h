#ifndef __DBUS2VDR_RECORDING_H
#define __DBUS2VDR_RECORDING_H

#include "message.h"


class cDBusDispatcherRecording : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherRecording(void);
  virtual ~cDBusDispatcherRecording(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
