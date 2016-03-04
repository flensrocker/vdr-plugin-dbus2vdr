#ifndef __DBUS2VDR_NULLDEVICE_H
#define __DBUS2VDR_NULLDEVICE_H

#include <vdr/device.h>

class cNullDevice : public cDevice
{
public:
  cNullDevice() : cDevice() {}
  virtual ~cNullDevice() {}

  virtual bool Ready(void) { return true; }

  virtual cString DeviceType(void) const { return "null"; }
  virtual cString DeviceName(void) const { return "nulldevice"; }
  virtual bool HasDecoder(void) const { return true; }
  virtual bool AvoidRecording(void) const { return true; }

  virtual bool SetPlayMode(ePlayMode PlayMode) { return true; }
  virtual int  PlayVideo(const uchar *Data, int Length) { return Length; }

  virtual int  PlayAudio(const uchar *Data, int Length, uchar Id) { return Length; }

  virtual int PlayTsVideo(const uchar *Data, int Length) { return Length; }
  virtual int PlayTsAudio(const uchar *Data, int Length) { return Length; }
  virtual int PlayTsSubtitle(const uchar *Data, int Length) { return Length; }

  virtual int PlayPes(const uchar *Data, int Length, bool VideoOnly = false) { return Length; }
  virtual int PlayTs(const uchar *Data, int Length, bool VideoOnly = false) { return Length; }

  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0) { return true; }
  virtual bool Flush(int TimeoutMs = 0) { return true; }
  bool Start(void) {return true;}

protected:
  virtual void MakePrimaryDevice(bool On);
};

#endif
