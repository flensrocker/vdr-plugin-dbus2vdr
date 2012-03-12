#include "remote.h"
#include "common.h"
#include "helper.h"

#include <vdr/plugin.h>
#include <vdr/remote.h>


class cDbusSelectMenu : public cOsdMenu
{
private:
  DBusConnection *_conn;
  bool _selected;

  void SendSelectedItem(int item)
  {
    if (cDBusDispatcherRemote::MainMenuAction != this)
       return;

    DBusMessage *msg = dbus_message_new_signal("/Remote", DBUS_VDR_REMOTE_INTERFACE, "AskUserSelect");
    if (msg == NULL) { 
       esyslog("dbus2vdr: can't create signal");
       return;
       }

    dbus_uint32_t serial = 0;
    const char *title = Title();
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title))
       esyslog("dbus2vdr: can't add title to signal");
    else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &item))
       esyslog("dbus2vdr: can't add selected item to signal");
    else if (!dbus_connection_send(_conn, msg, &serial))
       esyslog("dbus2vdr: can't send signal");
    else
       dbus_connection_flush(_conn);
    dbus_message_unref(msg);
  }

public:
 cDbusSelectMenu(DBusConnection* conn, const char *title)
  :cOsdMenu(title)
  ,_conn(conn)
  ,_selected(false) 
 {
 }

 virtual ~cDbusSelectMenu(void)
 {
   if (!_selected) {
      isyslog("dbus2vdr: selected nothing");
      SendSelectedItem(-1);
      }
   cDBusDispatcherRemote::MainMenuAction = NULL;
 }

 virtual eOSState ProcessKey(eKeys Key)
 {
   int state = cOsdMenu::ProcessKey(Key);
   if (state > os_User) {
      _selected = true;
      int item = state - os_User - 1;
      isyslog("dbus2vdr: select item %d", item);
      SendSelectedItem(item);
      return osEnd;
      }
   return (eOSState)state;
 }
};


cDBusMessageRemote::cDBusMessageRemote(cDBusMessageRemote::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageRemote::~cDBusMessageRemote(void)
{
}

void cDBusMessageRemote::Process(void)
{
  switch (_action) {
    case dmrCallPlugin:
      CallPlugin();
      break;
    case dmrEnable:
      Enable();
      break;
    case dmrDisable:
      Disable();
      break;
    case dmrStatus:
      Status();
      break;
    case dmrHitKey:
      HitKey();
      break;
    case dmrAskUser:
      AskUser();
      break;
    }
}

void cDBusMessageRemote::CallPlugin(void)
{
  const char *pluginName = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.CallPlugin: message misses an argument for the keyName", DBUS_VDR_REMOTE_INTERFACE);
  else {
     int rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &pluginName);
     if (rc < 0)
        esyslog("dbus2vdr: %s.CallPlugin: 'pluginName' argument is not a string", DBUS_VDR_REMOTE_INTERFACE);
     }

  dbus_int32_t replyCode = 550;
  cString replyMessage;
  if (pluginName) {
     // The parameter of cRemote::CallPlugin has to be static,
     // since it's stored by vdr and used later.
     // Hence this workaround to get a 'long-life' pointer to a string with the same content without storing it.
     cPlugin *plugin = cPluginManager::GetPlugin(pluginName);
     if (plugin) {
        if (cRemote::CallPlugin(plugin->Name())) {
           replyCode = 250;
           replyMessage = cString::sprintf("plugin %s called", pluginName);
           }
        else
           replyMessage = cString::sprintf("plugin %s not called, another call is pending", pluginName);
        }
     else
        replyMessage = cString::sprintf("plugin %s not found", pluginName);
     }
  else
     replyMessage = "name of plugin to be called is missing";

  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}

void cDBusMessageRemote::Enable(void)
{
  cRemote::SetEnabled(true);
  cDBusHelper::SendReply(_conn, _msg, 250, "Remote control enabled");
}

void cDBusMessageRemote::Disable(void)
{
  cRemote::SetEnabled(false);
  cDBusHelper::SendReply(_conn, _msg, 250, "Remote control disabled");
}

void cDBusMessageRemote::Status(void)
{
  int enabled = 0;
  if (cRemote::Enabled())
     enabled = 1;
  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &enabled))
     esyslog("dbus2vdr: %s.Status: out of memory while appending the status", DBUS_VDR_REMOTE_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.Status: out of memory while sending the reply", DBUS_VDR_REMOTE_INTERFACE);
  dbus_message_unref(reply);
}

void cDBusMessageRemote::HitKey(void)
{
  const char *keyName = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.HitKey: message misses an argument for the keyName", DBUS_VDR_REMOTE_INTERFACE);
  else {
     int rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &keyName);
     if (rc < 0)
        esyslog("dbus2vdr: %s.HitKey: 'keyName' argument is not a string", DBUS_VDR_REMOTE_INTERFACE);
     }

  dbus_int32_t replyCode = 500;
  cString replyMessage;
  if (keyName != NULL) {
     eKeys k = cKey::FromString(keyName);
     if (k != kNone) {
        cRemote::Put(k);
        replyCode = 250;
        replyMessage = cString::sprintf("Key \"%s\" accepted", keyName);
        }
     else {
        replyCode = 504;
        replyMessage = cString::sprintf("Unknown key: \"%s\"", keyName);
        }
     }

  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}


void cDBusMessageRemote::AskUser(void)
{
  if (cDBusDispatcherRemote::MainMenuAction != NULL) {
     cDBusHelper::SendReply(_conn, _msg, 550, "another selection menu already open");
     return;
     }

  cDbusSelectMenu *menu = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.AskUser: message misses an argument for the keyName", DBUS_VDR_REMOTE_INTERFACE);
  else {
     const char *item = NULL;
     int i = 0;
     while (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &item) >= 0) {
           if (menu == NULL)
              menu = new cDbusSelectMenu(_conn, item);
           else {
              menu->Add(new cOsdItem(item, (eOSState)(osUser1 + i)));
              i++;
              }
           }
     }

  if (menu == NULL) {
     cDBusHelper::SendReply(_conn, _msg, 501, "no title for selection menu given");
     return;
     }

  if (menu->Count() == 0) {
     delete menu;
     cDBusHelper::SendReply(_conn, _msg, 501, "no items for selection menu given");
     return;
     }

  cDBusDispatcherRemote::MainMenuAction = menu;
  if (!cRemote::CallPlugin("dbus2vdr")) {
     cDBusDispatcherRemote::MainMenuAction = NULL;
     delete menu;
     cDBusHelper::SendReply(_conn, _msg, 550, "can't display selection menu");
     return;
     }

  cDBusHelper::SendReply(_conn, _msg, 250, "display selection menu");
}


cOsdObject *cDBusDispatcherRemote::MainMenuAction = NULL;

cDBusDispatcherRemote::cDBusDispatcherRemote(void)
:cDBusMessageDispatcher(DBUS_VDR_REMOTE_INTERFACE)
{
}

cDBusDispatcherRemote::~cDBusDispatcherRemote(void)
{
}

cDBusMessage *cDBusDispatcherRemote::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Remote") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "CallPlugin"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrCallPlugin, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "Enable"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrEnable, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "Disable"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrDisable, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "Status"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrStatus, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "HitKey"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrHitKey, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_REMOTE_INTERFACE, "AskUser"))
     return new cDBusMessageRemote(cDBusMessageRemote::dmrAskUser, conn, msg);

  return NULL;
}

bool          cDBusDispatcherRemote::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Remote") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_REMOTE_INTERFACE"\">\n"
  "    <method name=\"CallPlugin\">\n"
  "      <arg name=\"pluginName\"   type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Enable\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Disable\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Status\">\n"
  "      <arg name=\"status\"       type=\"b\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"HitKey\">\n"
  "      <arg name=\"keyName\"      type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"AskUser\">\n"
  "      <arg name=\"title\"        type=\"s\"  direction=\"in\"/>\n"
  "      <arg name=\"items\"        type=\"as\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\"  direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\"  direction=\"out\"/>\n"
  "    </method>\n"
  "    <signal name=\"AskUserSelect\">\n"
  "      <arg name=\"title\"        type=\"s\"/>\n"
  "      <arg name=\"index\"        type=\"i\"/>\n"
  "    </signal>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}

void          cDBusDispatcherRemote::OnStop(void)
{
  MainMenuAction = NULL;
}
