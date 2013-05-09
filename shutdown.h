#ifndef __DBUS2VDR_SHUTDOWN_H
#define __DBUS2VDR_SHUTDOWN_H

#include "object.h"


class cDBusShutdownHelper;

class cDBusShutdown : public cDBusObject
{
friend class cDBusShutdownHelper;

private:
  static cString  _shutdownHooksDir;
  static cString  _shutdownHooksWrapper;

public:
  static time_t StartupTime;

  static void SetShutdownHooksDir(const char *Dir);
  static void SetShutdownHooksWrapper(const char *Wrapper);

  cDBusShutdown(void);
  virtual ~cDBusShutdown(void);
};

#endif
