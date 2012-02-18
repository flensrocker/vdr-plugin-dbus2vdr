#ifndef __DBUS2VDR_EXITPIPE_H
#define __DBUS2VDR_EXITPIPE_H

#include "libvdr-tools.h"

// copy of vdr's cPipe but returns exit code of child on Close
class cExitPipe
{
private:
  pid_t pid;
  FILE *f;
public:
  cExitPipe(void);
  ~cExitPipe();
  operator FILE* () { return f; }
  bool Open(const char *Command, const char *Mode);
  int Close(void);
};
#endif
