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


class cDBusRemoteActions
{
public:
  static void CallPlugin(DBusConnection* conn, DBusMessage* msg)
  {
    const char *pluginName = NULL;
    DBusMessageIter args;
    if (!dbus_message_iter_init(msg, &args))
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

    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }

  static void Enable(DBusConnection* conn, DBusMessage* msg)
  {
    cRemote::SetEnabled(true);
    cDBusHelper::SendReply(conn, msg, 250, "Remote control enabled");
  }

  static void Disable(DBusConnection* conn, DBusMessage* msg)
  {
    cRemote::SetEnabled(false);
    cDBusHelper::SendReply(conn, msg, 250, "Remote control disabled");
  }

  static void Status(DBusConnection* conn, DBusMessage* msg)
  {
    int enabled = 0;
    if (cRemote::Enabled())
       enabled = 1;
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args;
    dbus_message_iter_init_append(reply, &args);

    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &enabled))
       esyslog("dbus2vdr: %s.Status: out of memory while appending the status", DBUS_VDR_REMOTE_INTERFACE);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.Status: out of memory while sending the reply", DBUS_VDR_REMOTE_INTERFACE);
    dbus_message_unref(reply);
  }

  static void HitKey(DBusConnection* conn, DBusMessage* msg)
  {
    cVector<eKeys> keys;
    DBusMessageIter args;
    dbus_int32_t replyCode = 500;
    cString replyMessage;
    if (!dbus_message_iter_init(msg, &args)) {
       esyslog("dbus2vdr: %s.HitKey: message misses an argument for the keyName", DBUS_VDR_REMOTE_INTERFACE);
       cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
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
       cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
       return;
       }

    for (int i = 0; i < keys.Size(); i++)
        cRemote::Put(keys.At(i));

    replyCode = 250;
    replyMessage = cString::sprintf("Key%s accepted", keys.Size() > 1 ? "s" : "");
    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }

  static void AskUser(DBusConnection* conn, DBusMessage* msg)
  {
    if (cDBusDispatcherRemote::MainMenuAction != NULL) {
       cDBusHelper::SendReply(conn, msg, 550, "another selection menu already open");
       return;
       }

    char *title = NULL;
    char **item = NULL;
    int num = 0;
    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &title, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &item, &num, DBUS_TYPE_INVALID)) {
       if (item != NULL)
          dbus_free_string_array(item);
       cDBusHelper::SendReply(conn, msg, 501, "arguments are not as expected");
       return;
       }
    if (num < 1) {
       if (item != NULL)
          dbus_free_string_array(item);
       cDBusHelper::SendReply(conn, msg, 501, "no items for selection menu given");
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
       cDBusHelper::SendReply(conn, msg, 550, "can't display selection menu");
       return;
       }

    cDBusHelper::SendReply(conn, msg, 250, "display selection menu");
  }

  static void SwitchChannel(DBusConnection* conn, DBusMessage* msg)
  {
    dbus_int32_t replyCode = 500;
    cString replyMessage;
    char *Option = NULL;
    if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &Option, DBUS_TYPE_INVALID) && (Option != NULL) && (*Option != NULL)) {
       int n = -1;
       int d = 0;
       if (isnumber(Option)) {
          int o = strtol(Option, NULL, 10);
          if (o >= 1 && o <= Channels.MaxNumber())
             n = o;
          }
       else if (strcmp(Option, "-") == 0) {
          n = cDevice::CurrentChannel();
          if (n > 1) {
             n--;
             d = -1;
             }
          }
       else if (strcmp(Option, "+") == 0) {
          n = cDevice::CurrentChannel();
          if (n < Channels.MaxNumber()) {
             n++;
             d = 1;
             }
          }
       else {
          cChannel *channel = Channels.GetByChannelID(tChannelID::FromString(Option));
          if (channel)
             n = channel->Number();
          else {
             for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
                 if (!channel->GroupSep()) {
                    if (strcasecmp(channel->Name(), Option) == 0) {
                       n = channel->Number();
                       break;
                       }
                    }
                 }
             }
          }
       if (n < 0) {
          replyCode = 501;
          replyMessage = cString::sprintf("Undefined channel \"%s\"", Option);
          cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
          return;
          }
       if (!d) {
          cChannel *channel = Channels.GetByNumber(n);
          if (channel) {
             if (!cDevice::PrimaryDevice()->SwitchChannel(channel, true)) {
                replyCode = 554;
                replyMessage = cString::sprintf("Error switching to channel \"%d\"", channel->Number());
                cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
                return;
                }
             }
          else {
             replyCode = 550;
             replyMessage = cString::sprintf("Unable to find channel \"%s\"", Option);
             cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
             return;
             }
          }
       else
          cDevice::SwitchChannel(d);
       }
    cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
    if (channel) {
       replyCode = 250;
       replyMessage = cString::sprintf("%d %s", channel->Number(), channel->Name());
       }
    else {
       replyCode = 550;
       replyMessage = cString::sprintf("Unable to find channel \"%d\"", cDevice::CurrentChannel());
       }
    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }

  static void GetVolume(DBusConnection* conn, DBusMessage* msg)
  {
    int currentVol = cDevice::CurrentVolume();
    dbus_bool_t mute = cDevice::PrimaryDevice()->IsMute();
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_INT32, &currentVol))
       esyslog("dbus2vdr: %s.GetVolume: out of memory while appending the current volume", DBUS_VDR_REMOTE_INTERFACE);
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_BOOLEAN, &mute))
       esyslog("dbus2vdr: %s.GetVolume: out of memory while appending the mute status", DBUS_VDR_REMOTE_INTERFACE);
    cDBusHelper::SendReply(conn, reply);
  }

  static void SetVolume(DBusConnection* conn, DBusMessage* msg)
  {
    dbus_int32_t replyCode = 250;
    cString replyMessage;
    const char *option = NULL;
    int volume = -1;
    DBusMessageIter args;
    DBusMessageIter sub;
    if (dbus_message_iter_init(msg, &args)) {
       DBusMessageIter *refArgs = &args;
       if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
          dbus_message_iter_recurse(&args, &sub);
          refArgs = &sub;
          }
       if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_STRING)
          dbus_message_iter_get_basic(refArgs, &option);
       else if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_INT32)
          dbus_message_iter_get_basic(refArgs, &volume);
       }
    if (option != NULL) {
       if (isnumber(option))
          cDevice::PrimaryDevice()->SetVolume(strtol(option, NULL, 10), true);
       else if (strcmp(option, "+") == 0)
          cDevice::PrimaryDevice()->SetVolume(VOLUMEDELTA);
       else if (strcmp(option, "-") == 0)
          cDevice::PrimaryDevice()->SetVolume(-VOLUMEDELTA);
       else if (strcasecmp(option, "mute") == 0)
          cDevice::PrimaryDevice()->ToggleMute();
       else {
          replyCode = 501;
          replyMessage = cString::sprintf("Unknown option: \"%s\"", option);
          }
       }
    else {
       cDevice::PrimaryDevice()->SetVolume(volume, true);
       }

    int currentVol = cDevice::CurrentVolume();
    dbus_bool_t mute = cDevice::PrimaryDevice()->IsMute();
    if (mute)
       replyMessage = "Audio is mute";
    else
       replyMessage = cString::sprintf("Audio volume is %d", currentVol);
 
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_INT32, &currentVol))
       esyslog("dbus2vdr: %s.SetVolume: out of memory while appending the current volume", DBUS_VDR_REMOTE_INTERFACE);
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_BOOLEAN, &mute))
       esyslog("dbus2vdr: %s.SetVolume: out of memory while appending the mute status", DBUS_VDR_REMOTE_INTERFACE);
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_INT32, &replyCode))
       esyslog("dbus2vdr: %s.SetVolume: out of memory while appending the reply-code", DBUS_VDR_REMOTE_INTERFACE);
    const char *message = *replyMessage;
    if (!dbus_message_iter_append_basic(&replyArgs, DBUS_TYPE_STRING, &message))
       esyslog("dbus2vdr: %s.SetVolume: out of memory while appending the reply-message", DBUS_VDR_REMOTE_INTERFACE);
    cDBusHelper::SendReply(conn, reply);
  }
};


cOsdObject *cDBusDispatcherRemote::MainMenuAction = NULL;

cDBusDispatcherRemote::cDBusDispatcherRemote(void)
:cDBusMessageDispatcher(DBUS_VDR_REMOTE_INTERFACE)
{
  AddPath("/Remote");
  AddAction("CallPlugin", cDBusRemoteActions::CallPlugin);
  AddAction("Enable", cDBusRemoteActions::Enable);
  AddAction("Disable", cDBusRemoteActions::Disable);
  AddAction("Status", cDBusRemoteActions::Status);
  AddAction("HitKey", cDBusRemoteActions::HitKey);
  AddAction("AskUser", cDBusRemoteActions::AskUser);
  AddAction("SwitchChannel", cDBusRemoteActions::SwitchChannel);
  AddAction("GetVolume", cDBusRemoteActions::GetVolume);
  AddAction("SetVolume", cDBusRemoteActions::SetVolume);
}

cDBusDispatcherRemote::~cDBusDispatcherRemote(void)
{
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
  "    <method name=\"SwitchChannel\">\n"
  "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"GetVolume\">\n"
  "      <arg name=\"volume\"       type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"mute\"         type=\"b\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"SetVolume\">\n"
  "      <arg name=\"option\"       type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"volume\"       type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"mute\"         type=\"b\" direction=\"out\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}

void          cDBusDispatcherRemote::OnStop(void)
{
  MainMenuAction = NULL;
}
