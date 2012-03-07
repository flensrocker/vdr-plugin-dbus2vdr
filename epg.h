#ifndef __DBUS2VDR_EPG_H
#define __DBUS2VDR_EPG_H

#include "message.h"


class cDBusMessageEPG : public cDBusMessage
{
friend class cDBusDispatcherEPG;

public:
  enum eAction { dmeDisableScanner, dmeEnableScanner, dmeClearEPG, dmePutEntry, dmePutFile, dmeNow, dmeNext };
  enum eMode   { dmmAll, dmmPresent, dmmFollowing, dmmAtTime };

  virtual ~cDBusMessageEPG(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageEPG(eAction action, DBusConnection* conn, DBusMessage* msg);
  void DisableScanner(void);
  void EnableScanner(void);
  void ClearEPG(void);
  void PutEntry(void);
  void PutFile(void);
  void GetEntries(eMode mode);

  eAction _action;
};

class cDBusDispatcherEPG : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherEPG(void);
  virtual ~cDBusDispatcherEPG(void);

protected:
  virtual cDBusMessage *CreateMessage(DBusConnection* conn, DBusMessage* msg);
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
