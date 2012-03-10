#include "osd.h"

#include "common.h"
#include "helper.h"
#include "monitor.h"

#include <sys/time.h>

#include <png++/image.hpp>
#include <png++/rgba_pixel.hpp>

#include <vdr/device.h>
#include <vdr/videodir.h>

#define DBUSOSDDIR "/tmp/dbus2vdr"
//#define DBUSOSDDIR VideoDirectory


int   cDBusOsd::osd_number = 0;

cDBusOsd::cDBusOsd(cDBusOsdProvider& Provider, int Left, int Top, uint Level)
 :cOsd(Left, Top, Level)
 ,provider(Provider)
 ,osd_index(osd_number++)
 ,counter(0)
{
  osd_dir = cString::sprintf("%s/dbusosd-%04x", DBUSOSDDIR, osd_index);
  if (!MakeDirs(*osd_dir, true))
     esyslog("dbus2vdr: can't create %s", *osd_dir);
  provider.SendMessage(new cDbusOsdMsg("Open", osd_dir, Left, Top, 0, 0));
}

cDBusOsd::~cDBusOsd()
{
  provider.SendMessage(new cDbusOsdMsg("Close", osd_dir, 0, 0, 0, 0));
}

void cDBusOsd::Flush(void)
{
  if (!cOsd::Active())
      return;
/*
  struct timeval start;
  struct timeval end;
  struct timezone timeZone;
  gettimeofday(&start, &timeZone);
  bool write = false;
*/

  if (IsTrueColor()) {
    LOCK_PIXMAPS;
    int left = Left();
    int top = Top();
    const cRect* vp;
    int vx, vy, vw, vh;
    int x, y;
    const uint8_t *pixel;
    png::image<png::rgba_pixel> *pngfile;
    while (cPixmapMemory *pm = RenderPixmaps()) {
          //write = true;
          vp = &pm->ViewPort();
          vx = vp->X();
          vy = vp->Y();
          vw = vp->Width();
          vh = vp->Height();
          pixel = pm->Data();
          pngfile = new png::image<png::rgba_pixel>(vw, vh);
          for (y = 0; y < vh ; y++) {
              for (x = 0; x < vw; x++) {
                  (*pngfile)[y][x] = png::rgba_pixel(pixel[2], pixel[1], pixel[0], pixel[3]);
                  pixel += 4;
                  }
              }
          cString filename = cString::sprintf("%s/%04x-%d-%d-%d-%d.png", *osd_dir, counter, left, top, vx, vy);
          pngfile->write(*filename);
          provider.SendMessage(new cDbusOsdMsg("Display", filename, left, top, vx, vy));

          counter++;
          delete pngfile;
          delete pm;
          }
  }
/*
  if (write) {
     gettimeofday(&end, &timeZone);
     int timeNeeded = end.tv_usec - start.tv_usec;
     timeNeeded += (end.tv_sec - start.tv_sec) * 1000000;
     isyslog("dbus2vdr: flushing osd %d needed %d\n", osd_index, timeNeeded);
     }
*/
}


cDBusOsdProvider *_provider = NULL;

cDBusOsdProvider::cDBusOsdProvider(void)
{
  _provider = this;
  isyslog("dbus2vdr: new DBus-OSD-provider");
  SetDescription("dbus2vdr: osd-provider signal");
  Start();
}

cDBusOsdProvider::~cDBusOsdProvider()
{
  _provider = NULL;
  isyslog("dbus2vdr: delete DBus-OSD-provider");
  msgCond.Broadcast();
  Cancel(10);
}

cOsd *cDBusOsdProvider::CreateOsd(int Left, int Top, uint Level)
{
  return new cDBusOsd(*this, Left, Top, Level);
}

void cDBusOsdProvider::Action(void)
{
  while (Running() || (msgQueue.Count() > 0)) {
        cDbusOsdMsg *dbmsg = NULL;
        { // for short lock
          cMutexLock MutexLock(&msgMutex);
          dbmsg = msgQueue.First();
          if (dbmsg != NULL)
             msgQueue.Del(dbmsg, false);
        }
        if (dbmsg != NULL) {
           DBusMessage *msg = dbus_message_new_signal("/OSD", DBUS_VDR_OSD_INTERFACE, dbmsg->action);
           if (msg != NULL) {
              DBusMessageIter args;
              dbus_message_iter_init_append(msg, &args);

              bool msgOk= true;
              bool display= (strcmp(dbmsg->action, "Display") == 0);

              // osdid or filename for every signal
              const char *file = *dbmsg->file;
              if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &file))
                 msgOk = false;
              else if (display || (strcmp(dbmsg->action, "Open") == 0)) { // top and left only for Open and Display
                 if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &dbmsg->left))
                    msgOk = false;
                 else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &dbmsg->top))
                    msgOk = false;
                 else if (display) { // vx and vy only for Display
                    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &dbmsg->vx))
                       msgOk = false;
                    else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &dbmsg->vy))
                       msgOk = false;
                    }
                 }
              if (msgOk && cDBusMonitor::SendSignal(msg))
                 msg = NULL;
              else
                 dbus_message_unref(msg);
              }
           delete dbmsg;
           }
        cMutexLock MutexLock(&msgMutex);
        if (msgQueue.Count() == 0)
           msgCond.TimedWait(msgMutex, 1000);
        }
}

void cDBusOsdProvider::SendMessage(cDbusOsdMsg *Msg)
{
  cMutexLock MutexLock(&msgMutex);
  msgQueue.Add(Msg);
  msgCond.Broadcast();
}


cDbusOsdMsg::~cDbusOsdMsg(void)
{
  if (strcmp(action, "Close") == 0) {
     isyslog("dbus2vdr: deleting osd files at %s", *file);
     RemoveFileOrDir(*file, false);
     }
}


cDBusMessageOsd::cDBusMessageOsd(cDBusMessageOsd::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageOsd::~cDBusMessageOsd(void)
{
}

void cDBusMessageOsd::Process(void)
{
  switch (_action) {
    case dmoCreateProvider:
      CreateProvider();
      break;
    case dmoDeleteProvider:
      DeleteProvider();
      break;
    }
}

void cDBusMessageOsd::CreateProvider(void)
{
  if (cDBusOsdProvider::_provider == NULL) {
     new cDBusOsdProvider;
     cDBusHelper::SendReply(_conn, _msg, 250, "DBus-OSD-provider created");
     return;
     }
  cDBusHelper::SendReply(_conn, _msg, 900, "DBus-OSD-provider already active");
}

void cDBusMessageOsd::DeleteProvider(void)
{
  if (cDBusOsdProvider::_provider != NULL) {
     delete cDBusOsdProvider::_provider;
     // try to re-create the OSD provider of the primary device
     // I don't know if it's "good" or work with all output plugins
     cDevice *pd = cDevice::PrimaryDevice();
     if (pd)
        cDevice::SetPrimaryDevice(pd->CardIndex() + 1);
     cDBusHelper::SendReply(_conn, _msg, 250, "DBus-OSD-provider deleted");
     return;
     }
  cDBusHelper::SendReply(_conn, _msg, 900, "DBus-OSD-provider not active");
}


cDBusDispatcherOsd::cDBusDispatcherOsd(void)
:cDBusMessageDispatcher(DBUS_VDR_OSD_INTERFACE)
{
}

cDBusDispatcherOsd::~cDBusDispatcherOsd(void)
{
}

cDBusMessage *cDBusDispatcherOsd::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/OSD") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_OSD_INTERFACE, "CreateProvider"))
     return new cDBusMessageOsd(cDBusMessageOsd::dmoCreateProvider, conn, msg);

  if (dbus_message_is_method_call(msg, DBUS_VDR_OSD_INTERFACE, "DeleteProvider"))
     return new cDBusMessageOsd(cDBusMessageOsd::dmoDeleteProvider, conn, msg);

  return NULL;
}

bool          cDBusDispatcherOsd::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/OSD") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_OSD_INTERFACE"\">\n"
  "    <method name=\"CreateProvider\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"DeleteProvider\">\n"
  "      <arg name=\"replycode\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"replymessage\" type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <signal name=\"Open\">\n"
  "      <arg name=\"osdid\"  type=\"s\"/>\n"
  "      <arg name=\"left\"   type=\"i\"/>\n"
  "      <arg name=\"top\"    type=\"i\"/>\n"
  "    </signal>\n"
  "    <signal name=\"Display\">\n"
  "      <arg name=\"filename\"  type=\"s\"/>\n"
  "      <arg name=\"left\"      type=\"i\"/>\n"
  "      <arg name=\"top\"       type=\"i\"/>\n"
  "      <arg name=\"vx\"        type=\"i\"/>\n"
  "      <arg name=\"vy\"        type=\"i\"/>\n"
  "    </signal>\n"
  "    <signal name=\"Close\">\n"
  "      <arg name=\"osdid\"  type=\"s\"/>\n"
  "    </signal>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
