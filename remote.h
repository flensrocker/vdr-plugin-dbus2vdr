#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "message.h"


class cDBusMessageRemote : public cDBusMessage
{
friend class cDBusDispatcherRemote;

public:
  enum eAction { dmrEnable, dmrDisable, dmrStatus, dmrHitKey };

  virtual ~cDBusMessageRemote(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageRemote(eAction action, DBusConnection* conn, DBusMessage* msg);
  void Enable(void);
  void Disable(void);
  void Status(void);
  void HitKey(void);

  eAction _action;
};

class cDBusDispatcherRemote : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherRemote(void);
  virtual ~cDBusDispatcherRemote(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
