#include "skin.h"
#include "common.h"
#include "helper.h"

#include <vdr/skins.h>


cDBusMessageSkin::cDBusMessageSkin(cDBusMessageSkin::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageSkin::~cDBusMessageSkin(void)
{
}

void cDBusMessageSkin::Process(void)
{
  switch (_action) {
    case dmsCurrentSkin:
      CurrentSkin();
      break;
    case dmsListSkins:
      ListSkins();
      break;
    case dmsQueueMessage:
      QueueMessage();
      break;
    }
}

void AddSkin(DBusMessageIter& args, cSkin *skin)
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

void cDBusMessageSkin::CurrentSkin(void)
{
  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  dbus_int32_t replyCode = 900;
  cDBusHelper::AddArg(args, DBUS_TYPE_INT32, &replyCode);

  AddSkin(args, Skins.Current());

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.CurrentSkin: out of memory while sending the reply", DBUS_VDR_SKIN_INTERFACE);
  dbus_message_unref(reply);
}

void cDBusMessageSkin::ListSkins(void)
{
  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  dbus_int32_t replyCode = 900;
  cDBusHelper::AddArg(args, DBUS_TYPE_INT32, &replyCode);

  DBusMessageIter array;
  if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(iss)", &array))
     esyslog("dbus2vdr: %s.GetNames: can't open array container", DBUS_VDR_SKIN_INTERFACE);

  for (cSkin* skin = Skins.First(); skin; skin = Skins.Next(skin))
      AddSkin(array, skin);

  if (!dbus_message_iter_close_container(&args, &array))
     esyslog("dbus2vdr: %s.GetNames: can't close array container", DBUS_VDR_SKIN_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.GetNames: out of memory while sending the reply", DBUS_VDR_SKIN_INTERFACE);
  dbus_message_unref(reply);
}

void cDBusMessageSkin::QueueMessage(void)
{
  const char *messageText = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
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

  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}


cDBusDispatcherSkin::cDBusDispatcherSkin(void)
:cDBusMessageDispatcher(DBUS_VDR_SKIN_INTERFACE)
{
}

cDBusDispatcherSkin::~cDBusDispatcherSkin(void)
{
}

cDBusMessage *cDBusDispatcherSkin::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Skin") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_SKIN_INTERFACE, "CurrentSkin"))
     return new cDBusMessageSkin(cDBusMessageSkin::dmsCurrentSkin, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_SKIN_INTERFACE, "ListSkins"))
     return new cDBusMessageSkin(cDBusMessageSkin::dmsListSkins, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_SKIN_INTERFACE, "QueueMessage"))
     return new cDBusMessageSkin(cDBusMessageSkin::dmsQueueMessage, conn, msg);

  return NULL;
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
