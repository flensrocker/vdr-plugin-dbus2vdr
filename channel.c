#include "channel.h"
#include "common.h"
#include "helper.h"

#include <vdr/channels.h>
#include <vdr/device.h>


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
    "    <method name=\"Current\">\n"
    "      <arg name=\"number\"       type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"text\"         type=\"s\" direction=\"out\"/>\n"
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

  static void AddChannel(GVariantBuilder *Array, const cChannel *Channel)
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

  static void Count(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const cChannels *channels = NULL;
#if VDRVERSNUM > 20300
    LOCK_CHANNELS_READ;
    channels = Channels;
#else
    channels = &Channels;
#endif
    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(i)", channels->Count()));
  }

  static void Current(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int number = cDevice::CurrentChannel();
    cString text = "";
    const cChannel* currentChannel = NULL;
#if VDRVERSNUM > 20300
    LOCK_CHANNELS_READ;
    const cChannels *channels = Channels;
    currentChannel = channels->GetByNumber(number);
#else
    currentChannel = Channels.GetByNumber(number);
#endif
    if (currentChannel != NULL) {
       text = currentChannel->ToText();
       int len = strlen(*text);
       if ((len > 0) && ((*text)[len - 1] == '\n'))
          text.Truncate(-1);
       cDBusHelper::ToUtf8(text);
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(is)"));
    g_variant_builder_add(builder, "i", number);
    g_variant_builder_add(builder, "s", *text);

    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  }

  static void GetFromTo(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gint32 from_index = -1;
    gint32 to_index = -1;
    g_variant_get(Parameters, "(ii)", &from_index, &to_index);

    if (to_index < from_index)
       to_index = from_index;

    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(is)"));

    const cChannels *channels = NULL;
#if VDRVERSNUM > 20300
    LOCK_CHANNELS_READ;
    channels = Channels;
#else
    channels = &Channels;
#endif
    const cChannel *c = channels->Get(from_index);
    while ((c != NULL) && (to_index >= from_index)) {
       cDBusChannelsHelper::AddChannel(array, c);
       c = channels->Next(c);
       to_index--;
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(a(is))"));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  }

  static void List(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const gchar *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);

    bool withGroupSeps = (g_strcmp0(option, ":groups") == 0);
    gint32 replyCode = 250;
    cString replyMessage = "";
    
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(is)"));

#if VDRVERSNUM > 20300
    LOCK_CHANNELS_READ;
    const cChannels *channels = Channels;
#else
    cChannels *channels = &Channels;
#endif
    if ((option != NULL) && *option && !withGroupSeps) {
       if (isnumber(option)) {
          const cChannel *channel = channels->GetByNumber(strtol(option, NULL, 10));
          if (channel)
             cDBusChannelsHelper::AddChannel(array, channel);
          else {
             replyCode = 501;
             replyMessage = cString::sprintf("Channel \"%s\" not defined", option);
             }
          }
       else {
          const cChannel *next = channels->GetByChannelID(tChannelID::FromString(option));
          if (!next) {
             for (const cChannel *channel = channels->First(); channel; channel = channels->Next(channel)) {
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
    else if (channels->MaxNumber() >= 1) {
            for (const cChannel *channel = channels->First(); channel; channel = channels->Next(channel)) {
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
  AddMethod("Current", cDBusChannelsHelper::Current);
  AddMethod("GetFromTo", cDBusChannelsHelper::GetFromTo);
  AddMethod("List", cDBusChannelsHelper::List);
}

cDBusChannels::~cDBusChannels(void)
{
}
