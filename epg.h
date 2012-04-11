#ifndef __DBUS2VDR_EPG_H
#define __DBUS2VDR_EPG_H

#include "message.h"


class cDBusMessageEpg : public cDBusMessage
{
friend class cDBusDispatcherEpg;

public:
  enum eAction { dmeDisableScanner, dmeEnableScanner, dmeClearEPG, dmePutEntry, dmePutFile, dmeNow, dmeNext, dmeAt };
  enum eMode   { dmmAll, dmmPresent, dmmFollowing, dmmAtTime };

  virtual ~cDBusMessageEpg(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageEpg(eAction action, DBusConnection* conn, DBusMessage* msg);
  void DisableScanner(void);
  void EnableScanner(void);
  void ClearEPG(void);
  void PutEntry(void);
  void PutFile(void);
  void GetEntries(eMode mode);

  eAction _action;
};

class cDBusDispatcherEpg : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherEpg(void);
  virtual ~cDBusDispatcherEpg(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
