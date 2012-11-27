#include "setup.h"
#include "common.h"
#include "helper.h"

#include <limits.h>

#include <vdr/config.h>
#include <vdr/device.h>
#include <vdr/epg.h>
#include <vdr/plugin.h>
#include <vdr/recording.h>
#include <vdr/themes.h>


cSetupLine *FindSetupLine(cConfig<cSetupLine>& config, const char *name, const char *plugin)
{
  if (name != NULL) {
     cSetupLine *sl = config.First();
     while (sl != NULL) {
           if (((sl->Plugin() == NULL) == (plugin == NULL))
            && ((plugin == NULL) || (strcasecmp(sl->Plugin(), plugin) == 0))
            && (strcasecmp(sl->Name(), name) == 0))
              return sl;
           sl = config.Next(sl);
           }
     }
  return NULL;
}

class cDBusSetupActions
{
private:
  class cSetupBinding : public cListObject
  {
  private:
    cSetupBinding() {}

  public:
    enum eType { dstString, dstInt32, dstTimeT };

    const char *Name;
    eType Type;
    void *Value;
    // String
    int StrMaxLength;
    // Int32
    int Int32MinValue;
    int Int32MaxValue;

    virtual int Compare(const cListObject &ListObject) const
    {
      const cSetupBinding *sb = (cSetupBinding*)&ListObject;
      return strcasecmp(Name, sb->Name);
    }
    
    static cSetupBinding *NewString(void* value, const char *name, int maxLength)
    {
      cSetupBinding *b = new cSetupBinding();
      b->Name = name;
      b->Type = dstString;
      b->Value = value;
      b->StrMaxLength = maxLength;
      return b;
    }

    static cSetupBinding *NewInt32(void* value, const char *name, int minValue = 0, int maxValue = INT_MAX)
    {
      cSetupBinding *b = new cSetupBinding();
      b->Name = name;
      b->Type = dstInt32;
      b->Value = value;
      b->Int32MinValue = minValue;
      b->Int32MaxValue = maxValue;
      return b;
    }

    static cSetupBinding *NewTimeT(void* value, const char *name)
    {
      cSetupBinding *b = new cSetupBinding();
      b->Name = name;
      b->Type = dstTimeT;
      b->Value = value;
      return b;
    }

    static cSetupBinding* Find(const cList<cSetupBinding>& bindings, const char *name)
    {
      if (name == NULL)
         return NULL;
      cSetupBinding *sb = bindings.First();
      while ((sb != NULL) && (strcasecmp(sb->Name, name) != 0))
            sb = bindings.Next(sb);
      return sb;
    }
  };
  static cList<cSetupBinding> _bindings;

public:
  static void InitBindings(void)
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
       _bindings.Add(cSetupBinding::NewTimeT(&Setup.NextWakeupTime, "NextWakeupTime"));
       _bindings.Add(cSetupBinding::NewInt32(&Setup.MultiSpeedMode, "MultiSpeedMode", 0, 1));
       _bindings.Add(cSetupBinding::NewInt32(&Setup.ShowReplayMode, "ShowReplayMode", 0, 1));
       _bindings.Add(cSetupBinding::NewInt32(&Setup.ResumeID, "ResumeID", 0, 99));
       _bindings.Add(cSetupBinding::NewInt32(&Setup.InitialVolume, "InitialVolume", -1, 255));
#if VDRVERSNUM >= 10712
       _bindings.Add(cSetupBinding::NewInt32(&Setup.ChannelsWrap, "ChannelsWrap", 0, 1));
#endif
       _bindings.Add(cSetupBinding::NewInt32(&Setup.EmergencyExit, "EmergencyExit", 0, 1));

       _bindings.Sort();
       }
  }

  static void List(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
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
         case cSetupBinding::dstTimeT:
          {
           if (!dbus_message_iter_open_container(&element, DBUS_TYPE_VARIANT, "(x)", &variant))
              esyslog("dbus2vdr: %s.List: can't open variant container", DBUS_VDR_SETUP_INTERFACE);
           if (!dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL, &vstruct))
              esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
           time_t i64 = *(time_t*)(b->Value);
           if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT64, &i64))
              esyslog("dbus2vdr: %s.List: out of memory while appending the integer value", DBUS_VDR_SETUP_INTERFACE);
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
    int nolimit = -1;
    cString name;
    for (cSetupLine *line = Setup.First(); line; line = Setup.Next(line)) {
        // output all plugins and unknown settings
        if ((line->Plugin() == NULL) && (cSetupBinding::Find(_bindings, line->Name()) != NULL))
           continue;
        if (!dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &element))
           esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
        if (line->Plugin() == NULL)
           name = cString::sprintf("%s", line->Name());
        else
           name = cString::sprintf("%s.%s", line->Plugin(), line->Name());
        const char *str = *name;
        if (!dbus_message_iter_append_basic(&element, DBUS_TYPE_STRING, &str))
           esyslog("dbus2vdr: %s.List: out of memory while appending the key name", DBUS_VDR_SETUP_INTERFACE);
        if (!dbus_message_iter_open_container(&element, DBUS_TYPE_VARIANT, "(si)", &variant))
           esyslog("dbus2vdr: %s.List: can't open variant container", DBUS_VDR_SETUP_INTERFACE);
        if (!dbus_message_iter_open_container(&variant, DBUS_TYPE_STRUCT, NULL, &vstruct))
           esyslog("dbus2vdr: %s.List: can't open struct container", DBUS_VDR_SETUP_INTERFACE);
        str = line->Value();
        if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_STRING, &str))
           esyslog("dbus2vdr: %s.List: out of memory while appending the string value", DBUS_VDR_SETUP_INTERFACE);
        if (!dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_INT32, &nolimit))
           esyslog("dbus2vdr: %s.List: out of memory while appending the max string length value", DBUS_VDR_SETUP_INTERFACE);
        if (!dbus_message_iter_close_container(&variant, &vstruct))
           esyslog("dbus2vdr: %s.List: can't close struct container", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_close_container(&element, &variant))
            esyslog("dbus2vdr: %s.List: can't close variant container", DBUS_VDR_SETUP_INTERFACE);
         if (!dbus_message_iter_close_container(&array, &element))
            esyslog("dbus2vdr: %s.List: can't close struct container", DBUS_VDR_SETUP_INTERFACE);
        }
    if (!dbus_message_iter_close_container(&args, &array))
       esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_SETUP_INTERFACE);

    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.List: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
    dbus_message_unref(reply);
    return;
  }

  static void Get(DBusConnection* conn, DBusMessage* msg)
  {
    int rc;
    const char *name = NULL;
    DBusMessageIter args;
    if (!dbus_message_iter_init(msg, &args))
       esyslog("dbus2vdr: %s.Get: message misses an argument for the name", DBUS_VDR_SETUP_INTERFACE);
    else {
       rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name);
       if (rc < 0)
          esyslog("dbus2vdr: %s.Get: 'name' argument is not a string", DBUS_VDR_SETUP_INTERFACE);
       }

    dbus_int32_t replyCode = 501;
    cString replyMessage = "missing arguments";
    if (name != NULL) {
       cSetupBinding *b = cSetupBinding::Find(_bindings, name);
       if (b == NULL) {
          char *point = (char*)strchr(name, '.');
          char *plugin = NULL;
          const char *key = NULL;
          char *dummy = NULL;
          if (point == NULL) { // this is an unknown setting
             key = name;
             isyslog("dbus2vdr: %s.Get: looking for %s", DBUS_VDR_SETUP_INTERFACE, key);
             }
          else { // this is a plugin setting
             dummy = strdup(name);
             plugin = compactspace(dummy);
             point = strchr(plugin, '.');
             *point = 0;
             key = point + 1;
             isyslog("dbus2vdr: %s.Get: looking for %s.%s", DBUS_VDR_SETUP_INTERFACE, plugin, key);
             }
          const char *value = NULL;
          for (cSetupLine *line = Setup.First(); line; line = Setup.Next(line)) {
              if ((line->Plugin() == NULL) != (plugin == NULL))
                 continue;
              if ((plugin != NULL) && (strcasecmp(plugin, line->Plugin()) != 0))
                 continue;
              if (strcasecmp(key, line->Name()) != 0)
                 continue;
              value = line->Value();
              break;
              }
          if (dummy != NULL)
             free(dummy);
          if (value == NULL) {
             replyMessage = cString::sprintf("%s not found in setup.conf", name);
             esyslog("dbus2vdr: %s.Get: %s not found in setup.conf", DBUS_VDR_SETUP_INTERFACE, name);
             cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
             return;
             }
          replyCode = 900;
          replyMessage = cString::sprintf("getting %s", name);

          DBusMessage *reply = dbus_message_new_method_return(msg);
          DBusMessageIter args;
          dbus_message_iter_init_append(reply, &args);
          if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &value))
             esyslog("dbus2vdr: %s.Get: out of memory while appending the string value", DBUS_VDR_SETUP_INTERFACE);
          const char *message = replyMessage;
          if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
             esyslog("dbus2vdr: %s.Get: out of memory while appending the reply-message", DBUS_VDR_SETUP_INTERFACE);

          dbus_uint32_t serial = 0;
          if (!dbus_connection_send(conn, reply, &serial))
             esyslog("dbus2vdr: %s.Get: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
          dbus_message_unref(reply);
          return;
          }

       replyCode = 900;
       replyMessage = cString::sprintf("getting %s", name);

       DBusMessage *reply = dbus_message_new_method_return(msg);
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
         case cSetupBinding::dstTimeT:
          {
           time_t i64 = *(time_t*)(b->Value);
           if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT64, &i64))
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
       if (!dbus_connection_send(conn, reply, &serial))
          esyslog("dbus2vdr: %s.Get: out of memory while sending the reply", DBUS_VDR_SETUP_INTERFACE);
       dbus_message_unref(reply);
       return;
       }

    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }

  static void Set(DBusConnection* conn, DBusMessage* msg)
  {
    int rc;
    const char *name = NULL;
    DBusMessageIter args;
    if (!dbus_message_iter_init(msg, &args))
       esyslog("dbus2vdr: %s.Set: message misses an argument for the name", DBUS_VDR_SETUP_INTERFACE);
    else {
       rc = cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name);
       if (rc < 0)
          esyslog("dbus2vdr: %s.Set: 'name' argument is not a string", DBUS_VDR_SETUP_INTERFACE);
       }

    dbus_int32_t replyCode = 501;
    cString replyMessage = "missing arguments";
    DBusMessageIter &argsref = args;
    DBusMessageIter sub;
    if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
       dbus_message_iter_recurse(&args, &sub);
       argsref = sub;
       }
    if (name != NULL) {
       cSetupBinding *b = cSetupBinding::Find(_bindings, name);
       if (b == NULL) {
          const char *value = NULL;
          rc = cDBusHelper::GetNextArg(argsref, DBUS_TYPE_STRING, &value);
          if (rc < 0)
             replyMessage = cString::sprintf("argument for %s is not a string", name);
          else {
             cPlugin *plugin = NULL;
             char *dummy = NULL;
             char *pluginName = NULL;
             const char *key = NULL;
             char *point = (char*)strchr(name, '.');
             if (point == NULL) {
                key = name;
                isyslog("dbus2vdr: %s.Set: looking for %s", DBUS_VDR_SETUP_INTERFACE, key);
                }
             else { // this is a plugin setting
                dummy = strdup(name);
                pluginName = compactspace(dummy);
                point = strchr(pluginName, '.');
                *point = 0;
                key = point + 1;
                isyslog("dbus2vdr: %s.Set: looking for %s.%s", DBUS_VDR_SETUP_INTERFACE, pluginName, key);
                plugin = cPluginManager::GetPlugin(pluginName);
                if (plugin == NULL)
                   isyslog("dbus2vdr: %s.Set: plugin %s not loaded, try to set it directly", DBUS_VDR_SETUP_INTERFACE, pluginName);
                }
             if (plugin == NULL) {
                // save vdr-setup, load it in own Setup-container
                // adjust value, save as setup.conf, reload vdr-setup
                cSetupLine *line = FindSetupLine(Setup, key, pluginName);
                if (line != NULL) {
                    isyslog("dbus2vdr: %s.Set: found %s%s%s = %s", DBUS_VDR_SETUP_INTERFACE, (line->Plugin() == NULL) ? "" : line->Plugin(), (line->Plugin() == NULL) ? "" : ".", line->Name(), line->Value());
                    Setup.Save();
                    char *filename = strdup(Setup.FileName());
                    cConfig<cSetupLine> tmpSetup;
                    tmpSetup.Load(filename, true);
                    line = FindSetupLine(tmpSetup, key, pluginName);
                    if (line != NULL) {
                       cSetupLine *newLine = new cSetupLine(key, value, pluginName);
                       tmpSetup.Add(newLine, line);
                       tmpSetup.Del(line);
                       tmpSetup.Save();
                       Setup.Load(filename);
                       }
                    free(filename);
                    }
                else {
                   isyslog("dbus2vdr: %s.Set: add new line to setup.conf: %s%s%s = %s", DBUS_VDR_SETUP_INTERFACE, (pluginName == NULL) ? "" : pluginName, (pluginName == NULL) ? "" : ".", key, value);
                   Setup.Add(new cSetupLine(key, value, pluginName));
                   Setup.Save();
                   }
                replyCode = 900;
                replyMessage = cString::sprintf("storing %s%s%s = %s", (pluginName == NULL) ? "" : pluginName, (pluginName == NULL) ? "" : ".", key, value);
                }
             else {
                if (!plugin->SetupParse(key, value)) {
                   replyMessage = cString::sprintf("plugin %s can't parse %s = %s", pluginName, key, value);
                   esyslog("dbus2vdr: %s.Set: plugin %s can't parse %s = %s", DBUS_VDR_SETUP_INTERFACE, pluginName, key, value);
                   }
                else {
                   replyCode = 900;
                   replyMessage = cString::sprintf("storing %s = %s", name, value);
                   plugin->SetupStore(key, value);
                   Setup.Save();
                   }
                }
             if (dummy != NULL)
                free(dummy);
             }
          cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
          return;
          }

       bool save = false;
       bool ModifiedAppearance = false;
       switch (b->Type) {
         case cSetupBinding::dstString:
          {
           const char *str = NULL;
           rc = cDBusHelper::GetNextArg(argsref, DBUS_TYPE_STRING, &str);
           if (rc < 0)
              replyMessage = cString::sprintf("argument for %s is not a string", name);
           else {
              replyMessage = cString::sprintf("setting %s = %s", name, str);
              Utf8Strn0Cpy((char*)b->Value, str, b->StrMaxLength);
              save = true;
              // special handling of some setup values
              if ((strcasecmp(name, "OSDLanguage") == 0)
               || (strcasecmp(name, "FontOsd") == 0)
               || (strcasecmp(name, "FontSml") == 0)
               || (strcasecmp(name, "FontFix") == 0)) {
                 ModifiedAppearance = true;
                 }
              else if (strcasecmp(name, "OSDSkin") == 0) {
                 Skins.SetCurrent(str);
                 ModifiedAppearance = true;
                 }
              else if (strcasecmp(name, "OSDTheme") == 0) {
                 cThemes themes;
                 themes.Load(Skins.Current()->Name());
                 if ((themes.NumThemes() > 0) && Skins.Current()->Theme()) {
                    int themeIndex = themes.GetThemeIndex(str);
                    if (themeIndex >= 0) {
                       Skins.Current()->Theme()->Load(themes.FileName(themeIndex));
                       ModifiedAppearance = true;
                       }
                    }
                 }
             }
           break;
          }
         case cSetupBinding::dstInt32:
          {
           int i32;
           rc = cDBusHelper::GetNextArg(argsref, DBUS_TYPE_INT32, &i32);
           if (rc < 0)
              replyMessage = cString::sprintf("argument for %s is not a 32bit-integer", name);
           else if ((i32 < b->Int32MinValue) || (i32 > b->Int32MaxValue))
              replyMessage = cString::sprintf("argument for %s is out of range", name);
           else {
              replyMessage = cString::sprintf("setting %s = %d", name, i32);
              (*((int*)b->Value)) = i32;
              save = true;
              if (strcasecmp(name, "AntiAlias") == 0) {
                 ModifiedAppearance = true;
                 }
              }
           break;
          }
         case cSetupBinding::dstTimeT:
          {
           time_t i64;
           rc = cDBusHelper::GetNextArg(argsref, DBUS_TYPE_INT64, &i64);
           if (rc < 0)
              replyMessage = cString::sprintf("argument for %s is not a 64bit-integer", name);
           else {
              replyMessage = cString::sprintf("setting %s = %ld", name, i64);
              (*((time_t*)b->Value)) = i64;
              save = true;
              }
           break;
          }
         }

       if (save) {
          Setup.Save();
          replyCode = 900;
#if VDRVERSNUM > 10706
          if (ModifiedAppearance)
             cOsdProvider::UpdateOsdSize(true);
#endif
          }
       }

    cDBusHelper::SendReply(conn, msg, replyCode, replyMessage);
  }

  static void Del(DBusConnection* conn, DBusMessage* msg)
  {
    const char *name = NULL;
    if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID)) {
       esyslog("dbus2vdr: %s.Del: message misses an argument for the name", DBUS_VDR_SETUP_INTERFACE);
       cDBusHelper::SendReply(conn, msg, 501, "message misses an argument for the name");
       return;
       }

    isyslog("dbus2vdr: %s.Del: %s", DBUS_VDR_SETUP_INTERFACE, name);

    if (endswith(name, ".*")) {
       // delete all plugin settings
       char *plugin = strdup(name);
       plugin[strlen(name) - 2] = 0;
       cSetupLine *line = Setup.First();
       cSetupLine *next;
       while (line) {
             next = Setup.Next(line);
             if ((line->Plugin() != NULL) && (strcasecmp(line->Plugin(), plugin) == 0)) {
                isyslog("dbus2vdr: %s.Del: deleting %s.%s = %s", DBUS_VDR_SETUP_INTERFACE, line->Plugin(), line->Name(), line->Value());
                Setup.Del(line);
                }
             line = next;
             }
       Setup.Save();
       cDBusHelper::SendReply(conn, msg, 900, *cString::sprintf("deleted all settings for plugin %s", plugin));
       free(plugin);
       return;
       }

    cSetupLine delLine;
    if (!delLine.Parse((char*)*cString::sprintf("%s=", name))) {
       esyslog("dbus2vdr: %s.Del: can't parse %s", DBUS_VDR_SETUP_INTERFACE, name);
       cDBusHelper::SendReply(conn, msg, 501, *cString::sprintf("can't parse %s", name));
       return;
       }

    for (cSetupLine *line = Setup.First(); line; line = Setup.Next(line)) {
        if (line->Compare(delLine) == 0) {
           Setup.Del(line);
           Setup.Save();
           cDBusHelper::SendReply(conn, msg, 900, *cString::sprintf("%s deleted from setup.conf", name));
           return;
           }
        }

    cDBusHelper::SendReply(conn, msg, 550, *cString::sprintf("%s not found in setup.conf", name));
  }
};

cList<cDBusSetupActions::cSetupBinding> cDBusSetupActions::_bindings;


cDBusDispatcherSetup::cDBusDispatcherSetup(void)
:cDBusMessageDispatcher(DBUS_VDR_SETUP_INTERFACE)
{
  cDBusSetupActions::InitBindings();
  AddPath("/Setup");
  AddAction("List", cDBusSetupActions::List);
  AddAction("Get", cDBusSetupActions::Get);
  AddAction("Set", cDBusSetupActions::Set);
  AddAction("Del", cDBusSetupActions::Del);
}

cDBusDispatcherSetup::~cDBusDispatcherSetup(void)
{
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
  "    <method name=\"Del\">\n"
  "      <arg name=\"name\"         type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
