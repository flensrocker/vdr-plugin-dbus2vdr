#ifndef __DBUS2VDR_HELPER_H
#define __DBUS2VDR_HELPER_H

#include <dbus/dbus.h>


class cDBusHelper
{
public:
  static int  GetNextArg(DBusMessageIter &args, int type, void *value);
   ///< returns -1 on error or type mismatch
   ///<          0 on success and this was the last argument
   ///<          1 on success and there are arguments left
  static void SendVoidReply(DBusConnection *conn, DBusMessage *msg);
};
#endif
