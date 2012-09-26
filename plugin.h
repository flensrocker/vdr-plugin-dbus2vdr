#ifndef __DBUS2VDR_PLUGIN_H
#define __DBUS2VDR_PLUGIN_H

#include "message.h"


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

#endif
