#include "epg.h"
#include "common.h"
#include "helper.h"

#include <limits.h>

#include <vdr/eit.h>
#include <vdr/epg.h>
#include <vdr/svdrp.h>


cDBusMessageEPG::cDBusMessageEPG(cDBusMessageEPG::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageEPG::~cDBusMessageEPG(void)
{
}

void cDBusMessageEPG::Process(void)
{
  switch (_action) {
    case dmeDisableScanner:
      DisableScanner();
      break;
    case dmeEnableScanner:
      EnableScanner();
      break;
    case dmeClearEPG:
      ClearEPG();
      break;
    case dmePutEntry:
      PutEntry();
      break;
    case dmePutFile:
      PutFile();
      break;
    case dmeNow:
      GetEntries(dmmPresent);
      break;
    case dmeNext:
      GetEntries(dmmFollowing);
      break;
    }
}

void cDBusMessageEPG::DisableScanner(void)
{
#if APIVERSNUM >= 10711
  int eitDisableTime = 3600; // one hour should be ok as a default
  DBusMessageIter args;
  if (dbus_message_iter_init(_msg, &args)) {
     if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
        int s = 0;
        cDBusHelper::GetNextArg(args, DBUS_TYPE_INT32, &s);
        if (s > 0)
           eitDisableTime = s;
        }
     }
  cString msg = cString::sprintf("EIT scanner disabled for %d sseconds", eitDisableTime);
  isyslog("dbus2vdr: %s.DisableScanner: %s", DBUS_VDR_EPG_INTERFACE, *msg);
  cEitFilter::SetDisableUntil(time(NULL) + eitDisableTime);
  cDBusHelper::SendReply(_conn, _msg, 250, *msg);
#else
  esyslog("dbus2vdr: %s.DisableScanner: you need at least vdr 1.7.11", DBUS_VDR_EPG_INTERFACE);
  cDBusHelper::SendReply(_conn, _msg, 550, "you need at least vdr 1.7.11");
#endif
}

void cDBusMessageEPG::EnableScanner(void)
{
#if APIVERSNUM >= 10711
  isyslog("dbus2vdr: %s.EnableScanner: EIT scanner enabled", DBUS_VDR_EPG_INTERFACE);
  cEitFilter::SetDisableUntil(0);
  cDBusHelper::SendReply(_conn, _msg, 250, "EIT scanner enabled");
#else
  esyslog("dbus2vdr: %s.EnableScanner: you need at least vdr 1.7.11", DBUS_VDR_EPG_INTERFACE);
  cDBusHelper::SendReply(_conn, _msg, 550, "you need at least vdr 1.7.11");
#endif
}

void cDBusMessageEPG::ClearEPG(void)
{
  const char *channel = NULL;
  int eitDisableTime = 10; // seconds until EIT processing is enabled again after a CLRE command
  DBusMessageIter args;
  if (dbus_message_iter_init(_msg, &args)) {
     for (int argNr = 0; argNr < 2; argNr++) {
         if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
            cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &channel);
         else if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
            int s = 0;
            cDBusHelper::GetNextArg(args, DBUS_TYPE_INT32, &s);
            if (s > 0) {
               eitDisableTime = s;
               isyslog("dbus2vdr: %s.ClearEPG: using %d seconds as EIT disable timeout", DBUS_VDR_EPG_INTERFACE, eitDisableTime);
               }
            }
         }
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
              cDBusHelper::SendReply(_conn, _msg, 250, *replyMessage);
              }
           else {
              cString replyMessage = cString::sprintf("No EPG data found for channel \"%s\"", channel);
              cDBusHelper::SendReply(_conn, _msg, 550, *replyMessage);
              return;
              }
           }
        else
           cDBusHelper::SendReply(_conn, _msg, 451, "Can't get EPG data");
        }
     else {
        cString replyMessage = cString::sprintf("Undefined channel \"%s\"", channel);
        cDBusHelper::SendReply(_conn, _msg, 501, *replyMessage);
        }
     }
  else {
     cSchedules::ClearAll();
     #if APIVERSNUM >= 10711
     cEitFilter::SetDisableUntil(time(NULL) + eitDisableTime);
     #endif
     cDBusHelper::SendReply(_conn, _msg, 250, "EPG data cleared");
     }
}

void cDBusMessageEPG::PutEntry(void)
{
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args)) {
     cDBusHelper::SendReply(_conn, _msg, 501, "no lines for the epg entry given");
     return;
     }

  cPUTEhandler *handler = new cPUTEhandler();
  if (handler->Status() == 354) {
     const char *item = NULL;
     while (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &item) >= 0) {
           dsyslog("dbus2vdr: %s.PutEntry: item = %s", DBUS_VDR_EPG_INTERFACE, item);
           dsyslog("dbus2vdr: %s.PutEntry: status = %d, message = %s", DBUS_VDR_EPG_INTERFACE, handler->Status(), handler->Message());
           if (!handler->Process(item))
              break;
           }
     dsyslog("dbus2vdr: %s.PutEntry: status = %d, message = %s", DBUS_VDR_EPG_INTERFACE, handler->Status(), handler->Message());
     if (handler->Status() == 354)
        handler->Process(".");
     }
  cDBusHelper::SendReply(_conn, _msg, handler->Status(), handler->Message());
  delete handler;
}

void cDBusMessageEPG::PutFile(void)
{
  const char *filename = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.PutFile: message misses an argument for the filename", DBUS_VDR_EPG_INTERFACE);
  else {
     if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &filename) < 0)
        esyslog("dbus2vdr: %s.PutFile: 'filename' argument is not a string", DBUS_VDR_EPG_INTERFACE);
     }

  if (filename != NULL) {
     FILE *f = fopen(filename, "r");
     if (f) {
        cString message = cString::sprintf("start reading epg data from %s", filename);
        cDBusHelper::SendReply(_conn, _msg, 0, *message);
        if (cSchedules::Read(f))
           cSchedules::Cleanup(true);
        fclose(f);
        }
     else {
        esyslog("dbus2vdr: %s.PutFile: error opening %s", DBUS_VDR_EPG_INTERFACE, filename);
        cString message = cString::sprintf("error opening %s", filename);
        cDBusHelper::SendReply(_conn, _msg, -2, *message);
        }
     }
  else
     cDBusHelper::SendReply(_conn, _msg, -1, "no filename");
}

static void sAddEvent(DBusMessageIter &array, const cEvent &event)
{
  const char *c;
  dbus_uint32_t tu32;
  dbus_uint64_t tu64;
  int ti;
  dbus_bool_t tb;

  DBusMessageIter arr;
  if (!dbus_message_iter_open_container(&array, DBUS_TYPE_ARRAY, "(sv)", &arr))
     esyslog("dbus2vdr: sAddEvent: can't open array container");

  cString cid = event.ChannelID().ToString();
  c = *cid;
  cDBusHelper::AddKeyValue(arr, "ChannelID", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);

  tu32 = event.EventID();
  cDBusHelper::AddKeyValue(arr, "EventID", DBUS_TYPE_UINT32, DBUS_TYPE_UINT32_AS_STRING, &tu32);

  c = event.Title();
  if (c != NULL)
     cDBusHelper::AddKeyValue(arr, "Title", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);

  c = event.ShortText();
  if (c != NULL)
     cDBusHelper::AddKeyValue(arr, "ShortText", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);

  c = event.Description();
  if (c != NULL)
     cDBusHelper::AddKeyValue(arr, "Description", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);

  tu64 = event.StartTime();
  cDBusHelper::AddKeyValue(arr, "StartTime", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);

  tu64 = event.EndTime();
  cDBusHelper::AddKeyValue(arr, "EndTime", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);

  tu64 = event.Duration();
  cDBusHelper::AddKeyValue(arr, "Duration", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);

  tu64 = event.Vps();
  cDBusHelper::AddKeyValue(arr, "Vps", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);

  ti = event.RunningStatus();
  cDBusHelper::AddKeyValue(arr, "RunningStatus", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &ti);

  ti = event.ParentalRating();
  cDBusHelper::AddKeyValue(arr, "ParentalRating", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &ti);

  tb = event.HasTimer();
  cDBusHelper::AddKeyValue(arr, "HasTimer", DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &tb);

  for (int i = 0; i < MaxEventContents; i++) {
      tu32 = event.Contents(i);
      if (tu32 != 0) {
         cDBusHelper::AddKeyValue(arr, *cString::sprintf("ContentID[%d]", i), DBUS_TYPE_UINT32, DBUS_TYPE_UINT32_AS_STRING, &tu32);
         c = cEvent::ContentToString(tu32);
         cDBusHelper::AddKeyValue(arr, *cString::sprintf("Content[%d]", i), DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
         }
      }

  if (!dbus_message_iter_close_container(&array, &arr))
     esyslog("dbus2vdr: sAddEvent: can't close array container");
}

static bool sGetChannel(DBusMessageIter &args, const char **input, cChannel **channel)
{
  *channel = NULL;
  *input = NULL;
  if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, input) >= 0) {
     if (isnumber(*input))
        *channel = Channels.GetByNumber(strtol(*input, NULL, 10));
     else
        *channel = Channels.GetByChannelID(tChannelID::FromString(*input));
     if (*channel == NULL)
        return false;
     }
  return true;
}

void cDBusMessageEPG::GetEntries(eMode mode)
{
  cChannel *channel = NULL;
  DBusMessageIter args;
  if (dbus_message_iter_init(_msg, &args)) {
     const char *c = NULL;
     if (!sGetChannel(args, &c, &channel)) {
        cString reply = cString::sprintf("channel \"%s\" not defined", c);
        esyslog("dbus2vdr: %s.GetEntries: %s", DBUS_VDR_EPG_INTERFACE, *reply);
        cDBusHelper::SendReply(_conn, _msg, 501, *reply);
        return;
        }
     }

  cSchedulesLock sl(false, 1000);
  if (!sl.Locked()) {
     cDBusHelper::SendReply(_conn, _msg, 550, "got no lock on schedules");
     return;
     }

  const cSchedules *scheds = cSchedules::Schedules(sl);
  if (scheds == NULL) {
     cDBusHelper::SendReply(_conn, _msg, 550, "got no schedules");
     return;
     }

  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter rargs;
  dbus_message_iter_init_append(reply, &rargs);

  int returncode = 250;
  if (!dbus_message_iter_append_basic(&rargs, DBUS_TYPE_INT32, &returncode))
     esyslog("dbus2vdr: %s.GetEntries: out of memory while appending the return-code", DBUS_VDR_EPG_INTERFACE);

  const char *message = "";
  if (!dbus_message_iter_append_basic(&rargs, DBUS_TYPE_STRING, &message))
     esyslog("dbus2vdr: %s.GetEntries: out of memory while appending the reply-message", DBUS_VDR_EPG_INTERFACE);

  DBusMessageIter array;
  if (!dbus_message_iter_open_container(&rargs, DBUS_TYPE_ARRAY, "a(sv)", &array))
     esyslog("dbus2vdr: %s.GetEntries: can't open array container", DBUS_VDR_EPG_INTERFACE);

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

  if (!dbus_message_iter_close_container(&rargs, &array))
     esyslog("dbus2vdr: %s.GetEntries: can't close array container", DBUS_VDR_EPG_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
}


cDBusDispatcherEPG::cDBusDispatcherEPG(void)
:cDBusMessageDispatcher(DBUS_VDR_EPG_INTERFACE)
{
}

cDBusDispatcherEPG::~cDBusDispatcherEPG(void)
{
}

cDBusMessage *cDBusDispatcherEPG::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/EPG") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "DisableScanner"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeDisableScanner, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "EnableScanner"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeEnableScanner, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "ClearEPG"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeClearEPG, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "PutEntry"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmePutEntry, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "PutFile"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmePutFile, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "Now"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeNow, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "Next"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeNext, conn, msg);

  return NULL;
}

bool          cDBusDispatcherEPG::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/EPG") != 0)
     return false;
  Data =
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
  "  </interface>\n"
  "</node>\n";
  return true;
}
