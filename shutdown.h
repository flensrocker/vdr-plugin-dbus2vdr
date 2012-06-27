#ifndef __DBUS2VDR_SHUTDOWN_H
#define __DBUS2VDR_SHUTDOWN_H

#include "message.h"


class cDBusMessageShutdown : public cDBusMessage
{
friend class cDBusDispatcherShutdown;

public:
  enum eAction { dmsConfirmShutdown, dmsManualStart };

  static void SetShutdownHooksDir(const char *Dir);
  static void SetShutdownHooksWrapper(const char *Wrapper);

  virtual ~cDBusMessageShutdown(void);

protected:
  virtual void Process(void);

private:
  static cString  _shutdownHooksDir;
  static cString  _shutdownHooksWrapper;

  cDBusMessageShutdown(eAction action, DBusConnection* conn, DBusMessage* msg);
  void ConfirmShutdown(void);
  void ManualStart(void);

  eAction _action;
};

class cDBusDispatcherShutdown : public cDBusMessageDispatcher
{
public:
  static time_t StartupTime;

  cDBusDispatcherShutdown(void);
  virtual ~cDBusDispatcherShutdown(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
