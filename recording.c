#include "recording.h"
#include "common.h"
#include "helper.h"

#include <vdr/menu.h>
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
    // only update recordings list if empty
    if (recordings.Count() == 0)
       recordings.Update(true);
    cRecording *recording = NULL;
    DBusMessageIter args;
    if (dbus_message_iter_init(msg, &args)) {
       if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
          DBusMessageIter sub;
          dbus_message_iter_recurse(&args, &sub);
          if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
             const char *path = NULL;
             if ((cDBusHelper::GetNextArg(sub, DBUS_TYPE_STRING, &path) >= 0) && (path != NULL) && *path)
                recording = recordings.GetByName(path);
             }
          else if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_INT32) {
             int number = 0;
             if ((cDBusHelper::GetNextArg(sub, DBUS_TYPE_INT32, &number) >= 0) && (number > 0) && (number <= recordings.Count()))
                recording = recordings.Get(number - 1);
             }
          }
       }
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter replyArgs;
    dbus_message_iter_init_append(reply, &replyArgs);
    AddRecording(replyArgs, recording);
    cDBusHelper::SendReply(conn, reply);
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
    
    cDBusHelper::SendReply(conn, reply);
  };

  static void Play(DBusConnection* conn, DBusMessage* msg)
  {
    int replyCode = 501;
    cString replyMessage;
    cRecording *recording = NULL;
    int position = -1; // default: resume
    const char *hmsf = NULL;
    DBusMessageIter args;
    DBusMessageIter sub1;
    DBusMessageIter sub2;
    if (dbus_message_iter_init(msg, &args)) {
       DBusMessageIter *refArgs = &args;
       if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
          dbus_message_iter_recurse(&args, &sub1);
          refArgs = &sub1;
          }
       if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_STRING) {
          const char *path = NULL;
          dbus_message_iter_get_basic(refArgs, &path);
          if ((path != NULL) && *path) {
             recording = recordings.GetByName(path);
             if (recording == NULL) {
                recordings.Update(true);
                recording = recordings.GetByName(path);
                if (recording == NULL)
                   replyMessage = cString::sprintf("recording \"%s\" not found", path);
                }
             }
          }
       else if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_INT32) {
          int number = 0;
          dbus_message_iter_get_basic(refArgs, &number);
          if (number > 0) {
             recording = recordings.Get(number - 1);
             if (recording == NULL) {
                recordings.Update(true);
                recording = recordings.Get(number - 1);
                if (recording == NULL)
                   replyMessage = cString::sprintf("recording \"%d\" not found", number);
                }
             }
          }

       if (recording != NULL) {
          dbus_message_iter_next(&args);
          refArgs = &args;
          if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
             dbus_message_iter_recurse(&args, &sub2);
             refArgs = &sub2;
             }
          if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_STRING) {
             dbus_message_iter_get_basic(refArgs, &hmsf);
             if ((hmsf != NULL) && *hmsf) {
                if (strcasecmp(hmsf, "begin") == 0) {
                   position = 0;
                   hmsf = NULL;
                   }
                else
                   position = HMSFToIndex(hmsf, recording->FramesPerSecond());
                }
             else
                hmsf = NULL;
             }
          else if (dbus_message_iter_get_arg_type(refArgs) == DBUS_TYPE_INT32)
             dbus_message_iter_get_basic(refArgs, &position);
          }
       }

    if (recording != NULL) {
       replyCode = 250;
       cReplayControl::SetRecording(NULL);
       cControl::Shutdown();
       if (position >= 0) {
          cResumeFile resume(recording->FileName(), recording->IsPesRecording());
          if (position == 0)
             resume.Delete();
          else
             resume.Save(position);
          }
       cReplayControl::SetRecording(recording->FileName());
       cControl::Launch(new cReplayControl);
       cControl::Attach();

       if (hmsf != NULL)
          replyMessage = cString::sprintf("Playing recording \"%s\" from position %s", recording->FileName(), hmsf);
       else if (position >= 0)
          replyMessage = cString::sprintf("Playing recording \"%s\" from position %d", recording->FileName(), position);
       else
          replyMessage = cString::sprintf("Resume playing recording \"%s\"", recording->FileName());
       }
    cDBusHelper::SendReply(conn, msg, replyCode, *replyMessage);
  };
};

cRecordings cDBusRecordingActions::recordings;


cDBusDispatcherRecording::cDBusDispatcherRecording(void)
:cDBusMessageDispatcher(busSystem, DBUS_VDR_RECORDING_INTERFACE)
{
  AddPath("/Recording"); // to be compatible with previous versions
  AddPath("/Recordings");
  AddAction("Update", cDBusRecordingActions::Update);
  AddAction("Get", cDBusRecordingActions::Get);
  AddAction("List", cDBusRecordingActions::List);
  AddAction("Play", cDBusRecordingActions::Play);
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
  "      <arg name=\"number_or_path\" type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"recording\"      type=\"(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"List\">\n"
  "      <arg name=\"recordings\"   type=\"a(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Play\">\n"
  "      <arg name=\"number_or_path\" type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"begin\"          type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"      type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\"   type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
