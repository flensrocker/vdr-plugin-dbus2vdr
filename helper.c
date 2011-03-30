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

void  cDBusHelper::SendReply(DBusConnection *conn, DBusMessage *msg, int  returncode, const char *message)
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &returncode))
     esyslog("dbus2vdr: SendReply: out of memory while appending the return-code");

  if (message != NULL) {
     if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
        esyslog("dbus2vdr: SendReply: out of memory while appending the reply-message");
     }

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
}

void  cDBusHelper::SendReply(DBusConnection *conn, DBusMessage *msg, const char *message)
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (message != NULL) {
     if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
        esyslog("dbus2vdr: SendReply: out of memory while appending the reply-message");
     }

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
}
