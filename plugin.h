#ifndef __DBUS2VDR_PLUGIN_H
#define __DBUS2VDR_PLUGIN_H

#include "message.h"
#include "object.h"

#include <vdr/plugin.h>


class cDBusMessagePlugin : public cDBusMessage
{
friend class cDBusDispatcherPlugin;

public:
  enum eAction { dmpList, dmpSVDRPCommand, dmpService };

  virtual ~cDBusMessagePlugin(void);

protected:
  virtual void Process(void);

private:
  cDBusMessagePlugin(eAction action, DBusConnection* conn, DBusMessage* msg);
  void List(void);
  void SVDRPCommand(void);
  void Service(void);

  eAction _action;
};

class cDBusDispatcherPlugin : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherPlugin(void);
  virtual ~cDBusDispatcherPlugin(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

class cDBusPlugin : public cDBusObject
{
public:
  cDBusPlugin(const char *Path);
  virtual ~cDBusPlugin(void);

  static void AddAllPlugins(cDBusConnection *Connection);
};

class cDBusPluginManager : public cDBusObject
{
public:
  cDBusPluginManager(void);
  virtual ~cDBusPluginManager(void);
};

#endif
