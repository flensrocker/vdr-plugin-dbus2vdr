#ifndef __DBUS2VDR_MESSAGE_H
#define __DBUS2VDR_MESSAGE_H

#include <dbus/dbus.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusMessage;

class cDBusMessageHandler : public cThread, cListObject
{
public:
  virtual ~cDBusMessageHandler(void);

  static void NewHandler(cDBusMessage *msg);
  static void DeleteHandler(void);

protected:
  virtual void Action(void);

  cDBusMessage *_msg;

private:
  static cMutex   _listMutex;
  static cCondVar _listCondVar;
  static cList<cDBusMessageHandler> _activeHandler;
  static cList<cDBusMessageHandler> _finishedHandler;

  cDBusMessageHandler(cDBusMessage *msg);
};

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
