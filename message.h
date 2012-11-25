#ifndef __DBUS2VDR_MESSAGE_H
#define __DBUS2VDR_MESSAGE_H

#include <dbus/dbus.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusMessage;
class cDBusMessageHandler;

typedef void (*cDBusMessageActionFunc)(DBusConnection* conn, DBusMessage* msg);

class cDBusMessageAction : public cListObject
{
public:
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

  const char *_interface;
  cStringList _paths;
  cList<cDBusMessageAction> _actions;

protected:
  void                  AddPath(const char *path);
  void                  AddAction(cDBusMessageAction *action);
  
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg) { return NULL; };
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data) { return false; }
  virtual void          OnStop(void) {}

public:
  static bool Dispatch(DBusConnection* conn, DBusMessage* msg);
  static bool Introspect(DBusMessage *msg, cString &Data);
  static void Stop(void);
  static void Shutdown(void);

  cDBusMessageDispatcher(const char *interface);
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
