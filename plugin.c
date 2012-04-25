#include "plugin.h"
#include "common.h"
#include "helper.h"
#include "monitor.h"

#include <vdr/plugin.h>


cDBusMessagePlugin::cDBusMessagePlugin(cDBusMessagePlugin::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessagePlugin::~cDBusMessagePlugin(void)
{
}

void cDBusMessagePlugin::Process(void)
{
  switch (_action) {
    case dmpSVDRPCommand:
      SVDRPCommand();
      break;
    case dmpService:
      Service();
      break;
    }
}

void cDBusMessagePlugin::SVDRPCommand(void)
{
  const char *pluginName = dbus_message_get_path(_msg);
  const char *command = NULL;
  const char *option = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.SVDRPCommand: message misses an argument for the command", DBUS_VDR_PLUGIN_INTERFACE);
  else {
     int rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &command);
     if (rc < 0)
        esyslog("dbus2vdr: %s.SVDRPCommand: 'command' argument is not a string", DBUS_VDR_PLUGIN_INTERFACE);
     else if (rc == 0)
        isyslog("dbus2vdr: %s.SVDRPCommand: command '%s' has no option", DBUS_VDR_PLUGIN_INTERFACE, command);
     else if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &option) < 0)
        esyslog("dbus2vdr: %s.SVDRPCommand: 'option' argument is not string", DBUS_VDR_PLUGIN_INTERFACE);
     }

  dbus_int32_t replyCode = 500;
  cString replyMessage;
  if ((pluginName != NULL) && (command != NULL)) {
     if ((strlen(pluginName) > 9) && (strncmp(pluginName, "/Plugins/", 9) == 0)) {
        cPlugin *plugin = cPluginManager::GetPlugin(pluginName + 9);
        if (plugin != NULL) {
           if (option == NULL)
              option = "";
           isyslog("dbus2vdr: invoking %s.SVDRPCommand(\"%s\", \"%s\")", plugin->Name(), command, option);
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

  DBusMessage *reply = dbus_message_new_method_return(_msg);
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &replyCode))
     esyslog("dbus2vdr: %s.SVDRPCommand: out of memory while appending the reply-code", DBUS_VDR_PLUGIN_INTERFACE);

  if (*replyMessage == NULL)
     replyMessage = "";
  const char *message = *replyMessage;
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
     esyslog("dbus2vdr: %s.SVDRPCommand: out of memory while appending the reply-message", DBUS_VDR_PLUGIN_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.SVDRPCommand: out of memory while sending the reply", DBUS_VDR_PLUGIN_INTERFACE);
  dbus_message_unref(reply);
}

void cDBusMessagePlugin::Service(void)
{
  const char *pluginName = dbus_message_get_path(_msg);
  const char *id = NULL;
  const char *data = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.Service: message misses an argument for the id", DBUS_VDR_PLUGIN_INTERFACE);
  else {
     int rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &id);
     if (rc < 0)
        esyslog("dbus2vdr: %s.Service: 'id' argument is not a string", DBUS_VDR_PLUGIN_INTERFACE);
     else if (rc == 0)
        isyslog("dbus2vdr: %s.Service: id '%s' has no data", DBUS_VDR_PLUGIN_INTERFACE, id);
     else if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &data) < 0)
        esyslog("dbus2vdr: %s.Service: 'data' argument is not string", DBUS_VDR_PLUGIN_INTERFACE);
     }

  if ((pluginName != NULL) && (id != NULL)) {
     if ((strlen(pluginName) > 9) && (strncmp(pluginName, "/Plugins/", 9) == 0)) {
        cPlugin *plugin = cPluginManager::GetPlugin(pluginName + 9);
        if (plugin != NULL) {
           if (data == NULL)
              data = "";
           isyslog("dbus2vdr: invoking %s.Service(\"%s\", \"%s\")", plugin->Name(), id, data);
           if (!plugin->Service(id, (void*)data)) {
              cString message = cString::sprintf("%s.Service(\"%s\", \"%s\") returns false", plugin->Name(), id, data);
              esyslog("dbus2vdr: %s", *message);
              cDBusHelper::SendReply(_conn, _msg, -1, *message);
              return;
              }
           }
        }
     }

  cDBusHelper::SendReply(_conn, _msg, 0, "");
}


cDBusDispatcherPlugin::cDBusDispatcherPlugin(void)
:cDBusMessageDispatcher(DBUS_VDR_PLUGIN_INTERFACE)
{
}

cDBusDispatcherPlugin::~cDBusDispatcherPlugin(void)
{
}

void cDBusDispatcherPlugin::SendUpstartSignals(const char *action)
{
  int i = 0;
  const char *signal = "vdr-plugin";
  DBusMessageIter args;
  DBusMessageIter array;
  cString tmpPlugin;
  const char *p;
  cString tmpAction = cString::sprintf("ACTION=%s", action);
  const char *a = *tmpAction;
  int nowait = 1;
  do {
     cPlugin *plugin = cPluginManager::GetPlugin(i);
     if (plugin == NULL)
        break;
     bool msgError = true;
     DBusMessage *msg = dbus_message_new_signal("/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", "EmitEvent");
     if (msg != NULL) {
        dbus_message_iter_init_append(msg, &args);
        if (dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &signal)) {
           if (dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array)) {
              tmpPlugin = cString::sprintf("PLUGIN=%s", plugin->Name());
              p = *tmpPlugin;
              if (dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &p)) {
                 if (dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &a)) {
                    if (dbus_message_iter_close_container(&args, &array)) {
                       if (dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &nowait)) {
                          if (cDBusMonitor::SendSignal(msg)) {
                             msg = NULL;
                             msgError = false;
                             isyslog("dbus2vdr: send %s-signal for plugin %s", action, plugin->Name());
                             }
                          }
                       }
                    }
                 }
              }
           }
        if (msg != NULL)
           dbus_message_unref(msg);
        }
     if (msgError)
        esyslog("dbus2vdr: can't send %s-signal for plugin %s", action, plugin->Name());
     i++;
     } while (true);
}

cDBusMessage *cDBusDispatcherPlugin::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_PLUGIN_INTERFACE, "SVDRPCommand"))
     return new cDBusMessagePlugin(cDBusMessagePlugin::dmpSVDRPCommand, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_PLUGIN_INTERFACE, "Service"))
     return new cDBusMessagePlugin(cDBusMessagePlugin::dmpService, conn, msg);

  return NULL;
}

bool          cDBusDispatcherPlugin::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strncmp(dbus_message_get_path(msg), "/Plugins/", 9) != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_PLUGIN_INTERFACE"\">\n"
  "    <method name=\"SVDRPCommand\">\n"
  "      <arg name=\"command\"      type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Service\">\n"
  "      <arg name=\"id\"           type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"data\"         type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <signal name=\"Started\">\n"
  "      <arg name=\"name\"  type=\"s\"/>\n"
  "    </signal>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
