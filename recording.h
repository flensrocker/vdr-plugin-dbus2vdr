#ifndef __DBUS2VDR_RECORDING_H
#define __DBUS2VDR_RECORDING_H

#include "message.h"
#include "object.h"


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


class cDBusRecordings;

class cDBusRecordingsConst : public cDBusObject
{
friend class cDBusRecordings;

private:
  cDBusRecordingsConst(const char *NodeInfo);

public:
  cDBusRecordingsConst(void);
  virtual ~cDBusRecordingsConst(void);
};

class cDBusRecordings : public cDBusRecordingsConst
{
public:
  cDBusRecordings(void);
  virtual ~cDBusRecordings(void);
};

#endif
