#ifndef __DBUS2VDR_PLUGIN_H
#define __DBUS2VDR_PLUGIN_H

#include <dbus/dbus.h>


class cDBus2vdrPlugin
{
public:
  static void SVDRPCommand(DBusMessage* msg, DBusConnection* conn);
  static void Service(DBusMessage* msg, DBusConnection* conn);
};

#endif
