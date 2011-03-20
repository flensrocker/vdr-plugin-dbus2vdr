#ifndef __DBUS2VDR_PLUGIN_H
#define __DBUS2VDR_PLUGIN_H

#include "message.h"


class cDBusMessagePlugin : public cDBusMessage
{
public:
  static bool Dispatch(DBusConnection* conn, DBusMessage* msg);
  
  virtual ~cDBusMessagePlugin(void);

protected:
  virtual void Process(void);

private:
  enum eAction { dmpSVDRPCommand, dmpService };

  cDBusMessagePlugin(eAction action, DBusConnection* conn, DBusMessage* msg);
  void SVDRPCommand(void);
  void Service(void);

  eAction _action;
};

#endif
