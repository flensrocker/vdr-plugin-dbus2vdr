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

class cDBusMessage : public cListObject
{
friend class cDBusMessageHandler;

public:
  static void Dispatch(cDBusMessage *msg);
  static void StopDispatcher(void);

  virtual ~cDBusMessage();

protected:
  cDBusMessage(DBusConnection *conn, DBusMessage *msg);
  virtual void Process(void) = 0;

  DBusConnection *_conn;
  DBusMessage    *_msg;
};

#endif
