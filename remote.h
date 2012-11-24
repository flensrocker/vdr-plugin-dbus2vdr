#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "message.h"

#include <vdr/osdbase.h>


class cDBusMessageRemote : public cDBusMessage
{
friend class cDBusDispatcherRemote;

public:
  enum eAction { dmrCallPlugin, dmrEnable, dmrDisable, dmrStatus, dmrHitKey, dmrAskUser, dmrSwitchChannel };

  virtual ~cDBusMessageRemote(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageRemote(eAction action, DBusConnection* conn, DBusMessage* msg);
  void CallPlugin(void);
  void Enable(void);
  void Disable(void);
  void Status(void);
  void HitKey(void);
  void AskUser(void);
  void SwitchChannel(void);

  eAction _action;
};

class cDBusDispatcherRemote : public cDBusMessageDispatcher
{
public:
  static cOsdObject *MainMenuAction;

  cDBusDispatcherRemote(void);
  virtual ~cDBusDispatcherRemote(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
  virtual void          OnStop(void);
};

#endif
