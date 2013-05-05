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
    "  </interface>\n"
    "</node>\n";

#define EMPTY(s) (s == NULL ? "" : s)

  class cVdrStatus : public cStatus
  {
  private:
    cDBusStatus *_status;

    void EmitSignal(const char *Signal, GVariant *Parameters)
    {
      _status->Connection()->EmitSignal( new cDBusConnection::cDBusSignal(NULL, "/Status", DBUS_VDR_STATUS_INTERFACE, Signal, Parameters));
    };

  public:
    cVdrStatus(cDBusStatus *Status)
    {
      _status = Status;
    };

    virtual ~cVdrStatus(void)
    {
    };

    virtual void TimerChange(const cTimer *Timer, eTimerChange Change)
    {
    };

    virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView)
    {
      gint32 deviceNumber = -1;
      if (Device != NULL)
         deviceNumber = Device->DeviceNumber();
      gboolean liveView = (LiveView ? TRUE : FALSE);
      EmitSignal("ChannelSwitch", g_variant_new("(iib)", deviceNumber, ChannelNumber, liveView));
    };

    virtual void Recording(const cDevice *Device, const char *Name, const char *FileName, bool On)
    {
      gint32 deviceNumber = -1;
      if (Device != NULL)
         deviceNumber = Device->DeviceNumber();
      gboolean on = (On ? TRUE : FALSE);
      EmitSignal("Recording", g_variant_new("(issb)", deviceNumber, EMPTY(Name), EMPTY(FileName), on));
    };

    virtual void Replaying(const cControl *Control, const char *Name, const char *FileName, bool On)
    {
      gboolean on = (On ? TRUE : FALSE);
      EmitSignal("Replaying", g_variant_new("(ssb)", EMPTY(Name), EMPTY(FileName), on));
    };

    virtual void SetVolume(int Volume, bool Absolute)
    {
    };

    virtual void SetAudioTrack(int Index, const char * const *Tracks)
    {
    };

    virtual void SetAudioChannel(int AudioChannel)
    {
    };

    virtual void SetSubtitleTrack(int Index, const char * const *Tracks)
    {
    };

    virtual void OsdClear(void)
    {
    };

    virtual void OsdTitle(const char *Title)
    {
    };

    virtual void OsdStatusMessage(const char *Message)
    {
    };

    virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
    {
    };

    virtual void OsdItem(const char *Text, int Index)
    {
    };

    virtual void OsdCurrentItem(const char *Text)
    {
    };

    virtual void OsdTextItem(const char *Text, bool Scroll)
    {
    };

    virtual void OsdChannel(const char *Text)
    {
    };

    virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
    {
    };
  };

#undef EMTPY
}


cDBusStatus::cDBusStatus(void)
:cDBusObject("/Status", cDBusStatusHelper::_xmlNodeInfo)
{
  _status = new cDBusStatusHelper::cVdrStatus(this);
}

cDBusStatus::~cDBusStatus(void)
{
  delete _status;
}
