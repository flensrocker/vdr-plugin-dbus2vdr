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


cDBusMonitor::cDBusMonitor(cDBusBus *bus)
{
  _bus = bus;
  _conn = NULL;
  _started = false;
  _nameAcquired = false;
  Start();
  while (!_started)
        cCondWait::SleepMs(10);
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

void cDBusMonitor::StartMonitor(void)
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

  if (_monitor[busSystem] == NULL)
     _monitor[busSystem] = new cDBusMonitor(new cDBusSystemBus(*busname));

  dsyslog("dbus2vdr: StartMonitor Unlock 2");
}

void cDBusMonitor::StopMonitor(void)
{
  cMutexLock lock(&_mutex);
  dsyslog("dbus2vdr: StopMonitor Lock");
  if (_monitor[busSystem] != NULL)
     delete _monitor[busSystem];
  _monitor[busSystem] = NULL;
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
              isyslog("dbus2vdr: try to connect to system bus");
           else if (reconnectLogCount > 15) // ...and too quiet
              reconnectLogCount = 0;
           DBusConnection *conn = _bus->Connect();
           if (conn == NULL) {
              cCondWait::SleepMs(1000);
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

        if (!cDBusMessageDispatcher::Dispatch(_conn, msg))
           dbus_message_unref(msg);
        }
  cDBusMessageDispatcher::Stop();
  isyslog("dbus2vdr: monitor stopped on bus %s", _bus->Busname());
}

class cUpstartSignal : public cListObject
{
public:
  const char *_name;
  const char *_signal;
  cStringList _parameters;

  cUpstartSignal(const char *name, const char *signal)
   :_name(name),_signal(signal)
  {
  }

  virtual ~cUpstartSignal(void)
  {
  }
};

class cUpstartSignalSender : public cThread
{
private:
  cMutex                signalMutex;
  cCondWait             signalWait;
  cList<cUpstartSignal> signalQueue;

protected:
  virtual void Action(void)
  {
    DBusMessageIter args;
    DBusMessageIter array;
    while (Running() || (signalQueue.Count() > 0)) {
          cUpstartSignal *signal = NULL;
          signalMutex.Lock();
          signal = signalQueue.First();
          if (signal != NULL) {
             signalQueue.Del(signal, false);
             dsyslog("dbus2vdr: dequeue signal %s/%s", signal->_signal, signal->_name);
             }
          signalMutex.Unlock();
          if (signal != NULL) {
             bool msgError = true;
             DBusMessage *msg = dbus_message_new_method_call("com.ubuntu.Upstart", "/com/ubuntu/Upstart", "com.ubuntu.Upstart0_6", "EmitEvent");
             if (msg != NULL) {
                dbus_message_iter_init_append(msg, &args);
                if (dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &signal->_signal)) {
                   if (dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array)) {
                      bool parameterOk = true;
                      for (int p = 0; p < signal->_parameters.Size(); p++) {
                          const char *parameter = signal->_parameters[p];
                          if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &parameter)) {
                             parameter = false;
                             break;
                             }
                          }
                      if (parameterOk) {
                         if (dbus_message_iter_close_container(&args, &array)) {
                            int nowait = 1;
                            if (dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &nowait)) {
                               if (cDBusMonitor::SendSignal(msg, cDBusMonitor::busSystem)) {
                                  msg = NULL;
                                  msgError = false;
                                  isyslog("dbus2vdr: emit upstart-signal %s for %s", signal->_signal, signal->_name);
                                  }
                               }
                            }
                         }
                      }
                   }
                if (msg != NULL)
                   dbus_message_unref(msg);
                }
             if (msgError)
                esyslog("dbus2vdr: can't emit upstart-signal %s for %s", signal->_signal, signal->_name);
             delete signal;
             }
          if (Running()) {
             signalMutex.Lock();
             int sigCount = signalQueue.Count();
             signalMutex.Unlock();
             if (sigCount == 0)
                signalWait.Wait(1000);
             }
          }
  }

public:
  static cUpstartSignalSender *sender;

  cUpstartSignalSender(void)
  {
    isyslog("dbus2vdr: new DBus-Upstart-Signal-Sender");
    SetDescription("dbus2vdr: DBus-Upstart-Signal-Sender");
    Start();
  }

  virtual ~cUpstartSignalSender(void)
  {
    isyslog("dbus2vdr: delete DBus-Upstart-Signal-Sender");
    Cancel(10);
  }

  void SendCancel(void)
  {
    isyslog("dbus2vdr: stop DBus-Upstart-Signal-Sender");
    Cancel(-1);
    signalWait.Signal();
  }

  void AddSignal(cUpstartSignal *signal, bool broadcast)
  {
    cMutexLock MutexLock(&signalMutex);
    if (signal != NULL)
       signalQueue.Add(signal);
    if (broadcast)
       signalWait.Signal();
  }
};

cUpstartSignalSender *cUpstartSignalSender::sender = NULL;

void cDBusMonitor::SendUpstartSignal(const char *action)
{
  if (cUpstartSignalSender::sender == NULL)
     cUpstartSignalSender::sender = new cUpstartSignalSender();
  isyslog("dbus2vdr: emit upstart-signal %s for all plugins", action);
  cPlugin *plugin;
  cUpstartSignal *onesignal = new cUpstartSignal(action, "vdr-plugin");;
  for (int i = 0; (plugin = cPluginManager::GetPlugin(i)) != NULL; i++)
      onesignal->_parameters.Append(strdup(*cString::sprintf("%s=%s", plugin->Name(), action)));
  cUpstartSignalSender::sender->AddSignal(onesignal, true);
  isyslog("dbus2vdr: upstart-signal %s queued", action);
}

void cDBusMonitor::StopUpstartSender(void)
{
  if (cUpstartSignalSender::sender != NULL) {
     cUpstartSignalSender::sender->SendCancel();
     while (cUpstartSignalSender::sender->Active())
           cCondWait::SleepMs( 100);
     delete cUpstartSignalSender::sender;
     cUpstartSignalSender::sender = NULL;
     }
}
