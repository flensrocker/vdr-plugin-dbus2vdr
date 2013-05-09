#ifndef __DBUS2VDR_UPSTART_H
#define __DBUS2VDR_UPSTART_H

#include "connection.h"


class cDBusUpstart
{
public:
  static void EmitPluginEvent(cDBusConnection *Connection, const char *Action);
};

#endif
