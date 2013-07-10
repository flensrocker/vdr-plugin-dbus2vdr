#include "status.h"
#include "common.h"
#include "connection.h"
#include "helper.h"

#include <vdr/status.h>


namespace cDBusStatusHelper
{
  static const char *_xmlNodeInfo = 
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\""DBUS_VDR_STATUS_INTERFACE"\">\n"
    "    <signal name=\"TimerChange\">\n"
    "      <arg name=\"Timer\"           type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"Change\"          type=\"s\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"ChannelSwitch\">\n"
    "      <arg name=\"DeviceNumber\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"ChannelNumber\"   type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"LiveView\"        type=\"b\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"Recording\">\n"
    "      <arg name=\"DeviceNumber\"    type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"Name\"            type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"FileName\"        type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"On\"              type=\"b\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"Replaying\">\n"
    "      <arg name=\"Name\"            type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"FileName\"        type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"On\"              type=\"b\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <method name=\"IsReplaying\">\n"
    "      <arg name=\"Name\"            type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"FileName\"        type=\"s\" direction=\"out\"/>\n"
    "      <arg name=\"On\"              type=\"b\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <signal name=\"SetVolume\">\n"
    "      <arg name=\"Volume\"          type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"Absolute\"        type=\"b\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"SetAudioTrack\">\n"
    "      <arg name=\"Index\"           type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"Tracks\"          type=\"as\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"SetAudioChannel\">\n"
    "      <arg name=\"AudioChannel\"    type=\"i\" direction=\"out\"/>\n"
    "    </signal>\n"
    "    <signal name=\"SetSubtitleTrack\">\n"
    "      <arg name=\"Index\"           type=\"i\" direction=\"out\"/>\n"
    "      <arg name=\"Tracks\"          type=\"as\" direction=\"out\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "</node>\n";

#define EMPTY(s) (s == NULL ? "" : s)

  class cVdrStatus : public cStatus
  {
  private:
    cDBusStatus *_status;
    bool         _network;

    GMutex     _replay_mutex;
    gboolean   _replay_on;
    gchar     *_replay_name;
    gchar     *_replay_filename;

    void EmitSignal(const char *Signal, GVariant *Parameters)
    {
      _status->Connection()->EmitSignal( new cDBusSignal(NULL, "/Status", DBUS_VDR_STATUS_INTERFACE, Signal, Parameters, NULL, NULL));
    };

    void SetReplay(const char *Name, const char *Filename)
    {
      if (_replay_name != NULL) {
         g_free(_replay_name);
         _replay_name = NULL;
         }
      _replay_name = g_strdup(Name);

      if (_replay_filename != NULL) {
         g_free(_replay_filename);
         _replay_filename = NULL;
         }
      _replay_filename = g_strdup(Filename);
    };

  public:
    cVdrStatus(cDBusStatus *Status, bool Network)
    {
      _status = Status;
      _network = Network;

      g_mutex_init(&_replay_mutex);
      _replay_on = FALSE;
      _replay_name = NULL;
      _replay_filename = NULL;
    };

    virtual ~cVdrStatus(void)
    {
      g_mutex_lock(&_replay_mutex);
      SetReplay(NULL, NULL);
      g_mutex_unlock(&_replay_mutex);
      g_mutex_clear(&_replay_mutex);
    };

    virtual void TimerChange(const cTimer *Timer, eTimerChange Change)
    {
      const char *timer = NULL;
      const char *change = NULL;

      cString text;
      if (Timer != NULL) {
         text = Timer->ToText(true);
         timer = stripspace((char*)*text);
         }

      switch (Change) {
       case tcMod:
        change = "tcMod";
        break;
       case tcAdd:
        change = "tcAdd";
        break;
       case tcDel:
        change = "tcDel";
        break;
       }

      EmitSignal("TimerChange", g_variant_new("(ss)", EMPTY(timer), EMPTY(change)));
    };

    virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView)
    {
      if (_network)
         return;

      gint32 deviceNumber = -1;
      if (Device != NULL)
         deviceNumber = Device->DeviceNumber() + 1;
      gboolean liveView = (LiveView ? TRUE : FALSE);
      EmitSignal("ChannelSwitch", g_variant_new("(iib)", deviceNumber, ChannelNumber, liveView));
    };

    virtual void Recording(const cDevice *Device, const char *Name, const char *FileName, bool On)
    {
      if (_network)
         return;

      gint32 deviceNumber = -1;
      if (Device != NULL)
         deviceNumber = Device->DeviceNumber() + 1;
      gboolean on = (On ? TRUE : FALSE);
      EmitSignal("Recording", g_variant_new("(issb)", deviceNumber, EMPTY(Name), EMPTY(FileName), on));
    };

    virtual void Replaying(const cControl *Control, const char *Name, const char *FileName, bool On)
    {
      if (_network)
         return;

      g_mutex_lock(&_replay_mutex);
      _replay_on = (On ? TRUE : FALSE);
      if (On)
         SetReplay(Name, FileName);
      else
         SetReplay(NULL, NULL);
      EmitSignal("Replaying", g_variant_new("(ssb)", EMPTY(Name), EMPTY(FileName), _replay_on));
      g_mutex_unlock(&_replay_mutex);
    };

    static void IsReplaying(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation)
    {
      cDBusStatus *status = dynamic_cast<cDBusStatus*>(Object);
      if ((status == NULL) || (status->_status == NULL)) {
         g_dbus_method_invocation_return_error(Invocation, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED, "can't get status object");
         return;
         }

      g_mutex_lock(&status->_status->_replay_mutex);
      GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(ssb)"));
      g_variant_builder_add(builder, "s", EMPTY(status->_status->_replay_name));
      g_variant_builder_add(builder, "s", EMPTY(status->_status->_replay_filename));
      g_variant_builder_add(builder, "b", status->_status->_replay_on);
      g_dbus_method_invocation_return_value(Invocation, g_variant_builder_end(builder));
      g_variant_builder_unref(builder);
      g_mutex_unlock(&status->_status->_replay_mutex);
    };

    virtual void SetVolume(int Volume, bool Absolute)
    {
      if (_network)
         return;

      EmitSignal("SetVolume", g_variant_new("(ib)", Volume, Absolute));
    };

    virtual void SetAudioTrack(int Index, const char * const *Tracks)
    {
      if (_network)
         return;

      GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(ias)"));
      g_variant_builder_add(builder, "i", Index);
      GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("as"));
      for (int i = 0; Tracks[i] != NULL; i++)
            g_variant_builder_add(array, "s", Tracks[i]);
      g_variant_builder_add_value(builder, g_variant_builder_end(array));
      EmitSignal("SetAudioTrack", g_variant_builder_end(builder));
      g_variant_builder_unref(array);
      g_variant_builder_unref(builder);
    };

    virtual void SetAudioChannel(int AudioChannel)
    {
      if (_network)
         return;

      EmitSignal("SetAudioChannel", g_variant_new("(i)", AudioChannel));
    };

    virtual void SetSubtitleTrack(int Index, const char * const *Tracks)
    {
      if (_network)
         return;

      GVariantBuilder *builder = g_variant_builder_new(G_VARIANT_TYPE("(ias)"));
      g_variant_builder_add(builder, "i", Index);
      GVariantBuilder *array = g_variant_builder_new(G_VARIANT_TYPE("as"));
      for (int i = 0; Tracks[i] != NULL; i++)
            g_variant_builder_add(array, "s", Tracks[i]);
      g_variant_builder_add_value(builder, g_variant_builder_end(array));
      EmitSignal("SetSubtitleTrack", g_variant_builder_end(builder));
      g_variant_builder_unref(array);
      g_variant_builder_unref(builder);
    };

    virtual void OsdClear(void)
    {
      if (_network)
         return;
    };

    virtual void OsdTitle(const char *Title)
    {
      if (_network)
         return;
    };

    virtual void OsdStatusMessage(const char *Message)
    {
      if (_network)
         return;
    };

    virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
    {
      if (_network)
         return;
    };

    virtual void OsdItem(const char *Text, int Index)
    {
      if (_network)
         return;
    };

    virtual void OsdCurrentItem(const char *Text)
    {
      if (_network)
         return;
    };

    virtual void OsdTextItem(const char *Text, bool Scroll)
    {
      if (_network)
         return;
    };

    virtual void OsdChannel(const char *Text)
    {
      if (_network)
         return;
    };

    virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
    {
      if (_network)
         return;
    };
  };

#undef EMTPY
}


cDBusStatus::cDBusStatus(bool Network)
:cDBusObject("/Status", cDBusStatusHelper::_xmlNodeInfo)
{
  _status = new cDBusStatusHelper::cVdrStatus(this, Network);
  if (!Network)
     AddMethod("IsReplaying", cDBusStatusHelper::cVdrStatus::IsReplaying);
}

cDBusStatus::~cDBusStatus(void)
{
  delete _status;
}
