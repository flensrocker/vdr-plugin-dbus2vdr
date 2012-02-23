#ifndef __DBUS2VDR_HELPER_H
#define __DBUS2VDR_HELPER_H

#include <dbus/dbus.h>
#include <vdr/thread.h>


class cDBusHelper
{
public:
  static void AddArg(DBusMessageIter &args, int type, void *value);
  static void AddKeyValue(DBusMessageIter &array, const char *key, int type, const char *vtype, void *value);

  static int  GetNextArg(DBusMessageIter &args, int type, void *value);
   ///< returns -1 on error or type mismatch
   ///<          0 on success and this was the last argument
   ///<          1 on success and there are arguments left

  static void SendReply(DBusConnection *conn, DBusMessage *msg, int  returncode, const char *message);
  static void SendReply(DBusConnection *conn, DBusMessage *msg, const char *message);
};

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
