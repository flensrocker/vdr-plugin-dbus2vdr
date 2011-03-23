#ifndef __DBUS2VDR_MESSAGE_H
#define __DBUS2VDR_MESSAGE_H

#include <dbus/dbus.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusMessage;
class cDBusMessageHandler;

class cDBusMessageDispatcher : public cListObject
{
private:
  static cList<cDBusMessageDispatcher> _dispatcher;

  const char *_interface;

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg) = 0;

public:
  static bool Dispatch(DBusConnection* conn, DBusMessage* msg);
  static void Shutdown(void);

  cDBusMessageDispatcher(const char *interface);
  virtual ~cDBusMessageDispatcher(void);
};

class cDBusMessage
{
friend class cDBusMessageHandler;

public:
  virtual ~cDBusMessage();

protected:
  cDBusMessage(DBusConnection *conn, DBusMessage *msg);
  virtual void Process(void) = 0;

  DBusConnection *_conn;
  DBusMessage    *_msg;
};

#endif
