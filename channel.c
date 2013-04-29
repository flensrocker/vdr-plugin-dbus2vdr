#include "channel.h"
#include "common.h"
#include "helper.h"

#include <vdr/channels.h>


class cDBusChannelActions
{
private:
  static void AddChannel(DBusMessageIter& args, cChannel *channel)
  {
    if (channel == NULL)
       return;

    DBusMessageIter struc;
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_STRUCT, NULL, &struc))
       esyslog("dbus2vdr: %s.AddChannel: can't open struct container", DBUS_VDR_CHANNEL_INTERFACE);
    else {
       dbus_int32_t index = channel->GroupSep() ? 0 : channel->Number();
       cDBusHelper::AddArg(struc, DBUS_TYPE_INT32, &index);
       cString text = channel->ToText();
       int len = strlen(*text);
       if ((len > 0) && ((*text)[len - 1] == '\n'))
          text.Truncate(-1);
       cDBusHelper::ToUtf8(text);
       const char *line = *text;
       cDBusHelper::AddArg(struc, DBUS_TYPE_STRING, &line);
       if (!dbus_message_iter_close_container(&args, &struc))
          esyslog("dbus2vdr: %s.AddChannel: can't close struct container", DBUS_VDR_CHANNEL_INTERFACE);
       }
  }
public:
  static void Count(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);

    dbus_int32_t count = Channels.Count();
    cDBusHelper::AddArg(replyArgs, DBUS_TYPE_INT32, &count);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.List: out of memory while sending the reply", DBUS_VDR_CHANNEL_INTERFACE);
    dbus_message_unref(reply);
  }

  static void GetFromTo(DBusConnection* conn, DBusMessage* msg)
  {
    dbus_int32_t from_index = -1;
    dbus_int32_t to_index = -1;
    DBusMessageIter args;
    if (dbus_message_iter_init(msg, &args)) {
       if (cDBusHelper::GetNextArg(args, DBUS_TYPE_INT32, &from_index) >= 0)
          cDBusHelper::GetNextArg(args, DBUS_TYPE_INT32, &to_index);
       }
    if (to_index < from_index)
       to_index = from_index;

    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    DBusMessageIter array;
    if (!dbus_message_iter_open_container(&replyArgs, DBUS_TYPE_ARRAY, "(is)", &array))
       esyslog("dbus2vdr: %s.List: can't open array container", DBUS_VDR_CHANNEL_INTERFACE);

    cChannel *c = Channels.Get(from_index);
    while ((c != NULL) && (to_index >= from_index)) {
       AddChannel(array, c);
       c = Channels.Next(c);
       to_index--;
       }
    if (!dbus_message_iter_close_container(&replyArgs, &array))
       esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_CHANNEL_INTERFACE);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.List: out of memory while sending the reply", DBUS_VDR_CHANNEL_INTERFACE);
    dbus_message_unref(reply);
  }

  static void List(DBusConnection* conn, DBusMessage* msg)
  {
    const char *option = NULL;
    bool withGroupSeps = true;

    DBusMessageIter args;
    if (dbus_message_iter_init(msg, &args)) {
       if ((cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &option) >= 0) && (option != NULL))
          withGroupSeps = strcasecmp(option, ":groups") == 0;
       }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    dbus_int32_t replyCode = 250;
    cString replyMessage = "";

    DBusMessageIter array;
    if (!dbus_message_iter_open_container(&replyArgs, DBUS_TYPE_ARRAY, "(is)", &array))
       esyslog("dbus2vdr: %s.List: can't open array container", DBUS_VDR_CHANNEL_INTERFACE);

    if ((option != NULL) && *option && !withGroupSeps) {
       if (isnumber(option)) {
          cChannel *channel = Channels.GetByNumber(strtol(option, NULL, 10));
          if (channel)
             AddChannel(array, channel);
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
                         AddChannel(array, next);
                      next = channel;
                      }
                   }
                }
             }
          if (next)
             AddChannel(array, next);
          else {
             replyCode = 501;
             replyMessage = cString::sprintf("Channel \"%s\" not defined", option);
             }
          }
       }
    else if (Channels.MaxNumber() >= 1) {
            for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
                if (withGroupSeps)
                   AddChannel(array, channel);
                else if (!channel->GroupSep())
                   AddChannel(array, channel);
                }
       }
    else {
       replyCode = 550;
       replyMessage = "No channels defined";
       }

    if (!dbus_message_iter_close_container(&replyArgs, &array))
       esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_CHANNEL_INTERFACE);

    cDBusHelper::AddArg(replyArgs, DBUS_TYPE_INT32, &replyCode);
    cDBusHelper::AddArg(replyArgs, DBUS_TYPE_STRING, *replyMessage);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.List: out of memory while sending the reply", DBUS_VDR_CHANNEL_INTERFACE);
    dbus_message_unref(reply);
  }
};


cDBusDispatcherChannel::cDBusDispatcherChannel(void)
:cDBusMessageDispatcher(busSystem, DBUS_VDR_CHANNEL_INTERFACE)
{
  AddPath("/Channels");
  AddAction("Count", cDBusChannelActions::Count);
  AddAction("GetFromTo", cDBusChannelActions::GetFromTo);
  AddAction("List", cDBusChannelActions::List);
}

cDBusDispatcherChannel::~cDBusDispatcherChannel(void)
{
}

bool          cDBusDispatcherChannel::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Channels") != 0)
     return false;
  Data =
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
  return true;
}


const char *cDBusChannels::XmlNodeInfo = 
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

cDBusChannels::cDBusChannels(void)
:cDBusObject("/Channels", XmlNodeInfo)
{
}

cDBusChannels::~cDBusChannels(void)
{
}

void  cDBusChannels::HandleMethodCall(GDBusConnection       *connection,
                                      const gchar           *sender,
                                      const gchar           *object_path,
                                      const gchar           *interface_name,
                                      const gchar           *method_name,
                                      GVariant              *parameters,
                                      GDBusMethodInvocation *invocation)
{
  if (g_strcmp0(method_name, "Count") == 0) {
     g_dbus_method_invocation_return_value(invocation, g_variant_new_int32(Channels.Count()));
     }
  else if (g_strcmp0(method_name, "GetFromTo") == 0) {
     cDBusObject::HandleMethodCall(connection, sender, object_path, interface_name, method_name, parameters, invocation);
     }
  else if (g_strcmp0(method_name, "List") == 0) {
     cDBusObject::HandleMethodCall(connection, sender, object_path, interface_name, method_name, parameters, invocation);
     }
}
