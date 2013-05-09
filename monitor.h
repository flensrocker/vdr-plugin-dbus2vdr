#ifndef __DBUS2VDR_MONITOR_H
#define __DBUS2VDR_MONITOR_H

#include "common.h"
#include "bus.h"

#include <dbus/dbus.h>

#include <vdr/thread.h>


class cDBusMonitor : public cThread
{
private:
  static cMutex _mutex;
  static cDBusMonitor *_monitor[2];

  bool _started;
  bool _nameAcquired;
  cDBusBus *_bus;
  eBusType  _type;
  DBusConnection *_conn;

  cDBusMonitor(cDBusBus *bus, eBusType type);

public:
  static int PollTimeoutMs;

  virtual ~cDBusMonitor(void);

  eBusType GetType(void) const { return _type; }

  static void StartMonitor(bool enableNetwork);
  static void StopMonitor(void);

  static bool SendSignal(DBusMessage *msg, eBusType bus);

protected:
  virtual void Action(void);
};

#endif
