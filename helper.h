#ifndef __DBUS2VDR_HELPER_H
#define __DBUS2VDR_HELPER_H

#include <dbus/dbus.h>
#include <gio/gio.h>
#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusTcpAddress;

class cDBusHelper
{
private:
  static cString  _pluginConfigDir;

public:
  static void SetConfigDirectory(const char *configDir) { _pluginConfigDir = configDir; };
  static bool IsValidUtf8(const char *text);
  static void ToUtf8(cString &text);
  static void AddArg(DBusMessageIter &args, int type, const void *value);
  static void AddKeyValue(DBusMessageIter &array, const char *key, int type, const char *vtype, void *value);

  static int  GetNextArg(DBusMessageIter &args, int type, void *value);
   ///< returns -1 on error or type mismatch
   ///<          0 on success and this was the last argument
   ///<          1 on success and there are arguments left

  static void SendReply(DBusConnection *conn, DBusMessage *reply);
  static void SendReply(DBusConnection *conn, DBusMessage *msg, int  returncode, const char *message);
  static void SendReply(DBusConnection *conn, DBusMessage *msg, const char *message);

  static void AddKeyValue(GVariantBuilder *Array, const char *Key, const gchar *Type, void **Value);
  static void SendReply(GDBusMethodInvocation *Invocation, int  ReplyCode, const char *ReplyMessage);

  static cDBusTcpAddress *GetNetworkAddress(void);
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
