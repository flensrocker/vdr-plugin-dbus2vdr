#ifndef __DBUS2VDR_SHUTDOWN_H
#define __DBUS2VDR_SHUTDOWN_H

#include "message.h"


class cDBusMessageShutdown : public cDBusMessage
{
friend class cDBusDispatcherShutdown;

public:
  enum eAction { dmsConfirmShutdown };

  static void SetShutdownHooksDir(const char *Dir);

  virtual ~cDBusMessageShutdown(void);

protected:
  virtual void Process(void);

private:
  static cString  _shutdownHooksDir;

  cDBusMessageShutdown(eAction action, DBusConnection* conn, DBusMessage* msg);
  void ConfirmShutdown(void);

  eAction _action;
};

class cDBusDispatcherShutdown : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherShutdown(void);
  virtual ~cDBusDispatcherShutdown(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
