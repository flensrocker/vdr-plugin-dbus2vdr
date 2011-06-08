#include "epg.h"
#include "common.h"
#include "helper.h"

#include <limits.h>

#include <vdr/eit.h>
#include <vdr/epg.h>


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
    case dmePutFile:
      PutFile();
      break;
    case dmeClearEPG:
      ClearEPG();
      break;
    }
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
               esyslog("dbus2vdr: %s.ClearEPG: using %d seconds as EIT disable timeout", DBUS_VDR_EPG_INTERFACE, eitDisableTime);
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

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "PutFile"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmePutFile, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "ClearEPG"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmeClearEPG, conn, msg);

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
  "    <method name=\"PutFile\">\n"
  "      <arg name=\"filename\"       type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"ClearEPG\">\n"
  "      <arg name=\"channel\"        type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"eitdisabletime\" type=\"i\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
