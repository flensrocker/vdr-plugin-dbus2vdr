#ifndef __DBUS2VDR_TIMER_H
#define __DBUS2VDR_TIMER_H

#include "message.h"


class cDBusMessageTimer : public cDBusMessage
{
friend class cDBusDispatcherTimer;

public:
  enum eAction { dmtNext };

  virtual ~cDBusMessageTimer(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageTimer(eAction action, DBusConnection* conn, DBusMessage* msg);
  void Next(void);

  eAction _action;
};

class cDBusDispatcherTimer : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherTimer(void);
  virtual ~cDBusDispatcherTimer(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(cString &Data);
};

#endif
