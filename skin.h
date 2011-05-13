#ifndef __DBUS2VDR_SKIN_H
#define __DBUS2VDR_SKIN_H

#include "message.h"


class cDBusMessageSkin : public cDBusMessage
{
friend class cDBusDispatcherSkin;

public:
  enum eAction { dmsQueueMessage };

  virtual ~cDBusMessageSkin(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageSkin(eAction action, DBusConnection* conn, DBusMessage* msg);
  void QueueMessage(void);

  eAction _action;
};

class cDBusDispatcherSkin : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherSkin(void);
  virtual ~cDBusDispatcherSkin(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
