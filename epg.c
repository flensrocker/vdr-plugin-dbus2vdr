#include "epg.h"
#include "common.h"
#include "helper.h"

#include <limits.h>

#include <vdr/eit.h>
#include <vdr/epg.h>
#include <vdr/svdrp.h>


namespace cDBusEpgHelper
{
  enum eMode   { dmmAll, dmmPresent, dmmFollowing, dmmAtTime };

  static const char *_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_EPG_INTERFACE"\">\n"
    "    <method name=\"DisableScanner\">\n"
    "      <arg name=\"eitdisabletime\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"EnableScanner\">\n"
    "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"ClearEPG\">\n"
    "      <arg name=\"channel\"        type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"eitdisabletime\" type=\"i\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"PutEntry\">\n"
    "      <arg name=\"entryline\"      type=\"as\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"PutFile\">\n"
    "      <arg name=\"filename\"       type=\"s\" direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Now\">\n"
    "      <arg name=\"channel\"        type=\"s\"  direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "      <arg name=\"event_list\"     type=\"aa(sv)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"Next\">\n"
    "      <arg name=\"channel\"        type=\"s\"  direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "      <arg name=\"event_list\"     type=\"aa(sv)\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"At\">\n"
    "      <arg name=\"channel\"        type=\"s\"  direction=\"in\"/>\n"
    "      <arg name=\"time\"           type=\"t\"  direction=\"in\"/>\n"
    "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
    "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
    "      <arg name=\"event_list\"     type=\"aa(sv)\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

  static void DisableScanner(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
  #if APIVERSNUM >= 10711
    int eitDisableTime = 3600; // one hour should be ok as a default
    int s = 0;
    g_variant_get(Parameters, "(i)", &s);
    if (s > 0)
       eitDisableTime = s;
    cString replyMessage = cString::sprintf("EIT scanner disabled for %d sseconds", eitDisableTime);
    isyslog("dbus2vdr: %s.DisableScanner: %s", DBUS_VDR_EPG_INTERFACE, *replyMessage);
    cEitFilter::SetDisableUntil(time(NULL) + eitDisableTime);
    cDBusHelper::SendReply(Invocation, 250, *replyMessage);
  #else
    esyslog("dbus2vdr: %s.DisableScanner: you need at least vdr 1.7.11", DBUS_VDR_EPG_INTERFACE);
    cDBusHelper::SendReply(Invocation, 550, "you need at least vdr 1.7.11");
  #endif
  };

  static void EnableScanner(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
  #if APIVERSNUM >= 10711
    isyslog("dbus2vdr: %s.EnableScanner: EIT scanner enabled", DBUS_VDR_EPG_INTERFACE);
    cEitFilter::SetDisableUntil(0);
    cDBusHelper::SendReply(Invocation, 250, "EIT scanner enabled");
  #else
    esyslog("dbus2vdr: %s.EnableScanner: you need at least vdr 1.7.11", DBUS_VDR_EPG_INTERFACE);
    cDBusHelper::SendReply(Invocation, 550, "you need at least vdr 1.7.11");
  #endif
  };

  static void ClearEPG(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *channel = NULL;
    int timeout = 0;
    g_variant_get(Parameters, "(&si)", &channel, &timeout);
    int eitDisableTime = 10; // seconds until EIT processing is enabled again after a CLRE command
    if (timeout > 0) {
       eitDisableTime = timeout;
       isyslog("dbus2vdr: %s.ClearEPG: using %d seconds as EIT disable timeout", DBUS_VDR_EPG_INTERFACE, eitDisableTime);
       }

    if (channel) {
       tChannelID ChannelID = tChannelID::InvalidID;
       if (isnumber(channel)) {
          int o = strtol(channel, NULL, 10);
          if (o >= 1 && o <= Channels.MaxNumber())
             ChannelID = Channels.GetByNumber(o)->GetChannelID();
          }
       else {
          ChannelID = tChannelID::FromString(channel);
          if (ChannelID == tChannelID::InvalidID) {
             for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                 if (!Channel->GroupSep()) {
                    if (strcasecmp(Channel->Name(), channel) == 0) {
                       ChannelID = Channel->GetChannelID();
                       break;
                       }
                    }
                 }
             }
          }
       if (!(ChannelID == tChannelID::InvalidID)) {
          cSchedulesLock SchedulesLock(true, 1000);
          cSchedules *s = (cSchedules *)cSchedules::Schedules(SchedulesLock);
          if (s) {
             cSchedule *Schedule = NULL;
             ChannelID.ClrRid();
             for (cSchedule *p = s->First(); p; p = s->Next(p)) {
                 if (p->ChannelID() == ChannelID) {
                    Schedule = p;
                    break;
                    }
                 }
             if (Schedule) {
                Schedule->Cleanup(INT_MAX);
                #if APIVERSNUM >= 10711
                cEitFilter::SetDisableUntil(time(NULL) + eitDisableTime);
                #endif
                cString replyMessage = cString::sprintf("EPG data of channel \"%s\" cleared", channel);
                cDBusHelper::SendReply(Invocation, 250, *replyMessage);
                }
             else {
                cString replyMessage = cString::sprintf("No EPG data found for channel \"%s\"", channel);
                cDBusHelper::SendReply(Invocation, 550, *replyMessage);
                }
             }
          else
             cDBusHelper::SendReply(Invocation, 451, "Can't get EPG data");
          }
       else {
          cString replyMessage = cString::sprintf("Undefined channel \"%s\"", channel);
          cDBusHelper::SendReply(Invocation, 501, *replyMessage);
          }
       }
    else {
       cSchedules::ClearAll();
       #if APIVERSNUM >= 10711
       cEitFilter::SetDisableUntil(time(NULL) + eitDisableTime);
       #endif
       cDBusHelper::SendReply(Invocation, 250, "EPG data cleared");
       }
  };

  static void PutEntry(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    gsize len = 0;
    GVariant *array = g_variant_get_child_value(Parameters, 0);
    const gchar **line = g_variant_get_strv(array, &len);
    if (len < 1) {
       g_free(line);
       cDBusHelper::SendReply(Invocation, 501, "at least one element must be given");
       return;
       }

    cPUTEhandler *handler = new cPUTEhandler();
    if (handler->Status() == 354) {
       for (gsize i = 0; i < len; i++) {
           d4syslog("dbus2vdr: %s.PutEntry: item = %s", DBUS_VDR_EPG_INTERFACE, line[i]);
           d4syslog("dbus2vdr: %s.PutEntry: status = %d, message = %s", DBUS_VDR_EPG_INTERFACE, handler->Status(), handler->Message());
           if (!handler->Process(line[i]))
              break;
           }
       d4syslog("dbus2vdr: %s.PutEntry: status = %d, message = %s", DBUS_VDR_EPG_INTERFACE, handler->Status(), handler->Message());
       if (handler->Status() == 354)
          handler->Process(".");
       }
    cDBusHelper::SendReply(Invocation, handler->Status(), handler->Message());
    delete handler;
    g_free(line);
  };

  static void PutFile(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    const char *filename = NULL;
    g_variant_get(Parameters, "(&s)", &filename);

    if ((filename != NULL) && (*filename != 0)) {
       FILE *f = fopen(filename, "r");
       if (f) {
          cString message = cString::sprintf("start reading epg data from %s", filename);
          cDBusHelper::SendReply(Invocation, 0, *message);
          if (cSchedules::Read(f))
             cSchedules::Cleanup(true);
          fclose(f);
          }
       else {
          esyslog("dbus2vdr: %s.PutFile: error opening %s", DBUS_VDR_EPG_INTERFACE, filename);
          cString message = cString::sprintf("error opening %s", filename);
          cDBusHelper::SendReply(Invocation, -2, *message);
          }
       }
    else
       cDBusHelper::SendReply(Invocation, -1, "no filename");
  };

  static void sAddEvent(GVariantBuilder *Array, const cEvent &Event)
  {
    const char *c;
    guint32 tu32;
    guint64 tu64;
    int ti;
    gboolean tb;

    GVariantBuilder *arr = g_variant_builder_new(G_VARIANT_TYPE("a(sv)"));

    cString cid = Event.ChannelID().ToString();
    c = *cid;
    cDBusHelper::AddKeyValue(arr, "ChannelID", "s", (void**)&c);

    tu32 = Event.EventID();
    cDBusHelper::AddKeyValue(arr, "EventID", "u", (void**)&tu32);

    c = Event.Title();
    if (c != NULL)
       cDBusHelper::AddKeyValue(arr, "Title", "s", (void**)&c);

    c = Event.ShortText();
    if (c != NULL)
       cDBusHelper::AddKeyValue(arr, "ShortText", "s", (void**)&c);

    c = Event.Description();
    if (c != NULL)
       cDBusHelper::AddKeyValue(arr, "Description", "s", (void**)&c);

    tu64 = Event.StartTime();
    cDBusHelper::AddKeyValue(arr, "StartTime", "t", (void**)&tu64);

    tu64 = Event.EndTime();
    cDBusHelper::AddKeyValue(arr, "EndTime", "t", (void**)&tu64);

    tu64 = Event.Duration();
    cDBusHelper::AddKeyValue(arr, "Duration", "t", (void**)&tu64);

    tu64 = Event.Vps();
    cDBusHelper::AddKeyValue(arr, "Vps", "t", (void**)&tu64);

    ti = Event.RunningStatus();
    cDBusHelper::AddKeyValue(arr, "RunningStatus", "i", (void**)&ti);

  #if VDRVERSNUM >= 10711
    ti = Event.ParentalRating();
    cDBusHelper::AddKeyValue(arr, "ParentalRating", "i", (void**)&ti);

    tb = Event.HasTimer();
    cDBusHelper::AddKeyValue(arr, "HasTimer", "b", (void**)&tb);

    for (int i = 0; i < MaxEventContents; i++) {
        tu32 = Event.Contents(i);
        if (tu32 != 0) {
           cDBusHelper::AddKeyValue(arr, *cString::sprintf("ContentID[%d]", i), "u", (void**)&tu32);
           c = cEvent::ContentToString(tu32);
           cDBusHelper::AddKeyValue(arr, *cString::sprintf("Content[%d]", i), "s", (void**)&c);
           }
        }
  #endif

    g_variant_builder_add_value(Array, g_variant_builder_end(arr));
    g_variant_builder_unref(arr);
  }

  static bool sGetChannel(GVariant *Arg, const char **Input, cChannel **Channel)
  {
    *Channel = NULL;
    *Input = NULL;
    if (g_variant_is_of_type(Arg, G_VARIANT_TYPE_STRING)) {
       g_variant_get(Arg, "&s", Input);
       if (**Input == 0)
          return true;
       if (isnumber(*Input))
          *Channel = Channels.GetByNumber(strtol(*Input, NULL, 10));
       else
          *Channel = Channels.GetByChannelID(tChannelID::FromString(*Input));
       if (*Channel == NULL)
          return false;
       }
    return true;
  }

  static void sReturnError(GDBusMethodInvocation *Invocation, int  ReplyCode, const char *ReplyMessage)
  {
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(isaa(sv))"));
    g_variant_builder_add(builder, "i", ReplyCode);
    g_variant_builder_add(builder, "s", ReplyMessage);
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("aa(sv)"));
    GVariantBuilder *innerArray = g_variant_builder_new(G_VARIANT_TYPE("a(sv)"));
    g_variant_builder_add_value(array, g_variant_builder_end(innerArray));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(innerArray);
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  };
  
  static void sGetEntries(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation, eMode mode)
  {
    cChannel *channel = NULL;
    guint64 atTime = 0;
    GVariant *first = g_variant_get_child_value(Parameters, 0);
    
    const char *c = NULL;
    if (!sGetChannel(first, &c, &channel)) {
       cString reply = cString::sprintf("channel \"%s\" not defined", c);
       esyslog("dbus2vdr: %s.GetEntries: %s", DBUS_VDR_EPG_INTERFACE, *reply);
       sReturnError(Invocation, 501, *reply);
       g_variant_unref(first);
       return;
       }
    if (mode == dmmAtTime) {
       GVariant *second = g_variant_get_child_value(Parameters, 1);
       g_variant_get(second, "t", &atTime);
       g_variant_unref(second);
       if (atTime == 0) {
          cString reply = cString::sprintf("missing time");
          esyslog("dbus2vdr: %s.GetEntries: %s", DBUS_VDR_EPG_INTERFACE, *reply);
          sReturnError(Invocation, 501, *reply);
          g_variant_unref(first);
          return;
          }
       }
    g_variant_unref(first);

    cSchedulesLock sl(false, 1000);
    if (!sl.Locked()) {
       sReturnError(Invocation, 550, "got no lock on schedules");
       return;
       }

    const cSchedules *scheds = cSchedules::Schedules(sl);
    if (scheds == NULL) {
       sReturnError(Invocation, 550, "got no schedules");
       return;
       }

    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(isaa(sv))"));
    g_variant_builder_add(builder, "i", 250);
    g_variant_builder_add(builder, "s", "");
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("aa(sv)"));

    bool next = false;
    if (channel == NULL) {
       channel = Channels.First();
       next = true;
       }

    while (channel) {
          const cSchedule *s = scheds->GetSchedule(channel, false);
          if (s != NULL) {
             const cEvent *e = NULL;
             switch (mode) {
               case dmmPresent:
                 e = s->GetPresentEvent();
                 break;
               case dmmFollowing:
                 e = s->GetFollowingEvent();
                 break;
               case dmmAtTime:
                 e = s->GetEventAround(atTime);
                 break;
               default:
                 e = NULL;
                 break;
               }
             if (e != NULL)
                sAddEvent(array, *e);
             }
          if (next)
             channel = Channels.Next(channel);
          else
             break;
          }

    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  };

  static void Now(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    sGetEntries(Object, Parameters, Invocation, dmmPresent);
  };

  static void Next(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    sGetEntries(Object, Parameters, Invocation, dmmFollowing);
  };

  static void At(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    sGetEntries(Object, Parameters, Invocation, dmmAtTime);
  };
}

cDBusEpg::cDBusEpg(void)
:cDBusObject("/EPG", cDBusEpgHelper::_xmlNodeInfo)
{
  AddMethod("DisableScanner", cDBusEpgHelper::DisableScanner);
  AddMethod("EnableScanner", cDBusEpgHelper::EnableScanner);
  AddMethod("ClearEPG", cDBusEpgHelper::ClearEPG);
  AddMethod("PutEntry", cDBusEpgHelper::PutEntry);
  AddMethod("PutFile", cDBusEpgHelper::PutFile);
  AddMethod("Now", cDBusEpgHelper::Now);
  AddMethod("Next", cDBusEpgHelper::Next);
  AddMethod("At", cDBusEpgHelper::At);
}

cDBusEpg::~cDBusEpg(void)
{
}
