#ifndef __DBUS2VDR_HELPER_H
#define __DBUS2VDR_HELPER_H

#include <gio/gio.h>
#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusTcpAddress
{
private:
  cString _address;

public:
  const cString Host;
  const int     Port;

  cDBusTcpAddress(const char *host, int port)
   :Host(host),Port(port) {}

  const char *Address(void);

  static cDBusTcpAddress *LoadFromFile(const char *Filename);
};

class cDBusHelper
{
private:
  static cString  _pluginConfigDir;

public:
  static void SetConfigDirectory(const char *configDir) { _pluginConfigDir = configDir; };
  static bool IsValidUtf8(const char *text);
  static void ToUtf8(cString &text);

  static void AddKeyValue(GVariantBuilder *Array, const char *Key, const gchar *Type, void **Value);
  static void SendReply(GDBusMethodInvocation *Invocation, int  ReplyCode, const char *ReplyMessage);
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
