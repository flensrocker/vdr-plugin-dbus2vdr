#include "upstart.h"

#include <vdr/plugin.h>


void cDBusUpstart::EmitPluginEvent(cDBusConnection *Connection, const char *Action)
{
  if ((Connection == NULL) || (Action == NULL) || (*Action == 0))
     return;
  GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(sasb)"));
  g_variant_builder_add(builder, "s", "vdr-plugin");
  GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("as"));
  cPlugin *plugin;
  for (int i = 0; (plugin = cPluginManager::GetPlugin(i)) != NULL; i++)
      g_variant_builder_add(array, "s", *cString::sprintf("%s=%s", plugin->Name(), Action));
  g_variant_builder_add_value(builder, g_variant_builder_end(array));
  g_variant_builder_add(builder, "b", FALSE);

  Connection->CallMethod(new cDBusConnection::cDBusMethodCall("com.ubuntu.Upstart", "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", "EmitEvent", g_variant_builder_end(builder)));

  g_variant_builder_unref(array);
  g_variant_builder_unref(builder);
}
