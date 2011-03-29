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
  const char *option = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     isyslog("dbus2vdr: %s.Next: no option given", DBUS_VDR_TIMER_INTERFACE);
  else {
     if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &option) < 0)
        esyslog("dbus2vdr: %s.SVDRPCommand: option is not a string", DBUS_VDR_TIMER_INTERFACE);
     }

  cTimer *t = Timers.GetNextActiveTimer();
  if (t) {
     time_t start = t->StartTime();
     int number = t->Index() + 1;
     if (option == NULL)
        cDBusHelper::SendReply(_conn, _msg, 250, *cString::sprintf("%d %s", number, *TimeToString(start)));
     else if (strcasecmp(option, "ABS") == 0)
        cDBusHelper::SendReply(_conn, _msg, 250, *cString::sprintf("%d %ld", number, start));
     else if (strcasecmp(option, "REL") == 0)
        cDBusHelper::SendReply(_conn, _msg, 250, *cString::sprintf("%d %ld", number, start - time(NULL)));
     else if (strcasecmp(option, "VERBOSE") == 0) {
        DBusMessage *reply = dbus_message_new_method_return(_msg);
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);
        int returncode = 250;
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &returncode))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the return-code", DBUS_VDR_TIMER_INTERFACE);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &number))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the number", DBUS_VDR_TIMER_INTERFACE);
        int seconds = start - time(NULL);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &seconds))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the seconds", DBUS_VDR_TIMER_INTERFACE);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &start))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the start time", DBUS_VDR_TIMER_INTERFACE);
        time_t stop = t->StopTime();
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &stop))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the stop time", DBUS_VDR_TIMER_INTERFACE);
        const cEvent *e = t->Event();
        const char *title = "";
        if ((e != NULL) && (e->Title() != NULL))
           title = e->Title();
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &title))
           esyslog("dbus2vdr: %s.Next: out of memory while appending the title", DBUS_VDR_TIMER_INTERFACE);

        dbus_uint32_t serial = 0;
        if (!dbus_connection_send(_conn, reply, &serial))
           esyslog("dbus2vdr: %s.Next: out of memory while sending the reply", DBUS_VDR_TIMER_INTERFACE);
        dbus_message_unref(reply);
        return;
        }
     else
        cDBusHelper::SendReply(_conn, _msg, 501, *cString::sprintf("Unknown option: \"%s\"", option));
     }
  else
     cDBusHelper::SendReply(_conn, _msg, 550, "No active timers");
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
