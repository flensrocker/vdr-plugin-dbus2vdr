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

       cString s;
       const char *c;
       dbus_bool_t b;
       dbus_uint64_t tu64;
       int i;
       double d;

       c = recording->FileName();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Path", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       c = recording->Name();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Name", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       c = recording->Title();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Title", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
       tu64 = recording->Start();
       if (tu64 > 0)
          cDBusHelper::AddKeyValue(array, "Start", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);
       tu64 = recording->Deleted();
       if (tu64 > 0)
          cDBusHelper::AddKeyValue(array, "Deleted", DBUS_TYPE_UINT64, DBUS_TYPE_UINT64_AS_STRING, &tu64);
       i = recording->Priority();
       cDBusHelper::AddKeyValue(array, "Priority", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       i = recording->Lifetime();
       cDBusHelper::AddKeyValue(array, "Lifetime", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       i = recording->HierarchyLevels();
       cDBusHelper::AddKeyValue(array, "HierarchyLevels", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       d = recording->FramesPerSecond();
       cDBusHelper::AddKeyValue(array, "FramesPerSecond", DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &d);
       i = recording->NumFrames();
       cDBusHelper::AddKeyValue(array, "NumFrames", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       i = recording->LengthInSeconds();
       cDBusHelper::AddKeyValue(array, "LengthInSeconds", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       i = recording->FileSizeMB();
       cDBusHelper::AddKeyValue(array, "FileSizeMB", DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
       b = recording->IsPesRecording();
       cDBusHelper::AddKeyValue(array, "IsPesRecording", DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &b);
       b = recording->IsNew();
       cDBusHelper::AddKeyValue(array, "IsNew", DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &b);
       b = recording->IsEdited();
       cDBusHelper::AddKeyValue(array, "IsEdited", DBUS_TYPE_BOOLEAN, DBUS_TYPE_BOOLEAN_AS_STRING, &b);
       const cRecordingInfo *info = recording->Info();
       if (info != NULL) {
          s = info->ChannelID().ToString();
          c = *s;
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ChannelID", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          c = info->ChannelName();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ChannelName", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          c = info->Title();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Title", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          c = info->ShortText();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ShortText", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          c = info->Description();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Description", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          c = info->Aux();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Aux", DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
          d = info->FramesPerSecond();
          cDBusHelper::AddKeyValue(array, "Info/FramesPerSecond", DBUS_TYPE_DOUBLE, DBUS_TYPE_DOUBLE_AS_STRING, &d);
          const cComponents *components = info->Components();
          if ((components != NULL) && (components->NumComponents() > 0)) {
             for (int comp = 0; comp < components->NumComponents(); comp++) {
                 tComponent *component = components->Component(comp);
                 if (component != NULL) {
                    s = cString::sprintf("Info/Component/%d/stream", comp);
                    i = component->stream;
                    cDBusHelper::AddKeyValue(array, *s, DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
                    s = cString::sprintf("Info/Component/%d/type", comp);
                    i = component->type;
                    cDBusHelper::AddKeyValue(array, *s, DBUS_TYPE_INT32, DBUS_TYPE_INT32_AS_STRING, &i);
                    if (component->language[0] != 0) {
                       s = cString::sprintf("Info/Component/%d/language", comp);
                       c = component->language;
                       cDBusHelper::AddKeyValue(array, *s, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &c);
                       }
                    if (component->description != NULL) {
                       s = cString::sprintf("Info/Component/%d/description", comp);
                       cDBusHelper::AddKeyValue(array, *s, DBUS_TYPE_STRING, DBUS_TYPE_STRING_AS_STRING, &component->description);
                       }
                    }
                 }
             }
          }

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
       DBusMessageIter sub;
       DBusMessageIter *tmp = &args;
       if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
          dbus_message_iter_recurse(&args, &sub);
          tmp = &sub;
          }
       if (dbus_message_iter_get_arg_type(tmp) == DBUS_TYPE_STRING) {
          const char *path = NULL;
          if ((cDBusHelper::GetNextArg(*tmp, DBUS_TYPE_STRING, &path) >= 0) && (path != NULL) && *path)
             recording = recordings.GetByName(path);
          }
       else if (dbus_message_iter_get_arg_type(tmp) == DBUS_TYPE_INT32) {
          int number = 0;
          if ((cDBusHelper::GetNextArg(*tmp, DBUS_TYPE_INT32, &number) >= 0) && (number > 0) && (number <= recordings.Count()))
             recording = recordings.Get(number - 1);
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
:cDBusMessageDispatcher(DBUS_VDR_RECORDING_INTERFACE)
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
