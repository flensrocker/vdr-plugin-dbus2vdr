#ifndef __DBUS2VDR_MONITOR_H
#define __DBUS2VDR_MONITOR_H

#include <vdr/thread.h>


class cDBusMonitor : public cThread
{
private:
  static cDBusMonitor *_monitor;

  cDBusMonitor(void);

public:
  virtual ~cDBusMonitor(void);

  static void StartMonitor(void);
  static void StopMonitor(void);

protected:
  virtual void Action(void);
};

#endif
