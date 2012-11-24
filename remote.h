#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "message.h"

#include <vdr/osdbase.h>


class cDBusMessageRemote;
typedef void (cDBusMessageRemote::*cDBusMessageRemoteAction)(void);

class cDBusMessageRemote : public cDBusMessage
{
friend class cDBusDispatcherRemote;

public:
  virtual ~cDBusMessageRemote(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageRemote(cDBusMessageRemoteAction action, DBusConnection* conn, DBusMessage* msg);
  void CallPlugin(void);
  void Enable(void);
  void Disable(void);
  void Status(void);
  void HitKey(void);
  void AskUser(void);
  void SwitchChannel(void);

  cDBusMessageRemoteAction _action;
};

class cDBusDispatcherRemote : public cDBusMessageDispatcher
{
public:
  class cRemoteAction
  {
  public:
    const char *Name;
    cDBusMessageRemoteAction Action;

    cRemoteAction(const char *name, cDBusMessageRemoteAction action)
     :Name(name),Action(action)
    {
    };
  };

  static cOsdObject *MainMenuAction;

  cDBusDispatcherRemote(void);
  virtual ~cDBusDispatcherRemote(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
  virtual void          OnStop(void);

private:
  cRemoteAction **_action;
  int _actionCount;
};

#endif
