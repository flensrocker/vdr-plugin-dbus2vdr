#include "plugin.h"

#include <vdr/plugin.h>


void cDBus2vdrPlugin::SVDRPCommand(DBusMessage* msg, DBusConnection* conn)
{
  static char empty = 0;
  DBusMessageIter args;
  const char *pluginName = dbus_message_get_path(msg);
  char *command = NULL;
  char *option = NULL;
  if (!dbus_message_iter_init(msg, &args))
     esyslog("dbus2vdr: SVDRPCommand: message misses a command");
  else {
     if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
        esyslog("dbus2vdr: SVDRPCommand: 'command' argument is not a string");
     else
        dbus_message_iter_get_basic(&args, &command);
     if (!dbus_message_iter_next(&args))
        isyslog("dbus2vdr: SVDRPCommand: command '%s' has no option", command);
     else {
        if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING)
           esyslog("dbus2vdr: SVDRPCommand: 'option' argument is not string");
        else
           dbus_message_iter_get_basic(&args, &option);
        }
     }

  dbus_int32_t replyCode = 500;
  cString replyMessage;
  if ((pluginName != NULL) && (command != NULL)) {
     if ((strlen(pluginName) > 9) && (strncmp(pluginName, "/Plugins/", 9) == 0)) {
        cPlugin *plugin = cPluginManager::GetPlugin(pluginName + 9);
        if (plugin != NULL) {
           if (option == NULL)
              option = &empty;
           isyslog("dbus2vdr: invoking %s %s", command, option);
           replyCode = 900;
           cString s = plugin->SVDRPCommand(command, option, replyCode);
           if (*s) {
              replyMessage = s;
              }
           else {
              replyCode = 500;
              replyMessage = cString::sprintf("Command unrecognized: \"%s\"", command);
              }
           }
        }
     }

  DBusMessage *reply = dbus_message_new_method_return(msg);
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &replyCode))
     esyslog("dbus2vdr: SVDRPCommand: out of memory!"); 

  if (*replyMessage == NULL)
     replyMessage = &empty;
  const char *message = *replyMessage;
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
     esyslog("dbus2vdr: SVDRPCommand: out of memory!"); 

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SVDRPCommand: out of memory!"); 
  dbus_connection_flush(conn);
  dbus_message_unref(reply);
}

void cDBus2vdrPlugin::Service(DBusMessage* msg, DBusConnection* conn)
{
}
