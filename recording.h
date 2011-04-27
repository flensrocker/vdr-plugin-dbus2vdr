#ifndef __DBUS2VDR_RECORDING_H
#define __DBUS2VDR_RECORDING_H

#include "message.h"


class cDBusMessageRecording : public cDBusMessage
{
friend class cDBusDispatcherRecording;

public:
  enum eAction { dmrUpdate };

  virtual ~cDBusMessageRecording(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageRecording(eAction action, DBusConnection* conn, DBusMessage* msg);
  void Update(void);

  eAction _action;
};

class cDBusDispatcherRecording : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherRecording(void);
  virtual ~cDBusDispatcherRecording(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
