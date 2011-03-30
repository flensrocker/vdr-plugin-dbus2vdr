#include "timer.h"
#include "common.h"
#include "helper.h"

#include <vdr/timers.h>


cDBusMessageTimer::cDBusMessageTimer(cDBusMessageTimer::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageTimer::~cDBusMessageTimer(void)
{
}

void cDBusMessageTimer::Process(void)
{
  switch (_action) {
    case dmtNext:
      Next();
      break;
    }
}

void cDBusMessageTimer::Next(void)
{
  int returncode = 250;
  int number = -1;
  int seconds = 0;
  time_t start = 0;
  time_t stop = 0;
  const char *title = "";
  cTimer *t = Timers.GetNextActiveTimer();
  const cEvent *e;
  if (t) {
     start = t->StartTime();
     number = t->Index() + 1;
     seconds = start - time(NULL);
     stop = t->StopTime();
     e = t->Event();
     if ((e != NULL) && (e->Title() != NULL))
        title = e->Title();
     }
  else
     returncode = 550;

  DBusMessage *reply = dbus_message_new_method_return(_msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &returncode))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the return-code", DBUS_VDR_TIMER_INTERFACE);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &number))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the number", DBUS_VDR_TIMER_INTERFACE);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &seconds))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the seconds", DBUS_VDR_TIMER_INTERFACE);
  dbus_uint64_t tmp = start;
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT64, &tmp))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the start time", DBUS_VDR_TIMER_INTERFACE);
  tmp = stop;
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT64, &tmp))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the stop time", DBUS_VDR_TIMER_INTERFACE);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title))
     esyslog("dbus2vdr: %s.Next: out of memory while appending the title", DBUS_VDR_TIMER_INTERFACE);

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(_conn, reply, &serial))
     esyslog("dbus2vdr: %s.Next: out of memory while sending the reply", DBUS_VDR_TIMER_INTERFACE);
  dbus_message_unref(reply);
}


cDBusDispatcherTimer::cDBusDispatcherTimer(void)
:cDBusMessageDispatcher(DBUS_VDR_TIMER_INTERFACE)
{
}

cDBusDispatcherTimer::~cDBusDispatcherTimer(void)
{
}

cDBusMessage *cDBusDispatcherTimer::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/Timers") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_TIMER_INTERFACE, "Next"))
     return new cDBusMessageTimer(cDBusMessageTimer::dmtNext, conn, msg);

  return NULL;
}

bool          cDBusDispatcherTimer::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Timers") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_TIMER_INTERFACE"\">\n"
  "    <method name=\"Next\">\n"
  "      <arg name=\"replycode\" type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"number\"    type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"seconds\"   type=\"i\" direction=\"out\"/>\n"
  "      <arg name=\"start\"     type=\"t\" direction=\"out\"/>\n"
  "      <arg name=\"stop\"      type=\"t\" direction=\"out\"/>\n"
  "      <arg name=\"title\"     type=\"s\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "</node>\n";
  return true;
}
