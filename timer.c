#include "timer.h"
#include "common.h"
#include "helper.h"

#include <vdr/timers.h>


class cDBusTimerActions
{
public:
  static void List(DBusConnection* conn, DBusMessage* msg)
  {
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args;
    DBusMessageIter array;
    cString text;
    const char *tmp;
    dbus_message_iter_init_append(reply, &args);
    if (!dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array))
       esyslog("dbus2vdr: %s.List: can't open array container", DBUS_VDR_TIMER_INTERFACE);
    for (int i = 0; i < Timers.Count(); i++) {
        cTimer *timer = Timers.Get(i);
        if (timer) {
           text = timer->ToText(true);
           tmp = stripspace((char*)*text);
           if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &tmp))
              esyslog("dbus2vdr: %s.List: out of memory while appending the timer", DBUS_VDR_TIMER_INTERFACE);
           }
        }
    if (!dbus_message_iter_close_container(&args, &array))
       esyslog("dbus2vdr: %s.List: can't close array container", DBUS_VDR_TIMER_INTERFACE);
    cDBusHelper::SendReply(conn, reply);
  }

  static void Next(DBusConnection* conn, DBusMessage* msg)
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

    DBusMessage *reply = dbus_message_new_method_return(msg);
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
    if (!dbus_connection_send(conn, reply, &serial))
       esyslog("dbus2vdr: %s.Next: out of memory while sending the reply", DBUS_VDR_TIMER_INTERFACE);
    dbus_message_unref(reply);
  }
};


cDBusDispatcherTimerConst::cDBusDispatcherTimerConst(eBusType type)
:cDBusMessageDispatcher(type, DBUS_VDR_TIMER_INTERFACE)
{
  AddPath("/Timers");
  AddAction("List", cDBusTimerActions::List);
  AddAction("Next", cDBusTimerActions::Next);
}

cDBusDispatcherTimerConst::~cDBusDispatcherTimerConst(void)
{
}

bool          cDBusDispatcherTimerConst::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (strcmp(dbus_message_get_path(msg), "/Timers") != 0)
     return false;
  Data =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "       \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<node>\n"
  "  <interface name=\""DBUS_VDR_TIMER_INTERFACE"\">\n"
  "    <method name=\"List\">\n"
  "      <arg name=\"timer\"     type=\"as\" direction=\"out\"/>\n"
  "    </method>\n"
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


cDBusDispatcherTimer::cDBusDispatcherTimer(void)
:cDBusDispatcherTimerConst(busSystem)
{
}

cDBusDispatcherTimer::~cDBusDispatcherTimer(void)
{
}

bool          cDBusDispatcherTimer::OnIntrospect(DBusMessage *msg, cString &Data)
{
  if (!cDBusDispatcherTimerConst::OnIntrospect(msg, Data))
     return false;
  //TODO insert introspection data
  return true;
}
