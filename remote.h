#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "message.h"

#include <vdr/osdbase.h>


class cDBusDispatcherRemote : public cDBusMessageDispatcher
{
public:
  static cOsdObject *MainMenuAction;

  cDBusDispatcherRemote(void);
  virtual ~cDBusDispatcherRemote(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
  virtual void          OnStop(void);
};

#endif
