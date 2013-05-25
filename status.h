#ifndef __DBUS2VDR_STATUS_H
#define __DBUS2VDR_STATUS_H

#include "object.h"

#include <vdr/status.h>


namespace cDBusStatusHelper
{
  class cVdrStatus;
}


class cDBusStatus : public cDBusObject
{
private:
  cDBusStatusHelper::cVdrStatus *_status;

public:
  cDBusStatus(bool Network);
  virtual ~cDBusStatus(void);
};

#endif
