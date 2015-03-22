#include "remote.h"
#include "common.h"
#include "connection.h"
#include "helper.h"

#include <vdr/plugin.h>
#include <vdr/remote.h>


class cDbusSelectMenu : public cOsdMenu
{
private:
  cDBusObject *_object;
  cString      _title;
  bool         _selected;

  void SendSelectedItem(int item)
  {
    if ((cDBusRemote::MainMenuAction != this) || (_object == NULL))
       return;

    _object->Connection()->EmitSignal( new cDBusSignal(NULL, "/Remote", DBUS_VDR_REMOTE_INTERFACE, "AskUserSelect", g_variant_new("(si)", *_title, item), NULL, NULL));
  }

public:
 cDbusSelectMenu(cDBusObject *Object, const char *Title)
  :cOsdMenu(Title)
  ,_object(Object)
  ,_title(Title)
  ,_selected(false) 
 {
 }

 virtual ~cDbusSelectMenu(void)
 {
   if (!_selected) {
      isyslog("dbus2vdr: selected nothing");
      SendSelectedItem(-1);
      }
   cDBusRemote::MainMenuAction = NULL;
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


namespace cDBusRemoteHelper
{
  static const char *_xmlNodeInfo = 
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
    "    <method name=\"HitKeys\">\n"
    "      <arg name=\"keyNames\"     type=\"as\" direction=\"in\"/>\n"
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

  static void CallPlugin(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const gchar *pluginName = NULL;
    g_variant_get(Parameters, "(&s)", &pluginName);

    gint32 replyCode = 550;
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

    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void Enable(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    cRemote::SetEnabled(true);
    cDBusHelper::SendReply(Invocation, 250, "Remote control enabled");
  };

  static void Disable(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    cRemote::SetEnabled(false);
    cDBusHelper::SendReply(Invocation, 250, "Remote control disabled");
  };

  static void Status(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gboolean enabled = FALSE;
    if (cRemote::Enabled())
       enabled = TRUE;
    g_dbus_method_invocation_return_value(Invocation, g_variant_new("(b)", enabled));
  };

  static void HitKey(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const gchar *keyName = NULL;
    g_variant_get(Parameters, "(&s)", &keyName);

    gint32 replyCode = 500;
    cString replyMessage;
    eKeys k = cKey::FromString(keyName);
    if (k == kNone) {
       replyCode = 504;
       replyMessage = cString::sprintf("Unknown key: \"%s\"", keyName);
       cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
       return;
       }
    isyslog("dbus2vdr: %s.HitKey: get key '%s'", DBUS_VDR_REMOTE_INTERFACE, keyName);

    cRemote::Put(k);

    replyCode = 250;
    replyMessage = "Key accepted";
    cDBusHelper::SendReply(Invocation, replyCode, replyMessage);
  };

  static void HitKeys(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gsize len = 0;
    GVariant *array = g_variant_get_child_value(Parameters, 0);
    const gchar **keyNames = g_variant_get_strv(array, &len);

    gint32 replyCode = 500;
    cString replyMessage;
    cVector<eKeys> keys;
    for (gsize i = 0; i < len; i++) {
          eKeys k = cKey::FromString(keyNames[i]);
          if (k == kNone) {
             replyCode = 504;
             replyMessage = cString::sprintf("Unknown key: \"%s\"", keyNames[i]);
             keys.Clear();
             break; // just get the keys until first error
             }
          isyslog("dbus2vdr: %s.HitKeys: get key '%s'", DBUS_VDR_REMOTE_INTERFACE, keyNames[i]);
          keys.Append(k);
          }
    g_free(keyNames);
    g_variant_unref(array);

    if (keys.Size() == 0) {
       esyslog("dbus2vdr: %s.HitKeys: no valid 'keyName' given", DBUS_VDR_REMOTE_INTERFACE);
       cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
       return;
       }

    for (int i = 0; i < keys.Size(); i++)
        cRemote::Put(keys.At(i));

    replyCode = 250;
    replyMessage = cString::sprintf("Key%s accepted", keys.Size() > 1 ? "s" : "");
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void AskUser(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    if (cDBusRemote::MainMenuAction != NULL) {
       cDBusHelper::SendReply(Invocation, 550, "another selection menu is already open");
       return;
       }

    const gchar *title = NULL;
    GVariant *t = g_variant_get_child_value(Parameters, 0);
    g_variant_get(t, "&s", &title);
    
    gsize num = 0;
    GVariant *array = g_variant_get_child_value(Parameters, 1);
    const gchar **item = g_variant_get_strv(array, &num);

    if (num < 1) {
       cDBusHelper::SendReply(Invocation, 501, "no items for selection menu given");
       g_free(item);
       g_variant_unref(t);
       g_variant_unref(array);
       return;
       }

    cDbusSelectMenu *menu = new cDbusSelectMenu(Object, title);
    for (gsize i = 0; i < num; i++)
        menu->Add(new cOsdItem(item[i], (eOSState)(osUser1 + i)));
    g_free(item);
    g_variant_unref(t);
    g_variant_unref(array);

    cDBusRemote::MainMenuAction = menu;
    if (!cRemote::CallPlugin("dbus2vdr")) {
       cDBusRemote::MainMenuAction = NULL;
       delete menu;
       cDBusHelper::SendReply(Invocation, 550, "can't display selection menu");
       return;
       }

    cDBusHelper::SendReply(Invocation, 250, "display selection menu");
  };

  static void SwitchChannel(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gint32 replyCode = 500;
    cString replyMessage;
    char *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);
    if ((option != NULL) && (option[0] != 0)) {
       int n = -1;
       int d = 0;
       if (isnumber(option)) {
          int o = strtol(option, NULL, 10);
          if (o >= 1 && o <= Channels.MaxNumber())
             n = o;
          }
       else if (strcmp(option, "-") == 0) {
          n = cDevice::CurrentChannel();
          if (n > 1) {
             n--;
             d = -1;
             }
          }
       else if (strcmp(option, "+") == 0) {
          n = cDevice::CurrentChannel();
          if (n < Channels.MaxNumber()) {
             n++;
             d = 1;
             }
          }
       else {
          cChannel *channel = Channels.GetByChannelID(tChannelID::FromString(option));
          if (channel)
             n = channel->Number();
          else {
             for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
                 if (!channel->GroupSep()) {
                    if (strcasecmp(channel->Name(), option) == 0) {
                       n = channel->Number();
                       break;
                       }
                    }
                 }
             }
          }
       if (n < 0) {
          replyCode = 501;
          replyMessage = cString::sprintf("Undefined channel \"%s\"", option);
          cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
          return;
          }
       if (!d) {
          cChannel *channel = Channels.GetByNumber(n);
          if (channel) {
             if (!cDevice::PrimaryDevice()->SwitchChannel(channel, true)) {
                replyCode = 554;
                replyMessage = cString::sprintf("Error switching to channel \"%d\"", channel->Number());
                cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
                return;
                }
             }
          else {
             replyCode = 550;
             replyMessage = cString::sprintf("Unable to find channel \"%s\"", option);
             cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
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
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void GetVolume(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int currentVol = cDevice::CurrentVolume();
    gboolean mute = FALSE;
    if (cDevice::PrimaryDevice()->IsMute())
       mute = TRUE;
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(ib)"));
    g_variant_builder_add(builder, "i", currentVol);
    g_variant_builder_add(builder, "b", mute);
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  };

  static void SetVolume(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gint32 replyCode = 250;
    cString replyMessage;
    const char *option = NULL;
    int volume = -1;
    GVariant *value = g_variant_get_child_value(Parameters, 0);
    GVariant *refValue = value;
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT))
       refValue = g_variant_get_child_value(value, 0);

    if (g_variant_is_of_type(refValue, G_VARIANT_TYPE_STRING))
       g_variant_get(refValue, "&s", &option);
    else if (g_variant_is_of_type(refValue, G_VARIANT_TYPE_INT32))
       g_variant_get(refValue, "i", &volume);

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
    if (refValue != value)
       g_variant_unref(refValue);
    g_variant_unref(value);

    int currentVol = cDevice::CurrentVolume();
    gboolean mute = FALSE;
    if (cDevice::PrimaryDevice()->IsMute())
       mute = TRUE;
    if (mute)
       replyMessage = "Audio is mute";
    else
       replyMessage = cString::sprintf("Audio volume is %d", currentVol);
 
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(ibis)"));
    g_variant_builder_add(builder, "i", currentVol);
    g_variant_builder_add(builder, "b", mute);
    g_variant_builder_add(builder, "i", replyCode);
    g_variant_builder_add(builder, "s", *replyMessage);
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(builder);
  };
}


cOsdObject *cDBusRemote::MainMenuAction = NULL;

cDBusRemote::cDBusRemote(void)
:cDBusObject("/Remote", cDBusRemoteHelper::_xmlNodeInfo)
{
  AddMethod("CallPlugin", cDBusRemoteHelper::CallPlugin);
  AddMethod("Enable", cDBusRemoteHelper::Enable);
  AddMethod("Disable", cDBusRemoteHelper::Disable);
  AddMethod("Status", cDBusRemoteHelper::Status);
  AddMethod("HitKey", cDBusRemoteHelper::HitKey);
  AddMethod("HitKeys", cDBusRemoteHelper::HitKeys);
  AddMethod("AskUser", cDBusRemoteHelper::AskUser);
  AddMethod("SwitchChannel", cDBusRemoteHelper::SwitchChannel);
  AddMethod("GetVolume", cDBusRemoteHelper::GetVolume);
  AddMethod("SetVolume", cDBusRemoteHelper::SetVolume);
}

cDBusRemote::~cDBusRemote(void)
{
}
