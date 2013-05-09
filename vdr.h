#ifndef __DBUS2VDR_VDR_H
#define __DBUS2VDR_VDR_H

#include "message.h"
#include "object.h"


class cDBusDispatcherVdr;

class cDBusVdr : public cDBusObject
{
friend class cDBusDispatcherVdr;

public:
  enum eVdrStatus { statusUnknown = -1, statusStart = 0, statusReady = 1, statusStop = 2};

  static bool       SetStatus(eVdrStatus Status);
  static eVdrStatus GetStatus(void);

  cDBusVdr(void);
  virtual ~cDBusVdr(void);

private:
  static eVdrStatus         _status;
  static cVector<cDBusVdr*> _objects;
  static cMutex             _objectsMutex;
};


class cDBusDispatcherVdr : public cDBusMessageDispatcher
{
public:
  static void       SendStatus(cDBusVdr::eVdrStatus Status);

  cDBusDispatcherVdr(void);
  virtual ~cDBusDispatcherVdr(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};


#endif
