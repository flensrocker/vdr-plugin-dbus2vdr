#ifndef __DBUS2VDR_SHUTDOWN_H
#define __DBUS2VDR_SHUTDOWN_H

#include "message.h"


class cDBusShutdownActions
{
public:
  static void SetShutdownHooksDir(const char *Dir);
  static void SetShutdownHooksWrapper(const char *Wrapper);

  static void ConfirmShutdown(DBusConnection* conn, DBusMessage* msg);
  static void ManualStart(DBusConnection* conn, DBusMessage* msg);
  static void SetUserInactive(DBusConnection* conn, DBusMessage* msg);

private:
  static cString  _shutdownHooksDir;
  static cString  _shutdownHooksWrapper;
};

class cDBusDispatcherShutdown : public cDBusMessageDispatcher
{
public:
  static time_t StartupTime;

  cDBusDispatcherShutdown(void);
  virtual ~cDBusDispatcherShutdown(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
