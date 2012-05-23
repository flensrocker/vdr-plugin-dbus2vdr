#include "remote.h"
#include "common.h"
#include "helper.h"
#include "monitor.h"

#include <vdr/plugin.h>
#include <vdr/remote.h>


class cDbusSelectMenu : public cOsdMenu
{
private:
  cString _title;
  bool    _selected;

  void SendSelectedItem(int item)
  {
    if (cDBusDispatcherRemote::MainMenuAction != this)
       return;

    DBusMessage *msg = dbus_message_new_signal("/Remote", DBUS_VDR_REMOTE_INTERFACE, "AskUserSelect");
    if (msg == NULL) { 
       esyslog("dbus2vdr: can't create signal");
       return;
       }

    const char *title = *_title;
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title))
       esyslog("dbus2vdr: can't add title to signal");
    else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &item))
       esyslog("dbus2vdr: can't add selected item to signal");
    else if (!cDBusMonitor::SendSignal(msg))
       esyslog("dbus2vdr: can't send signal");
    else
       msg = NULL;

    if (msg != NULL)
       dbus_message_unref(msg);
  }

public:
 cDbusSelectMenu(const char *title)
  :cOsdMenu(title)
  ,_title(title)
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
  cVector<eKeys> keys;
  DBusMessageIter args;
  dbus_int32_t replyCode = 500;
  cString replyMessage;
  if (!dbus_message_iter_init(_msg, &args)) {
     esyslog("dbus2vdr: %s.HitKey: message misses an argument for the keyName", DBUS_VDR_REMOTE_INTERFACE);
     cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
     return;
     }

  const char *keyName = NULL;
  while (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &keyName) >= 0) {
        eKeys k = cKey::FromString(keyName);
        if (k == kNone) {
           replyCode = 504;
           replyMessage = cString::sprintf("Unknown key: \"%s\"", keyName);
           keys.Clear();
           break; // just get the keys until first error
           }
        isyslog("dbus2vdr: %s.HitKey: get key '%s'", DBUS_VDR_REMOTE_INTERFACE, keyName);
        keys.Append(k);
        }

  if (keys.Size() == 0) {
     esyslog("dbus2vdr: %s.HitKey: no valid 'keyName' given", DBUS_VDR_REMOTE_INTERFACE);
     cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
     return;
     }

  for (int i = 0; i < keys.Size(); i++)
      cRemote::Put(keys.At(i));

  replyCode = 250;
  replyMessage = cString::sprintf("Key%s accepted", keys.Size() > 1 ? "s" : "");
  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}


void cDBusMessageRemote::AskUser(void)
{
  if (cDBusDispatcherRemote::MainMenuAction != NULL) {
     cDBusHelper::SendReply(_conn, _msg, 550, "another selection menu already open");
     return;
     }

  char *title = NULL;
  char **item = NULL;
  int num = 0;
  if (!dbus_message_get_args(_msg, NULL, DBUS_TYPE_STRING, &title, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &item, &num, DBUS_TYPE_INVALID)) {
     if (item != NULL)
        dbus_free_string_array(item);
     cDBusHelper::SendReply(_conn, _msg, 501, "arguments are not as expected");
     return;
     }
  if (num < 1) {
     if (item != NULL)
        dbus_free_string_array(item);
     cDBusHelper::SendReply(_conn, _msg, 501, "no items for selection menu given");
     return;
     }

  cDbusSelectMenu *menu = new cDbusSelectMenu(title);
  for (int i = 0; i < num; i++)
      menu->Add(new cOsdItem(item[i], (eOSState)(osUser1 + i)));
  if (item != NULL)
     dbus_free_string_array(item);

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
