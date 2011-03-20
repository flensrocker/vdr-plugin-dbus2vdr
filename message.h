#ifndef __DBUS2VDR_MESSAGE_H
#define __DBUS2VDR_MESSAGE_H

#include <dbus/dbus.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusMessage;

class cDBusMessageHandler : public cThread
{
public:
  cDBusMessageHandler(void);
  virtual ~cDBusMessageHandler(void);

protected:
  virtual void Action(void);
};

class cDBusMessage : public cListObject
{
friend class cDBusMessageHandler;

public:
  static void Dispatch(cDBusMessage *msg);
  static void StopMessageQueue(void);

  virtual ~cDBusMessage();

protected:
  cDBusMessage(DBusConnection *conn, DBusMessage *msg);
  virtual void Process(void) = 0;

  DBusConnection *_conn;
  DBusMessage    *_msg;
};

#endif
