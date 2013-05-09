#include "skin.h"
#include "common.h"
#include "helper.h"

#include <vdr/skins.h>


namespace cDBusSkinHelper
{
  static const char *_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_SKIN_INTERFACE"\">\n"
    "    <method name=\"CurrentSkin\">\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"skin\"         type=\"(iss)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"ListSkins\">\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"names\"        type=\"a(iss)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"SetSkin\">\n"
    "      <arg name=\"name\"         type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"QueueMessage\">\n"
    "      <arg name=\"messageText\"  type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static GVariant *BuildSkin(cSkin *skin)
  {
    GVariantBuilder *struc = g_variant_builder_new(G_VARIANT_TYPE("(iss)"));
    gint32 index = -1;
    const char *name = "";
    const char *description = "";
    if (skin != NULL) {
       index = skin->Index();
       name = skin->Name();
       description = skin->Description();
       }
    g_variant_builder_add(struc, "i", index);
    g_variant_builder_add(struc, "s", name);
    g_variant_builder_add(struc, "s", description);
    GVariant *ret = g_variant_builder_end(struc);
    g_variant_builder_unref(struc);
    return ret;
  }

  static void CurrentSkin(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE("(i(iss))"));
    g_variant_builder_add(ret, "i", 900);
    g_variant_builder_add_value(ret, BuildSkin(Skins.Current()));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(ret));
    g_variant_builder_unref(ret);
  };

  static void ListSkins(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    GVariantBuilder *ret = g_variant_builder_new(G_VARIANT_TYPE("(ia(iss))"));
    g_variant_builder_add(ret, "i", 900);
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(iss)"));
    for (cSkin* skin = Skins.First(); skin; skin = Skins.Next(skin))
        g_variant_builder_add_value(array, BuildSkin(skin));
    g_variant_builder_add_value(ret, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(ret));
    g_variant_builder_unref(array);
    g_variant_builder_unref(ret);
  };

  static void SetSkin(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *name = NULL;
    g_variant_get(Parameters, "(&s)", &name);
    gint32 replyCode = 900;
    cString replyMessage = cString::sprintf("set skin to '%s'", name);
    if (*name == 0) {
       replyCode = 501;
       replyMessage = "no skin name given";
       }
    else if (!Skins.SetCurrent(name)) {
       replyCode = 550;
       replyMessage = cString::sprintf("can't set skin to '%s'", name);
       }
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void QueueMessage(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *messageText = NULL;
    g_variant_get(Parameters, "(&s)", &messageText);

    gint32 replyCode = 501;
    cString replyMessage;
    if ((messageText != NULL) && (*messageText != 0)) {
       Skins.QueueMessage(mtInfo, messageText);
       replyCode = 250;
       replyMessage = "Message queued";
       }
    else
       replyMessage = "Missing message";

    cDBusHelper::SendReply(Invocation, replyCode, replyMessage);
  };
}


cDBusSkin::cDBusSkin(void)
:cDBusObject("/Skin", cDBusSkinHelper::_xmlNodeInfo)
{
  AddMethod("CurrentSkin", cDBusSkinHelper::CurrentSkin);
  AddMethod("ListSkins", cDBusSkinHelper::ListSkins);
  AddMethod("SetSkin", cDBusSkinHelper::SetSkin);
  AddMethod("QueueMessage", cDBusSkinHelper::QueueMessage);
}

cDBusSkin::~cDBusSkin(void)
{
}
