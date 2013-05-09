#ifndef __DBUS2VDR_VDR_H
#define __DBUS2VDR_VDR_H

#include "object.h"


class cDBusVdr : public cDBusObject
{
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

#endif
