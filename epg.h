#ifndef __DBUS2VDR_EPG_H
#define __DBUS2VDR_EPG_H

#include "message.h"


class cDBusMessageEPG : public cDBusMessage
{
friend class cDBusDispatcherEPG;

public:
  enum eAction { dmePutFile, dmeClearEPG };

  virtual ~cDBusMessageEPG(void);

protected:
  virtual void Process(void);

private:
  cDBusMessageEPG(eAction action, DBusConnection* conn, DBusMessage* msg);
  void PutFile(void);
  void ClearEPG(void);

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
