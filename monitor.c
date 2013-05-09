#include "monitor.h"
#include "common.h"
#include "helper.h"
#include "message.h"

#include <vdr/plugin.h>
#include <vdr/recording.h>
#include <vdr/tools.h>


cMutex cDBusMonitor::_mutex;
cDBusMonitor *cDBusMonitor::_monitor[] = { NULL, NULL };
int cDBusMonitor::PollTimeoutMs = 10;


cDBusMonitor::cDBusMonitor(cDBusBus *bus, eBusType type)
{
  _bus = bus;
  _type = type;
  _conn = NULL;
  _started = false;
  _nameAcquired = false;
  Start();
  time_t start = time(NULL);
  while (!_started && !_nameAcquired) {
        if ((time(NULL) - start) > 5) { // 5 seconds timeout
           esyslog("dbus2vdr: monitor %d has not been started or connected fast enough...", type);
           break;
           }
        if (!_started)
           isyslog("dbus2vdr: wait for monitor %d to start", type);
        else if (!_nameAcquired)
           isyslog("dbus2vdr: wait for monitor %d to connect", type);
        cCondWait::SleepMs(10);
        }
}

cDBusMonitor::~cDBusMonitor(void)
{
  Cancel(-1);
  Cancel(5);
  _conn = NULL;
  if (_bus != NULL)
     delete _bus;
  _bus = NULL;
}

void cDBusMonitor::StartMonitor(bool enableNetwork)
{
  cMutexLock lock(&_mutex);
  dsyslog("dbus2vdr: StartMonitor Lock");

  cString busname;
#if VDRVERSNUM < 10704
  busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#else
  if (InstanceId > 0)
     busname = cString::sprintf("%s%d", DBUS_VDR_BUSNAME, InstanceId);
  else
     busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#endif

  if (enableNetwork && (_monitor[busNetwork] == NULL))
     _monitor[busNetwork] = new cDBusMonitor(new cDBusNetworkBus(*busname), busNetwork);

  dsyslog("dbus2vdr: StartMonitor Unlock 2");
}

void cDBusMonitor::StopMonitor(void)
{
  cMutexLock lock(&_mutex);
  dsyslog("dbus2vdr: StopMonitor Lock");
  if (_monitor[busSystem] != NULL)
     delete _monitor[busSystem];
  _monitor[busSystem] = NULL;
  if (_monitor[busNetwork] != NULL)
     delete _monitor[busNetwork];
  _monitor[busNetwork] = NULL;
  dsyslog("dbus2vdr: StopMonitor Unlock 2");
}

bool cDBusMonitor::SendSignal(DBusMessage *msg, eBusType bus)
{
  DBusConnection *conn = NULL;
  int retry = 0;
  while (conn == NULL) {
        isyslog("dbus2vdr: retrieving connection for sending signal");
        _mutex.Lock();
        dsyslog("dbus2vdr: SendSignal Lock");
        if (_monitor[bus] != NULL) {
           conn = _monitor[bus]->_conn;
           if ((conn != NULL) && _monitor[bus]->_nameAcquired) {
              dsyslog("dbus2vdr: SendSignal Unlock");
              _mutex.Unlock();
              break;
              }
           }
        dsyslog("dbus2vdr: SendSignal Unlock");
        _mutex.Unlock();
        retry++;
        if (retry > 5) {
           esyslog("dbus2vdr: retrieving connection for sending signal timeout/%d", retry);
           return false;
           }
        dsyslog("dbus2vdr: retrieving connection for sending signal (waiting/%d)", retry);
        cCondWait::SleepMs(1000);
        }

  dsyslog("dbus2vdr: SendSignal: dbus_connection_send");
  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, msg, &serial)) {
     esyslog("dbus2vdr: out of memory while sending signal");
     return false;
     }
  dsyslog("dbus2vdr: SendSignal: dbus_message_unref");
  dbus_message_unref(msg);
  dsyslog("dbus2vdr: signal sent");
  return true;
}

void cDBusMonitor::Action(void)
{
  _started = true;
  isyslog("dbus2vdr: monitor started on bus %s", _bus->Busname());

  int reconnectLogCount = 0;
  bool isLocked = false;
  while (true) {
        if (_conn == NULL) {
           if (!isLocked) {
              _mutex.Lock();
              dsyslog("dbus2vdr: Action Lock");
              isLocked = true;
              }
           // don't get too verbose...
           if (reconnectLogCount < 5)
              isyslog("dbus2vdr: try to connect to %s bus", _bus->Name());
           else if (reconnectLogCount > 15) // ...and too quiet
              reconnectLogCount = 0;
           DBusConnection *conn = _bus->Connect();
           if (conn == NULL) {
              isLocked = false;
              _mutex.Unlock();
              cCondWait::SleepMs(1000);
              if (!Running())
                 break;
              _mutex.Lock();
              isLocked = true;
              reconnectLogCount++;
              continue;
              }

           _conn = conn;
           isyslog("dbus2vdr: connect to %s bus successful, unique name is %s", _bus->Name(), dbus_bus_get_unique_name(_conn));
           reconnectLogCount = 0;
           }
        if (isLocked) {
           isLocked = false;
           dsyslog("dbus2vdr: Action Unlock");
           _mutex.Unlock();
           }
        dbus_connection_read_write_dispatch(_conn, PollTimeoutMs);
        // handle outgoing messages
        if (dbus_connection_has_messages_to_send(_conn)) {
           dsyslog("dbus2vdr: connection has messages to send, flushing");
           dbus_connection_flush(_conn);
           dsyslog("dbus2vdr: done flushing");
           }
        // handle incoming messages
        DBusMessage *msg = dbus_connection_pop_message(_conn);
        if (msg == NULL) {
           if (!Running())
              break;
           continue;
           }
        const char *object = dbus_message_get_path(msg);
        const char *interface = dbus_message_get_interface(msg);
        const char *member = dbus_message_get_member(msg);
        if ((object == NULL) || (interface == NULL) || (member == NULL)) {
           dbus_message_unref(msg);
           continue;
           }

        isyslog("dbus2vdr: new message, object %s, interface %s, member %s", object, interface, member);
        if ((strcmp(interface, "org.freedesktop.DBus") == 0)
         && (strcmp(object, "/org/freedesktop/DBus") == 0)
         && (strcmp(member, "NameAcquired") == 0)) {
          const char *name = NULL;
          DBusMessageIter args;
          if (!dbus_message_iter_init(msg, &args))
             esyslog("dbus2vdr: NameAcquired: message misses an argument for the name");
          else {
             if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &name) < 0)
                esyslog("dbus2vdr: NameAcquired: 'name' argument is not a string");
             }
           if (name != NULL)
              isyslog("dbus2vdr: NameAcquired: get ownership of name %s", name);
           cDBusHelper::SendReply(_conn, msg, "");
           dbus_message_unref(msg);
           _nameAcquired = true;
           continue;
           }

        if ((strcmp(interface, "org.freedesktop.DBus.Local") == 0)
         && (strcmp(object, "/org/freedesktop/DBus/Local") == 0)
         && (strcmp(member, "Disconnected") == 0)) {
           isyslog("dbus2vdr: disconnected from %s bus, will try to reconnect", _bus->Name());
           _conn = NULL;
           _nameAcquired = false;
           _bus->Disconnect();
           continue;
           }

        if (strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
           isyslog("dbus2vdr: introspect object %s with %s", object, member);
           cString data( "");
           if (!cDBusMessageDispatcher::Introspect(msg, data))
              esyslog("dbus2vdr: can't introspect object %s", object);
           cDBusHelper::SendReply(_conn, msg, *data);
           dbus_message_unref(msg);
           continue;
           }

        if (!cDBusMessageDispatcher::Dispatch(this, _conn, msg))
           dbus_message_unref(msg);
        }
  cDBusMessageDispatcher::Stop(_type);
  _bus->Disconnect();
  isyslog("dbus2vdr: monitor stopped on bus %s", _bus->Busname());
}
