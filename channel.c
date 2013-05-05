#include "channel.h"
#include "common.h"
#include "helper.h"

#include <vdr/channels.h>


namespace cDBusChannelsHelper
{
  static const char *_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_CHANNEL_INTERFACE"\">\n"
    "    <method name=\"Count\">\n"
    "      <arg name=\"count\"        type=\"i\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"GetFromTo\">\n"
    "      <arg name=\"from_index\"   type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"to_index\"     type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"channel\"      type=\"a(is)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"List\">\n"
    "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"channel\"      type=\"a(is)\" direction=\"out\"/>\n"
    "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static void AddChannel(GVariantBuilder *Array, cChannel *Channel)
  {
    if (Channel == NULL)
       return;

    gint32 index = Channel->GroupSep() ? 0 : Channel->Number();
    cString text = Channel->ToText();
    int len = strlen(*text);
    if ((len > 0) && ((*text)[len - 1] == '\n'))
       text.Truncate(-1);
    cDBusHelper::ToUtf8(text);

    g_variant_builder_add(Array, "(is)", index, *text);
  }

  static void Count(const gchar *ObjectPath, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(i)", Channels.Count()));
  }

  static void GetFromTo(const gchar *ObjectPath, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gint32 from_index = -1;
    gint32 to_index = -1;
    g_variant_get(Parameters, "(ii)", &from_index, &to_index);

    if (to_index < from_index)
       to_index = from_index;

    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(is)"));

    cChannel *c = Channels.Get(from_index);
    while ((c != NULL) && (to_index >= from_index)) {
       cDBusChannelsHelper::AddChannel(array, c);
       c = Channels.Next(c);
       to_index--;
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a(is))"));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  }

  static void List(const gchar *ObjectPath, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const gchar *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);

    bool withGroupSeps = (g_strcmp0(option, ":groups") == 0);
    gint32 replyCode = 250;
    cString replyMessage = "";
    
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(is)"));

    if ((option != NULL) && *option && !withGroupSeps) {
       if (isnumber(option)) {
          cChannel *channel = Channels.GetByNumber(strtol(option, NULL, 10));
          if (channel)
             cDBusChannelsHelper::AddChannel(array, channel);
          else {
             replyCode = 501;
             replyMessage = cString::sprintf("Channel \"%s\" not defined", option);
             }
          }
       else {
          cChannel *next = Channels.GetByChannelID(tChannelID::FromString(option));
          if (!next) {
             for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
                if (!channel->GroupSep()) {
                   if (strcasestr(channel->Name(), option)) {
                      if (next)
                         cDBusChannelsHelper::AddChannel(array, next);
                      next = channel;
                      }
                   }
                }
             }
          if (next)
             cDBusChannelsHelper::AddChannel(array, next);
          else {
             replyCode = 501;
             replyMessage = cString::sprintf("Channel \"%s\" not defined", option);
             }
          }
       }
    else if (Channels.MaxNumber() >= 1) {
            for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
                if (withGroupSeps || !channel->GroupSep())
                   cDBusChannelsHelper::AddChannel(array, channel);
                }
       }
    else {
       replyCode = 550;
       replyMessage = "No channels defined";
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a(is)is)"));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_variant_builder_add(builder, "i", replyCode);
    g_variant_builder_add(builder, "s", *replyMessage);
    
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  }
}


cDBusChannels::cDBusChannels(void)
:cDBusObject("/Channels", cDBusChannelsHelper::_xmlNodeInfo)
{
  AddMethod("Count", cDBusChannelsHelper::Count);
  AddMethod("GetFromTo", cDBusChannelsHelper::GetFromTo);
  AddMethod("List", cDBusChannelsHelper::List);
}

cDBusChannels::~cDBusChannels(void)
{
}
