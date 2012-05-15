#include "setup.h"
#include "common.h"
#include "helper.h"

#include <vdr/config.h>
#include <vdr/device.h>
#include <vdr/epg.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>

cList<cDBusMessageSetup::cSetupBinding> cDBusMessageSetup::_bindings;

cDBusMessageSetup::cDBusMessageSetup(cDBusMessageSetup::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
  if (_bindings.Count() == 0) {
     _bindings.Add(cSetupBinding::NewString(Setup.OSDLanguage, "OSDLanguage", I18N_MAX_LOCALE_LEN));
     _bindings.Add(cSetupBinding::NewString(Setup.OSDSkin, "OSDSkin", MaxSkinName));
     _bindings.Add(cSetupBinding::NewString(Setup.OSDTheme, "OSDTheme", MaxThemeName));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.PrimaryDVB, "PrimaryDVB", 1, cDevice::NumDevices()));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ShowInfoOnChSwitch, "ShowInfoOnChSwitch", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.TimeoutRequChInfo, "TimeoutRequChInfo", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MenuScrollPage, "MenuScrollPage", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MenuScrollWrap, "MenuScrollWrap", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MenuKeyCloses, "MenuKeyCloses", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MarkInstantRecord, "MarkInstantRecord", 0, 1));
     _bindings.Add(cSetupBinding::NewString(Setup.NameInstantRecord, "NameInstantRecord", MaxFileName));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.InstantRecordTime, "InstantRecordTime", 1, 24 * 60 -1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.LnbSLOF, "LnbSLOF"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.LnbFrequLo, "LnbFrequLo"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.LnbFrequHi, "LnbFrequHi"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.DiSEqC, "DiSEqC", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SetSystemTime, "SetSystemTime", 0, 1));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.TimeSource, "TimeSource"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.TimeTransponder, "TimeTransponder"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MarginStart, "MarginStart"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MarginStop, "MarginStop"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.DisplaySubtitles, "DisplaySubtitles", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SubtitleOffset, "SubtitleOffset", -100, 100));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SubtitleFgTransparency, "SubtitleFgTransparency", 0, 9));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SubtitleBgTransparency, "SubtitleBgTransparency", 0, 10));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.EPGScanTimeout, "EPGScanTimeout"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.EPGBugfixLevel, "EPGBugfixLevel", 0, MAXEPGBUGFIXLEVEL));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.EPGLinger, "EPGLinger", 0));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SVDRPTimeout, "SVDRPTimeout"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ZapTimeout, "ZapTimeout"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ChannelEntryTimeout, "ChannelEntryTimeout", 0));
#if VDRVERSNUM < 10726
     _bindings.Add(cSetupBinding::NewInt32(&Setup.PrimaryLimit, "PrimaryLimit", 0, MAXPRIORITY));
#endif
     _bindings.Add(cSetupBinding::NewInt32(&Setup.DefaultPriority, "DefaultPriority", 0, MAXPRIORITY));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.DefaultLifetime, "DefaultLifetime", 0, MAXLIFETIME));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.PausePriority, "PausePriority", 0, MAXPRIORITY));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.PauseLifetime, "PauseLifetime", 0, MAXLIFETIME));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.PauseKeyHandling, "PauseKeyHandling"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.UseSubtitle, "UseSubtitle", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.UseVps, "UseVps", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.VpsMargin, "VpsMargin", 0));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.RecordingDirs, "RecordingDirs", 0, 1));
#if VDRVERSNUM >= 10712
     _bindings.Add(cSetupBinding::NewInt32(&Setup.FoldersInTimerMenu, "FoldersInTimerMenu", 0, 1));
#endif
#if VDRVERSNUM >= 10715
     _bindings.Add(cSetupBinding::NewInt32(&Setup.NumberKeysForChars, "NumberKeysForChars", 0, 1));
#endif
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.VideoDisplayFormat, "VideoDisplayFormat"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.VideoFormat, "VideoFormat"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.UpdateChannels, "UpdateChannels"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.UseDolbyDigital, "UseDolbyDigital", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ChannelInfoPos, "ChannelInfoPos", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ChannelInfoTime, "ChannelInfoTime", 1, 60));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.OSDLeft, "OSDLeft"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.OSDTop, "OSDTop"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.OSDWidth, "OSDWidth"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.OSDHeight, "OSDHeight"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.OSDMessageTime, "OSDMessageTime", 1, 60));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.UseSmallFont, "UseSmallFont"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.AntiAlias, "AntiAlias", 0, 1));
     _bindings.Add(cSetupBinding::NewString(Setup.FontOsd, "FontOsd", MAXFONTNAME));
     _bindings.Add(cSetupBinding::NewString(Setup.FontSml, "FontSml", MAXFONTNAME));
     _bindings.Add(cSetupBinding::NewString(Setup.FontFix, "FontFix", MAXFONTNAME));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.FontOsdSize, "FontOsdSize"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.FontSmlSize, "FontSmlSize"));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.FontFixSize, "FontFixSize"));
#if VDRVERSNUM >= 10704
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MaxVideoFileSize, "MaxVideoFileSize", MINVIDEOFILESIZE, MAXVIDEOFILESIZETS));
#endif
     _bindings.Add(cSetupBinding::NewInt32(&Setup.SplitEditedFiles, "SplitEditedFiles", 0, 1));
     //_bindings.Add(cSetupBinding::NewInt32(&Setup.DelTimeshiftRec, "DelTimeshiftRec"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MinEventTimeout, "MinEventTimeout"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MinUserInactivity, "MinUserInactivity"));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.MultiSpeedMode, "MultiSpeedMode", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ShowReplayMode, "ShowReplayMode", 0, 1));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ResumeID, "ResumeID", 0, 99));
     _bindings.Add(cSetupBinding::NewInt32(&Setup.InitialVolume, "InitialVolume", -1, 255));
#if VDRVERSNUM >= 10712
     _bindings.Add(cSetupBinding::NewInt32(&Setup.ChannelsWrap, "ChannelsWrap", 0, 1));
#endif
     _bindings.Add(cSetupBinding::NewInt32(&Setup.EmergencyExit, "EmergencyExit", 0, 1));
     }
}

cDBusMessageSetup::~cDBusMessageSetup(void)
{
}

void cDBusMessageSetup::Process(void)
{
  switch (_action) {
    case dmsList:
      List();
      break;
    case dmsGet:
      Get();
      break;
    case dmsSet:
      Set();
      break;
    }
}

void cDBusMessageSetup::List(void)
{
  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter args;
  DBusMessageIter array;
  DBusMessageIter element;
  DBusMessageIter variant;
  DBusMessageIter vstruct;
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(sv)", &array))
     esyslog("dbus2vdr: %s.List: can't open array container", DBUS_VDR_SETUP_INTERFACE);
  for (cSetupBinding *b = _bindings.First(); b; b = _bindings.Next(b)) {
      if (!dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &element))
         esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
      if (!dbus_message_iter_append_basic(&element, DBUS_TYPE_STRING, &b->Name))
         esyslog("dbus2vdr: %s.List: out of memory while appending the key name", DBUS_VDR_SETUP_INTERFACE);
      switch (b->Type) {
       case cSetupBinding::dstString:
        {
         if (!dbus_message_iter_open_container(&element, DBUS_TYPE_VARIANT, "(si)", &variant))
            esyslog("dbus2vdr: %s.List: can't open variant container", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL, &vstruct))
            esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
         const char *str = (const char*)b->Value;
         if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_STRING, &str))
            esyslog("dbus2vdr: %s.List: out of memory while appending the string value", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT32, &b->StrMaxLength))
            esyslog("dbus2vdr: %s.List: out of memory while appending the max string length value", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_close_container(&variant, &vstruct))
            esyslog("dbus2vdr: %s.List: can't close struct container", DBUS_VDR_SETUP_INTERFACE);
         break;
        }
       case cSetupBinding::dstInt32:
        {
         if (!dbus_message_iter_open_container(&element, DBUS_TYPE_VARIANT, "(iii)", &variant))
            esyslog("dbus2vdr: %s.List: can't open variant container", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL, &vstruct))
            esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
         int i32 = *(int*)(b->Value);
         if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT32, &i32))
            esyslog("dbus2vdr: %s.List: out of memory while appending the integer value", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT32, &b->Int32MinValue))
            esyslog("dbus2vdr: %s.List: out of memory while appending the min integer value", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT32, &b->Int32MaxValue))
            esyslog("dbus2vdr: %s.List: out of memory while appending the max integer value", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_close_container(&variant, &vstruct))
            esyslog("dbus2vdr: %s.List: can't close struct container", DBUS_VDR_SETUP_INTERFACE);
         break;
        }
       }
       if (!dbus_message_iter_close_container(&element, &variant))
          esyslog("dbus2vdr: %s.List: can't close variant container", DBUS_VDR_SETUP_INTERFACE);
       if (!dbus_message_iter_close_container(&array, &element))
          esyslog("dbus2vdr: %s.List: can't close struct container", DBUS_VDR_SETUP_INTERFACE);
      }
  if (!dbus_message_iter_close_container(&args, &array))
     esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_SETUP_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.List: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
  dbus_message_unref(reply);
  return;
}

void cDBusMessageSetup::Get(void)
{
  int rc;
  const char *name = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.Get: message misses an argument for the name", DBUS_VDR_SETUP_INTERFACE);
  else {
     rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name);
     if (rc < 0)
        esyslog("dbus2vdr: %s.Get: 'name' argument is not a string", DBUS_VDR_SETUP_INTERFACE);
     }

  dbus_int32_t replyCode = 501;
  cString replyMessage = "missing arguments";
  if (name != NULL) {
     char *point = (char*)strchr(name, '.');
     if (point) { // this is a plugin setting
        char *dummy = strdup(name);
        char *plugin = compactspace(dummy);
        point = strchr(plugin, '.');
        *point = 0;
        char *key = point + 1;
        isyslog("dbus2vdr: %s.Get: looking for %s.%s", DBUS_VDR_SETUP_INTERFACE, plugin, key);
        const char *value = NULL;
        for (cSetupLine *line = Setup.First(); line; line = Setup.Next(line)) {
            if (line->Plugin() == NULL)
               continue;
            if (strcasecmp(plugin, line->Plugin()) != 0)
               continue;
            if (strcasecmp(key, line->Name()) != 0)
               continue;
            value = line->Value();
            break;
            }
        free(dummy);
        if (value == NULL) {
           replyMessage = cString::sprintf("%s not found in setup.conf", name);
           esyslog("dbus2vdr: %s.Get: %s not found in setup.conf", DBUS_VDR_SETUP_INTERFACE, name);
           cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
           return;
           }
        replyCode = 900;
        replyMessage = cString::sprintf("getting %s", name);

        DBusMessage *reply = dbus_message_new_method_return(_msg);
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &value))
           esyslog("dbus2vdr: %s.Get: out of memory while appending the string value", DBUS_VDR_SETUP_INTERFACE);
        const char *message = replyMessage;
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
           esyslog("dbus2vdr: %s.Get: out of memory while appending the reply-message", DBUS_VDR_SETUP_INTERFACE);

        dbus_uint32_t serial = 0;
        if (!dbus_connection_send(_conn, reply, &serial))
           esyslog("dbus2vdr: %s.Get: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
        dbus_message_unref(reply);
        return;
        }

     replyMessage = cString::sprintf("%s is not yet implemented", name);
     for (cSetupBinding *b = _bindings.First(); b; b = _bindings.Next(b)) {
         if (strcasecmp(name, b->Name) == 0) {
            replyCode = 900;
            replyMessage = cString::sprintf("getting %s", name);

            DBusMessage *reply = dbus_message_new_method_return(_msg);
            DBusMessageIter args;
            dbus_message_iter_init_append(reply, &args);

            switch (b->Type) {
              case cSetupBinding::dstString:
               {
                const char *str = (const char*)b->Value;
                if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &str))
                   esyslog("dbus2vdr: %s.Get: out of memory while appending the string value", DBUS_VDR_SETUP_INTERFACE);
                break;
               }
              case cSetupBinding::dstInt32:
               {
                int i32 = *(int*)(b->Value);
                if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &i32))
                   esyslog("dbus2vdr: %s.Get: out of memory while appending the integer value", DBUS_VDR_SETUP_INTERFACE);
                break;
               }
              }

            if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &replyCode))
               esyslog("dbus2vdr: %s.Get: out of memory while appending the return-code", DBUS_VDR_SETUP_INTERFACE);

            const char *message = replyMessage;
            if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
               esyslog("dbus2vdr: %s.Get: out of memory while appending the reply-message", DBUS_VDR_SETUP_INTERFACE);

            dbus_uint32_t serial = 0;
            if (!dbus_connection_send(_conn, reply, &serial))
               esyslog("dbus2vdr: %s.Get: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
            dbus_message_unref(reply);
            return;
            }
         }
     }

  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}

void cDBusMessageSetup::Set(void)
{
  int rc;
  const char *name = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.Set: message misses an argument for the name", DBUS_VDR_SETUP_INTERFACE);
  else {
     rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name);
     if (rc < 0)
        esyslog("dbus2vdr: %s.Set: 'name' argument is not a string", DBUS_VDR_SETUP_INTERFACE);
     }

  dbus_int32_t replyCode = 501;
  cString replyMessage = "missing arguments";
  if (name != NULL) {
     char *point = (char*)strchr(name, '.');
     if (point) { // this is a plugin setting
        const char *str = NULL;
        rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &str);
        if (rc < 0)
           replyMessage = cString::sprintf("argument for %s is not a string", name);
        else {
           char *dummy = strdup(name);
           char *pluginName = compactspace(dummy);
           point = strchr(pluginName, '.');
           *point = 0;
           char *key = point + 1;
           isyslog("dbus2vdr: %s.Set: looking for %s.%s", DBUS_VDR_SETUP_INTERFACE, pluginName, key);
           cPlugin *plugin = cPluginManager::GetPlugin(pluginName);
           if (plugin == NULL) {
              replyMessage = cString::sprintf("plugin %s not loaded", pluginName);
              esyslog("dbus2vdr: %s.Set: plugin %s not loaded", DBUS_VDR_SETUP_INTERFACE, pluginName);
              }
           else {
              if (!plugin->SetupParse(key, str)) {
                 replyMessage = cString::sprintf("plugin %s can't parse %s = %s", pluginName, key, str);
                 esyslog("dbus2vdr: %s.Set: plugin %s can't parse %s = %s", DBUS_VDR_SETUP_INTERFACE, pluginName, key, str);
                 }
              else {
                 replyCode = 900;
                 replyMessage = cString::sprintf("storing %s = %s", name, str);
                 plugin->SetupStore(key, str);
                 Setup.Save();
                 }
              }
           free(dummy);
           }
        cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
        return;
        }

     replyMessage = cString::sprintf("%s is not yet implemented", name);
     bool save = false;
     for (cSetupBinding *b = _bindings.First(); b; b = _bindings.Next(b)) {
         if (strcasecmp(name, b->Name) == 0) {
            switch (b->Type) {
              case cSetupBinding::dstString:
               {
                const char *str = NULL;
                rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &str);
                if (rc < 0)
                   replyMessage = cString::sprintf("argument for %s is not a string", name);
                else {
                   replyMessage = cString::sprintf("setting %s = %s", name, str);
                   Utf8Strn0Cpy((char*)b->Value, str, b->StrMaxLength);
                   save = true;
                   }
                break;
               }
              case cSetupBinding::dstInt32:
               {
                int i32;
                rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_INT32, &i32);
                if (rc < 0)
                   replyMessage = cString::sprintf("argument for %s is not a 32bit-integer", name);
                else if ((i32 < b->Int32MinValue) || (i32 >= b->Int32MaxValue))
                   replyMessage = cString::sprintf("argument for %s is out of range", name);
                else {
                   replyMessage = cString::sprintf("setting %s = %d", name, i32);
                   (*((int*)b->Value)) = i32;
                   save = true;
                   }
                break;
               }
              }
            break;
            }
         }

     if (save) {
        Setup.Save();
        replyCode = 900;
        }
     }

  cDBusHelper::SendReply(_conn, _msg, replyCode, replyMessage);
}


cDBusDispatcherSetup::cDBusDispatcherSetup(void)
:cDBusMessageDispatcher(DBUS_VDR_SETUP_INTERFACE)
{
}

cDBusDispatcherSetup::~cDBusDispatcherSetup(void)
{
}

cDBusMessage *cDBusDispatcherSetup::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Setup") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_SETUP_INTERFACE, "List"))
     return new cDBusMessageSetup(cDBusMessageSetup::dmsList, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_SETUP_INTERFACE, "Get"))
     return new cDBusMessageSetup(cDBusMessageSetup::dmsGet, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_SETUP_INTERFACE, "Set"))
     return new cDBusMessageSetup(cDBusMessageSetup::dmsSet, conn, msg);

  return NULL;
}

bool          cDBusDispatcherSetup::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Setup") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_SETUP_INTERFACE"\">\n"
  "    <method name=\"List\">\n"
  "      <arg name=\"key_value_list\" type=\"a(sv)\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Get\">\n"
  "      <arg name=\"name\"         type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"value\"        type=\"v\" direction=\"out\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Set\">\n"
  "      <arg name=\"name\"         type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"value\"        type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
