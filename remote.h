#ifndef __DBUS2VDR_REMOTE_H
#define __DBUS2VDR_REMOTE_H

#include "object.h"

#include <vdr/osdbase.h>


class cDBusRemote : public cDBusObject
{
public:
  static cOsdObject *MainMenuAction;

  cDBusRemote(void);
  virtual ~cDBusRemote(void);
};

#endif
