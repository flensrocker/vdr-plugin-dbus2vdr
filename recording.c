#include "recording.h"
#include "common.h"
#include "helper.h"

#include <vdr/recording.h>


class cDBusRecordingActions
{
private:
  // own recordings list like in SVDRP
  // so Play has the right numbers
  static cRecordings recordings;

  static void AddRecording(DBusMessageIter &args, const cRecording *recording)
  {
    if (recording == NULL)
       return;

    DBusMessageIter struc;
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_STRUCT, NULL, &struc))
       esyslog("dbus2vdr: %s.AddRecording: can't open struct container", DBUS_VDR_RECORDING_INTERFACE);
    else {
       dbus_int32_t index = recording->Index() + 1;
       cDBusHelper::AddArg(struc, DBUS_TYPE_INT32, &index);
       DBusMessageIter array;
       if (!dbus_message_iter_open_container(&struc, DBUS_TYPE_ARRAY, "(sv)", &array))
          esyslog("dbus2vdr: %s.AddRecording: can't open array container", DBUS_VDR_RECORDING_INTERFACE);

       const char *c;
       dbus_bool_t b;

       c = recording->FileName();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Path", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       c = recording->Name();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Name", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       c = recording->Title();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Title", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       // TODO: Priority, Lifetime, FramesPerSecond, NumFrames, LengthInSeconds, FileSizeMB
       b = recording->IsPesRecording();
       cDBusHelper::AddKeyValue(array, "IsPesRecording", DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &b);

       if (!dbus_message_iter_close_container(&struc, &array))
          esyslog("dbus2vdr: %s.AddRecording: can't close array container", DBUS_VDR_RECORDING_INTERFACE);
       if (!dbus_message_iter_close_container(&args, &struc))
          esyslog("dbus2vdr: %s.AddRecording: can't close struct container", DBUS_VDR_RECORDING_INTERFACE);
       }
  };

public:
  static void Update(DBusConnection* conn, DBusMessage* msg)
  {
    Recordings.Update(false);
    cDBusHelper::SendReply(conn, msg, 250, "update of recordings triggered");
  };

  static void Get(DBusConnection* conn, DBusMessage* msg)
  {
    cDBusHelper::SendReply(conn, msg, 501, "Get is not implemented yet");
  };

  static void List(DBusConnection* conn, DBusMessage* msg)
  {
    recordings.Update(true);

    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    DBusMessageIter array;
    if (!dbus_message_iter_open_container(&replyArgs, DBUS_TYPE_ARRAY, "(ia(sv))", &array))
       esyslog("dbus2vdr: %s.List: can't open array container", DBUS_VDR_RECORDING_INTERFACE);
    for (cRecording *r = recordings.First(); r; r = recordings.Next(r))
        AddRecording(array, r);
    if (!dbus_message_iter_close_container(&replyArgs, &array))
       esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_RECORDING_INTERFACE);
    
    cDBusHelper::SendReply(conn, msg, reply);
  };

  static void PlayPath(DBusConnection* conn, DBusMessage* msg)
  {
    cDBusHelper::SendReply(conn, msg, 501, "PlayPath is not implemented yet");
  };
};

cRecordings cDBusRecordingActions::recordings;


cDBusDispatcherRecording::cDBusDispatcherRecording(void)
:cDBusMessageDispatcher(DBUS_VDR_RECORDING_INTERFACE)
{
  AddPath("/Recording"); // to be compatible with previous versions
  AddPath("/Recordings");
  AddAction("Update", cDBusRecordingActions::Update);
  AddAction("Get", cDBusRecordingActions::Get);
  AddAction("List", cDBusRecordingActions::List);
  AddAction("PlayPath", cDBusRecordingActions::PlayPath);
}

cDBusDispatcherRecording::~cDBusDispatcherRecording(void)
{
}

bool          cDBusDispatcherRecording::OnIntrospect(DBusMessage *msg, cString &Data)
{
  const char *path = dbus_message_get_path(msg);
  if ((strcmp(path, "/Recording") != 0) && (strcmp(path, "/Recordings") != 0))
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_RECORDING_INTERFACE"\">\n"
  "    <method name=\"Update\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Get\">\n"
  "      <arg name=\"option\"       type=\"i\" direction=\"in\"/>\n"
  "      <arg name=\"recording\"    type=\"(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"List\">\n"
  "      <arg name=\"recordings\"   type=\"a(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"PlayPath\">\n"
  "      <arg name=\"path\"         type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
