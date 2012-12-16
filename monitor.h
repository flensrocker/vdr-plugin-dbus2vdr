#ifndef __DBUS2VDR_MONITOR_H
#define __DBUS2VDR_MONITOR_H

#include "bus.h"

#include <dbus/dbus.h>

#include <vdr/thread.h>


class cDBusMonitor : public cThread
{
public:
  enum eBusType { busSystem = 0, busNetwork = 1};

private:
  static cMutex _mutex;
  static cDBusMonitor *_monitor[2];

  bool _started;
  bool _nameAcquired;
  cDBusBus *_bus;
  DBusConnection *_conn;

  cDBusMonitor(cDBusBus *bus);

public:
  static int PollTimeoutMs;

  virtual ~cDBusMonitor(void);

  static void StartMonitor(void);
  static void StopMonitor(void);

  static bool SendSignal(DBusMessage *msg, eBusType bus);

  static void SendUpstartSignal(const char *action);
  static void StopUpstartSender(void);

protected:
  virtual void Action(void);
};

#endif
