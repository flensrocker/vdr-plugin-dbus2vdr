#ifndef __DBUS2VDR_DEVICE_H
#define __DBUS2VDR_DEVICE_H

#include "object.h"
#include <vdr/thread.h>

class cDBusDevices : public cDBusObject
{
private:
  static cMutex _requestedPrimaryDeviceMutex;
  static int _requestedPrimaryDevice;

public:
  static int _nulldeviceIndex;
  static void SetPrimaryDeviceRequest(int index);
  static int RequestedPrimaryDevice(bool clear);

  cDBusDevices(void);
  virtual ~cDBusDevices(void);
};

#endif
