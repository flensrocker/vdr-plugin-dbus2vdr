#include "helper.h"

#include <vdr/tools.h>


int  cDBusHelper::GetNextArg(DBusMessageIter& args, int type, void *value)
{
  if (dbus_message_iter_get_arg_type(&args) != type)
     return -1;
  dbus_message_iter_get_basic(&args, value);
  if (dbus_message_iter_next(&args))
     return 1;
  return 0;
}

void  cDBusHelper::SendVoidReply(DBusConnection *conn, DBusMessage *msg)
{
  if ((conn == NULL) || (msg == NULL))
     return;
  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: out of memory while sending the reply");
  dbus_message_unref(reply);
}
