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
    case dmsQueueMessage:
      QueueMessage();
      break;
    }
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
  "    <method name=\"QueueMessage\">\n"
  "      <arg name=\"messageText\"    type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"message\"        type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
