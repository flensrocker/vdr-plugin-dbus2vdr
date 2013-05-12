#include "plugin.h"
#include "common.h"
#include "connection.h"
#include "helper.h"

#include <vdr/plugin.h>


namespace cDBusPluginsHelper
{
  static const char *_xmlNodeInfoManager = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_PLUGINMANAGER_INTERFACE"\">\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"pluginlist\"   type=\"a(ss)\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static const char *_xmlNodeInfoPlugin = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_PLUGIN_INTERFACE"\">\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"pluginlist\"   type=\"a(ss)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"SVDRPCommand\">\n"
    "      <arg name=\"command\"      type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Service\">\n"
    "      <arg name=\"id\"           type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"data\"         type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"return\"       type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <signal name=\"Started\">\n"
    "    </signal>\n"
    "  </interface>\n"
    "</node>\n";

  static void List(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
#define EMPTY(s) (s == NULL ? "" : s)
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(ss)"));
    int index = 0;
    do
    {
      cPlugin *plugin = cPluginManager::GetPlugin(index);
      if (plugin == NULL)
         break;
      const char *name = plugin->Name();
      const char *version = plugin->Version();
      g_variant_builder_add(array, "(ss)", EMPTY(name), EMPTY(version));
      index++;
    } while (true);
#undef EMPTY

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a(ss))"));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  }

  static void SVDRPCommand(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *pluginName = Object->Path();
    const char *command = NULL;
    const char *option = NULL;
    g_variant_get(Parameters, "(&s&s)", &command, &option);

    gint32  replyCode = 500;
    cString replyMessage;
    if ((pluginName != NULL) && (command != NULL)) {
       if ((strlen(pluginName) > 9) && (strncmp(pluginName, "/Plugins/", 9) == 0)) {
          cPlugin *plugin = cPluginManager::GetPlugin(pluginName + 9);
          if (plugin != NULL) {
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

    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(is)", replyCode, *replyMessage));
  }

  static void Service(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gboolean reply = FALSE;
    const char *pluginName = Object->Path();
    const char *id = NULL;
    const char *data = NULL;
    g_variant_get(Parameters, "(&s&s)", &id, &data);

    if ((pluginName != NULL) && (id != NULL)) {
       if ((strlen(pluginName) > 9) && (strncmp(pluginName, "/Plugins/", 9) == 0)) {
          cPlugin *plugin = cPluginManager::GetPlugin(pluginName + 9);
          if (plugin != NULL) {
             if (data == NULL)
                data = "";
             if (plugin->Service(id, (void*)data))
                reply = TRUE;
             }
          }
       }

    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(b)", reply));
  }
}

cDBusPlugin::cDBusPlugin(const char *Path)
:cDBusObject(Path, cDBusPluginsHelper::_xmlNodeInfoPlugin)
{
  AddMethod("List", cDBusPluginsHelper::List);
  AddMethod("SVDRPCommand", cDBusPluginsHelper::SVDRPCommand);
  AddMethod("Service", cDBusPluginsHelper::Service);
}

cDBusPlugin::~cDBusPlugin(void)
{
}

void cDBusPlugin::AddAllPlugins(cDBusConnection *Connection)
{
  int index = 0;
  do
  {
    cPlugin *plugin = cPluginManager::GetPlugin(index);
    if (plugin == NULL)
       break;
    gchar *path = g_strconcat("/Plugins/", plugin->Name(), NULL);
    for (int i = 0; path[i] != 0; i++) {
        if (path[i] == '-')
           path[i] = '_';
        }
    Connection->AddObject(new cDBusPlugin(path));
    g_free(path);
    index++;
  } while (true);
}


cDBusPluginManager::cDBusPluginManager(void)
:cDBusObject("/Plugins", cDBusPluginsHelper::_xmlNodeInfoManager)
{
  AddMethod("List", cDBusPluginsHelper::List);
}

cDBusPluginManager::~cDBusPluginManager(void)
{
}
