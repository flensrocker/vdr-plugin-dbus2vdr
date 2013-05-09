#ifndef __DBUS2VDR_PLUGIN_H
#define __DBUS2VDR_PLUGIN_H

#include "object.h"


class cDBusPlugin : public cDBusObject
{
public:
  cDBusPlugin(const char *Path);
  virtual ~cDBusPlugin(void);

  static void AddAllPlugins(cDBusConnection *Connection);
};

class cDBusPluginManager : public cDBusObject
{
public:
  cDBusPluginManager(void);
  virtual ~cDBusPluginManager(void);
};

#endif
