#ifndef __DBUS2VDR_MESSAGE_H
#define __DBUS2VDR_MESSAGE_H

#include "common.h"

#include <dbus/dbus.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusMessage;
class cDBusMessageDispatcher;
class cDBusMessageHandler;
class cDBusMonitor;

typedef void (*cDBusMessageActionFunc)(DBusConnection* conn, DBusMessage* msg);

class cDBusMessageAction : public cListObject
{
friend class cDBusMessageDispatcher;

private:
  const char *Name;
  cDBusMessageActionFunc Action;

  cDBusMessageAction(const char *name, cDBusMessageActionFunc action)
   :Name(name),Action(action)
  {
  };
};

class cDBusMessageDispatcher : public cListObject
{
private:
  static cList<cDBusMessageDispatcher> _dispatcher;

  eBusType    _busType;
  const char *_interface;
  cStringList _paths;
  cList<cDBusMessageAction> _actions;

protected:
  void                  AddPath(const char *path);
  void                  AddAction(const char *name, cDBusMessageActionFunc action);
  
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg) { return NULL; };
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data) { return false; }
  virtual void          OnStop(void) {}

public:
  static bool Dispatch(cDBusMonitor *monitor, DBusConnection* conn, DBusMessage* msg);
  static bool Introspect(DBusMessage *msg, cString &Data);
  static void Stop(void);
  static void Shutdown(void);

  cDBusMessageDispatcher(eBusType busType, const char *interface);
  virtual ~cDBusMessageDispatcher(void);
};

class cDBusMessage
{
friend class cDBusMessageDispatcher;
friend class cDBusMessageHandler;

public:
  virtual ~cDBusMessage();

protected:
  cDBusMessage(DBusConnection *conn, DBusMessage *msg);
  cDBusMessage(DBusConnection *conn, DBusMessage *msg, cDBusMessageActionFunc action);
  virtual void Process(void);

  DBusConnection *_conn;
  DBusMessage    *_msg;

private:
  cDBusMessageActionFunc _action;
};

#endif
