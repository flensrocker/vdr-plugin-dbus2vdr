#ifndef __DBUS2VDR_RECORDING_H
#define __DBUS2VDR_RECORDING_H

#include "message.h"


class cDBusDispatcherRecordingConst : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherRecordingConst(eBusType type);
  virtual ~cDBusDispatcherRecordingConst(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

class cDBusDispatcherRecording : public cDBusDispatcherRecordingConst
{
public:
  cDBusDispatcherRecording(void);
  virtual ~cDBusDispatcherRecording(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
