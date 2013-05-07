#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "message.h"
#include "object.h"

#include <vdr/osdbase.h>


class cDBusDispatcherRemote : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherRemote(void);
  virtual ~cDBusDispatcherRemote(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};


class cDBusRemote : public cDBusObject
{
public:
  static cOsdObject *MainMenuAction;

  cDBusRemote(void);
  virtual ~cDBusRemote(void);
};

#endif
