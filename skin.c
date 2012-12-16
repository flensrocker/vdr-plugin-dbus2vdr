#include "skin.h"
#include "common.h"
#include "helper.h"

#include <vdr/skins.h>


class cDBusSkinActions
{
private:
  static void AddSkin(DBusMessageIter& args, cSkin *skin)
  {
    if (skin == NULL)
       return;

    DBusMessageIter struc;
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_STRUCT, NULL, &struc))
       esyslog("dbus2vdr: %s.AddSkin: can't open struct container", DBUS_VDR_SKIN_INTERFACE);
    else {
       dbus_int32_t index = skin->Index();
       cDBusHelper::AddArg(struc, DBUS_TYPE_INT32, &index);
       const char *name = skin->Name();
       cDBusHelper::AddArg(struc, DBUS_TYPE_STRING, &name);
       const char *desc = skin->Description();
       cDBusHelper::AddArg(struc, DBUS_TYPE_STRING, &desc);
       if (!dbus_message_iter_close_container(&args, &struc))
          esyslog("dbus2vdr: %s.AddSkin: can't close struct container", DBUS_VDR_SKIN_INTERFACE);
       }
  }

public:
  static void CurrentSkin(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args;
    dbus_message_iter_init_append(reply, &args);

    dbus_int32_t replyCode = 900;
    cDBusHelper::AddArg(args, DBUS_TYPE_INT32, &replyCode);

    AddSkin(args, Skins.Current());

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.CurrentSkin: out of memory while sending the reply", DBUS_VDR_SKIN_INTERFACE);
    dbus_message_unref(reply);
  }

  static void ListSkins(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args;
    dbus_message_iter_init_append(reply, &args);

    dbus_int32_t replyCode = 900;
    cDBusHelper::AddArg(args, DBUS_TYPE_INT32, &replyCode);

    DBusMessageIter array;
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(iss)", &array))
       esyslog("dbus2vdr: %s.ListSkins: can't open array container", DBUS_VDR_SKIN_INTERFACE);

    for (cSkin* skin = Skins.First(); skin; skin = Skins.Next(skin))
        AddSkin(array, skin);

    if (!dbus_message_iter_close_container(&args, &array))
       esyslog("dbus2vdr: %s.ListSkins: can't close array container", DBUS_VDR_SKIN_INTERFACE);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.ListSkins: out of memory while sending the reply", DBUS_VDR_SKIN_INTERFACE);
    dbus_message_unref(reply);
  }

  static void QueueMessage(DBusConnection* conn, DBusMessage* msg)
  {
    const char *messageText = NULL;
    DBusMessageIter args;
    if (!dbus_message_iter_init(msg, &args))
       esyslog("dbus2vdr: %s.QueueMessage: message misses an argument for the message text", DBUS_VDR_SKIN_INTERFACE);
    else {
       int rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &messageText);
       if (rc < 0)
          esyslog("dbus2vdr: %s.QueueMessage: 'messageText' argument is not a string", DBUS_VDR_SKIN_INTERFACE);
       }

    dbus_int32_t replyCode = 501;
    cString replyMessage;
    if (messageText != NULL) {
       Skins.QueueMessage(mtInfo, messageText);
       replyCode = 250;
       replyMessage = "Message queued";
       }
    else
       replyMessage = "Missing message";

    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }
};


cDBusDispatcherSkin::cDBusDispatcherSkin(void)
:cDBusMessageDispatcher(busSystem, DBUS_VDR_SKIN_INTERFACE)
{
  AddPath("/Skin");
  AddAction("CurrentSkin", cDBusSkinActions::CurrentSkin);
  AddAction("ListSkins", cDBusSkinActions::ListSkins);
  AddAction("QueueMessage", cDBusSkinActions::QueueMessage);
}

cDBusDispatcherSkin::~cDBusDispatcherSkin(void)
{
}

bool          cDBusDispatcherSkin::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Skin") != 0)
     return false;
  Data =
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
  "    <method name=\"QueueMessage\">\n"
  "      <arg name=\"messageText\"  type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
