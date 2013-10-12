#include "recording.h"
#include "common.h"
#include "helper.h"

#include <vdr/menu.h>
#include <vdr/recording.h>
#include <vdr/videodir.h>


class cDBusRecordingsHelper
{
private:
  // own recordings list like in SVDRP
  // so Play has the right numbers
  static cRecordings recordings;

public:
  static const char *_xmlNodeInfoConst;
  static const char *_xmlNodeInfo;

  static GVariant *BuildRecording(const cRecording *recording)
  {
    GVariantBuilder *struc = g_variant_builder_new(G_VARIANT_TYPE("(ia(sv))"));
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(sv)"));
    if (recording == NULL)
       g_variant_builder_add(struc, "i", -1);
    else {
       g_variant_builder_add(struc, "i", recording->Index() + 1);
       cString s;
       const char *c;
       gboolean b;
       guint64 tu64;
       int i;
       gdouble d;

       c = recording->FileName();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Path", "s", (void**)&c);
       c = recording->Name();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Name", "s", (void**)&c);
       c = recording->Title();
       if (c != NULL)
          cDBusHelper::AddKeyValue(array, "Title", "s", (void**)&c);
       tu64 = recording->Start();
       if (tu64 > 0)
          cDBusHelper::AddKeyValue(array, "Start", "t", (void**)&tu64);
       tu64 = recording->Deleted();
       if (tu64 > 0)
          cDBusHelper::AddKeyValue(array, "Deleted", "t", (void**)&tu64);
       i = recording->Priority();
       cDBusHelper::AddKeyValue(array, "Priority", "i", (void**)&i);
       i = recording->Lifetime();
       cDBusHelper::AddKeyValue(array, "Lifetime", "i", (void**)&i);
       i = recording->HierarchyLevels();
       cDBusHelper::AddKeyValue(array, "HierarchyLevels", "i", (void**)&i);
       d = recording->FramesPerSecond();
       cDBusHelper::AddKeyValue(array, "FramesPerSecond", "d", (void**)&d);
       i = recording->NumFrames();
       cDBusHelper::AddKeyValue(array, "NumFrames", "i", (void**)&i);
       i = recording->LengthInSeconds();
       cDBusHelper::AddKeyValue(array, "LengthInSeconds", "i", (void**)&i);
       i = recording->FileSizeMB();
       cDBusHelper::AddKeyValue(array, "FileSizeMB", "i", (void**)&i);
       b = recording->IsPesRecording() ? TRUE : FALSE;
       cDBusHelper::AddKeyValue(array, "IsPesRecording", "b", (void**)&b);
       b = recording->IsNew() ? TRUE : FALSE;
       cDBusHelper::AddKeyValue(array, "IsNew", "b", (void**)&b);
       b = recording->IsEdited() ? TRUE : FALSE;
       cDBusHelper::AddKeyValue(array, "IsEdited", "b", (void**)&b);
       const cRecordingInfo *info = recording->Info();
       if (info != NULL) {
          s = info->ChannelID().ToString();
          c = *s;
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ChannelID", "s", (void**)&c);
          c = info->ChannelName();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ChannelName", "s", (void**)&c);
          c = info->Title();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Title", "s", (void**)&c);
          c = info->ShortText();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/ShortText", "s", (void**)&c);
          c = info->Description();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Description", "s", (void**)&c);
          c = info->Aux();
          if (c != NULL)
             cDBusHelper::AddKeyValue(array, "Info/Aux", "s", (void**)&c);
          d = info->FramesPerSecond();
          cDBusHelper::AddKeyValue(array, "Info/FramesPerSecond", "d", (void**)&d);
          const cComponents *components = info->Components();
          if ((components != NULL) && (components->NumComponents() > 0)) {
             for (int comp = 0; comp < components->NumComponents(); comp++) {
                 tComponent *component = components->Component(comp);
                 if (component != NULL) {
                    s = cString::sprintf("Info/Component/%d/stream", comp);
                    i = component->stream;
                    cDBusHelper::AddKeyValue(array, *s, "i", (void**)&i);
                    s = cString::sprintf("Info/Component/%d/type", comp);
                    i = component->type;
                    cDBusHelper::AddKeyValue(array, *s, "i", (void**)&i);
                    if (component->language[0] != 0) {
                       s = cString::sprintf("Info/Component/%d/language", comp);
                       c = component->language;
                       cDBusHelper::AddKeyValue(array, *s, "s", (void**)&c);
                       }
                    if (component->description != NULL) {
                       s = cString::sprintf("Info/Component/%d/description", comp);
                       cDBusHelper::AddKeyValue(array, *s, "s", (void**)&component->description);
                       }
                    }
                 }
             }
          }
       }

    g_variant_builder_add_value(struc, g_variant_builder_end(array));
    GVariant *ret = g_variant_builder_end(struc);
    g_variant_builder_unref(array);
    g_variant_builder_unref(struc);
    return ret;
  };

  static void Get(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    // only update recordings list if empty
    // so we don't mess around with the index values returned by List
    if (recordings.Count() == 0)
       recordings.Update(true);

    cRecording *recording = NULL;
    GVariant *first = g_variant_get_child_value(Parameters, 0);
    GVariant *refValue = first;
    if (g_variant_is_of_type(first, G_VARIANT_TYPE_VARIANT))
       refValue = g_variant_get_child_value(first, 0);
    if (g_variant_is_of_type(refValue, G_VARIANT_TYPE_STRING)) {
       const char *path = NULL;
       g_variant_get(refValue, "&s", &path);
       if ((path != NULL) && *path)
          recording = recordings.GetByName(path);
       }
    else if (g_variant_is_of_type(refValue, G_VARIANT_TYPE_INT32)) {
       int number = 0;
       g_variant_get(refValue, "i", &number);
       if ((number > 0) && (number <= recordings.Count()))
          recording = recordings.Get(number - 1);
       }
    if (refValue != first)
       g_variant_unref(refValue);
    g_variant_unref(first);

    GVariant *rec = BuildRecording(recording);
    g_dbus_method_invocation_return_value(Invocation, g_variant_new_tuple(&rec, 1));
  };

  static void List(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    recordings.Update(true);

    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("a(ia(sv))"));
    for (cRecording *r = recordings.First(); r; r = recordings.Next(r))
        g_variant_builder_add_value(array, BuildRecording(r));
    GVariant *a = g_variant_builder_end(array);
    g_dbus_method_invocation_return_value(Invocation, g_variant_new_tuple(&a, 1));
    g_variant_builder_unref(array);
  };

  static void Update(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    Recordings.Update(false);
    cDBusHelper::SendReply(Invocation, 250, "update of recordings triggered");
  };

  static void Play(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int replyCode = 501;
    cString replyMessage;
    cRecording *recording = NULL;
    int position = -1; // default: resume
    const char *hmsf = NULL;

    GVariant *first = g_variant_get_child_value(Parameters, 0);
    GVariant *refFirst = first;
    if (g_variant_is_of_type(first, G_VARIANT_TYPE_VARIANT))
       refFirst = g_variant_get_child_value(first, 0);
    GVariant *second = g_variant_get_child_value(Parameters, 1);
    GVariant *refSecond = second;
    if (g_variant_is_of_type(second, G_VARIANT_TYPE_VARIANT))
       refSecond = g_variant_get_child_value(second, 0);

    if (g_variant_is_of_type(refFirst, G_VARIANT_TYPE_STRING)) {
       const char *path = NULL;
       g_variant_get(refFirst, "&s", &path);
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
    else if (g_variant_is_of_type(refFirst, G_VARIANT_TYPE_INT32)) {
       int number = 0;
       g_variant_get(refFirst, "i", &number);
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
       if (g_variant_is_of_type(refSecond, G_VARIANT_TYPE_STRING)) {
          g_variant_get(refSecond, "&s", &hmsf);
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
       else if (g_variant_is_of_type(refSecond, G_VARIANT_TYPE_INT32))
          g_variant_get(refSecond, "i", &position);

       replyCode = 250;
#if VDRVERSNUM < 10728
       cReplayControl::SetRecording(NULL, NULL);
#else
       cReplayControl::SetRecording(NULL);
#endif
       cControl::Shutdown();
       if (position >= 0) {
          cResumeFile resume(recording->FileName(), recording->IsPesRecording());
          if (position == 0)
             resume.Delete();
          else
             resume.Save(position);
          }
#if VDRVERSNUM < 10728
       cReplayControl::SetRecording(recording->FileName(), recording->Title());
#else
       cReplayControl::SetRecording(recording->FileName());
#endif
       cControl::Launch(new cReplayControl);
       cControl::Attach();

       if (hmsf != NULL)
          replyMessage = cString::sprintf("Playing recording \"%s\" from position %s", recording->FileName(), hmsf);
       else if (position >= 0)
          replyMessage = cString::sprintf("Playing recording \"%s\" from position %d", recording->FileName(), position);
       else
          replyMessage = cString::sprintf("Resume playing recording \"%s\"", recording->FileName());
       }
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
    if (refFirst != first)
       g_variant_unref(refFirst);
    g_variant_unref(first);
    if (refSecond != second)
       g_variant_unref(refSecond);
    g_variant_unref(second);
  };

  static void ListExtraVideoDirectories(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int replyCode = 500;
    cString replyMessage = "Missing extra-video-directories patch";
    cStringList dirs;
#ifdef EXTRA_VIDEO_DIRECTORIES_PATCH
    if (!LockExtraVideoDirectories(false)) {
       replyCode = 550;
       replyMessage = "Unable to lock extra video directory list";
       }
    else {
       if (ExtraVideoDirectories.Size() == 0) {
          replyCode = 550;
          replyMessage = "no extra video directories in list";
          }
       else {
          replyCode = 250;
          replyMessage = "";
          for (int i = 0; i < ExtraVideoDirectories.Size(); i++)
              dirs.Append(strdup(ExtraVideoDirectories.At(i)));
          }
       UnlockExtraVideoDirectories();
       }
#endif
    GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(isas)"));
    g_variant_builder_add(builder, "i", replyCode);
    g_variant_builder_add(builder, "s", *replyMessage);
    GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("as"));
    for (int i = 0; i < dirs.Size(); i++)
        g_variant_builder_add(array, "s", dirs.At(i));
    g_variant_builder_add_value(builder, g_variant_builder_end(array));
    g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
    g_variant_builder_unref(array);
    g_variant_builder_unref(builder);
  };

  static void AddExtraVideoDirectory(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int replyCode = 500;
    cString replyMessage = "Missing extra-video-directories patch";
#ifdef EXTRA_VIDEO_DIRECTORIES_PATCH
    const char *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);

    if (*option) {
       if (!LockExtraVideoDirectories(false)) {
          replyCode = 550;
          replyMessage = "Unable to lock extra video directory list";
          cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
          return;
          }
       ::AddExtraVideoDirectory(option);
       UnlockExtraVideoDirectories();
       replyCode = 250;
       replyMessage = cString::sprintf("added '%s' to extra video directory list", option);
       cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
       return;
       }
    replyCode = 501;
    replyMessage = "Missing directory name";
#endif
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void ClearExtraVideoDirectories(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int replyCode = 500;
    cString replyMessage = "Missing extra-video-directories patch";
#ifdef EXTRA_VIDEO_DIRECTORIES_PATCH
    if (!LockExtraVideoDirectories(false)) {
       replyCode = 550;
       replyMessage = "Unable to lock extra video directory list";
       cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
       return;
       }
    ExtraVideoDirectories.Clear();
    UnlockExtraVideoDirectories();
    replyCode = 250;
    replyMessage = "cleared extra video directory list";
#endif
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };

  static void DeleteExtraVideoDirectory(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
  {
    int replyCode = 500;
    cString replyMessage = "Missing extra-video-directories patch";
#ifdef EXTRA_VIDEO_DIRECTORIES_PATCH
    const char *option = NULL;
    g_variant_get(Parameters, "(&s)", &option);

    if (*option) {
       if (!LockExtraVideoDirectories(false)) {
          replyCode = 550;
          replyMessage = "Unable to lock extra video directory list";
          cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
          return;
          }
       DelExtraVideoDirectory(option);
       UnlockExtraVideoDirectories();
       replyCode = 250;
       replyMessage = cString::sprintf("removed '%s' from extra video directory list", option);
       cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
       return;
       }
    replyCode= 501;
    replyMessage = "Missing directory name";
#endif
    cDBusHelper::SendReply(Invocation, replyCode, *replyMessage);
  };
};

cRecordings cDBusRecordingsHelper::recordings;

const char *cDBusRecordingsHelper::_xmlNodeInfoConst = 
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_RECORDING_INTERFACE"\">\n"
  "    <method name=\"Get\">\n"
  "      <arg name=\"number_or_path\" type=\"v\" direction=\"in\"/>\n"
  "      <arg name=\"recording\"      type=\"(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"List\">\n"
  "      <arg name=\"recordings\"   type=\"a(ia(sv))\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"ListExtraVideoDirectories\">\n"
  "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
  "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
  "      <arg name=\"extravideodirs\" type=\"as\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";

const char *cDBusRecordingsHelper::_xmlNodeInfo = 
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
  "    <method name=\"AddExtraVideoDirectory\">\n"
  "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"ClearExtraVideoDirectories\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"DeleteExtraVideoDirectory\">\n"
  "      <arg name=\"option\"       type=\"s\" direction=\"in\"/>\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"ListExtraVideoDirectories\">\n"
  "      <arg name=\"replycode\"      type=\"i\"  direction=\"out\"/>\n"
  "      <arg name=\"replymessage\"   type=\"s\"  direction=\"out\"/>\n"
  "      <arg name=\"extravideodirs\" type=\"as\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";


cDBusRecordingsConst::cDBusRecordingsConst(const char *NodeInfo)
:cDBusObject("/Recordings", NodeInfo)
{
  AddMethod("Get", cDBusRecordingsHelper::Get);
  AddMethod("List", cDBusRecordingsHelper::List);
  AddMethod("ListExtraVideoDirectories", cDBusRecordingsHelper::ListExtraVideoDirectories);
}

cDBusRecordingsConst::cDBusRecordingsConst(void)
:cDBusObject("/Recordings", cDBusRecordingsHelper::_xmlNodeInfoConst)
{
  AddMethod("Get", cDBusRecordingsHelper::Get);
  AddMethod("List", cDBusRecordingsHelper::List);
  AddMethod("ListExtraVideoDirectories", cDBusRecordingsHelper::ListExtraVideoDirectories);
}

cDBusRecordingsConst::~cDBusRecordingsConst(void)
{
}

cDBusRecordings::cDBusRecordings(void)
:cDBusRecordingsConst(cDBusRecordingsHelper::_xmlNodeInfo)
{
  AddMethod("Update", cDBusRecordingsHelper::Update);
  AddMethod("Play", cDBusRecordingsHelper::Play);
  AddMethod("AddExtraVideoDirectory", cDBusRecordingsHelper::AddExtraVideoDirectory);
  AddMethod("ClearExtraVideoDirectories", cDBusRecordingsHelper::ClearExtraVideoDirectories);
  AddMethod("DeleteExtraVideoDirectory", cDBusRecordingsHelper::DeleteExtraVideoDirectory);
}

cDBusRecordings::~cDBusRecordings(void)
{
}
