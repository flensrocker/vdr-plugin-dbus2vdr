#ifndef __DBUS2VDR_SHUTDOWN_WRAPPER_H
#define __DBUS2VDR_SHUTDOWN_WRAPPER_H

#include "libvdr-tools.h"

class dbus2vdrShutdownWrapper
{
public:
  dbus2vdrShutdownWrapper(const char *Dir);
  virtual ~dbus2vdrShutdownWrapper(void);

  int ConfirmShutdown(const char *Args);
  
protected:

private:
  cString _shutdownHooksDir;
};

#endif
