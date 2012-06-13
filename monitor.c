#include "monitor.h"
#include "common.h"
#include "helper.h"
#include "plugin.h"

#include <vdr/plugin.h>
#include <vdr/tools.h>


cMutex cDBusMonitor::_mutex;
cDBusMonitor *cDBusMonitor::_monitor = NULL;


cDBusMonitor::cDBusMonitor(void)
{
  _conn = NULL;
  started = false;
  nameAcquired = false;
}

cDBusMonitor::~cDBusMonitor(void)
{
  if (_conn != NULL)
     dbus_connection_unref(_conn);
  _conn = NULL;
}

void cDBusMonitor::StartMonitor(void)
{
  cMutexLock lock(&_mutex);
  if (_monitor != NULL)
     return;
  _monitor = new cDBusMonitor;
  if (_monitor) {
     _monitor->Start();
     while (!_monitor->started)
           cCondWait::SleepMs(10);
     }
}

void cDBusMonitor::StopMonitor(void)
{
  cMutexLock lock(&_mutex);
  if (_monitor == NULL)
     return;
  _monitor->Cancel(-1);
  _monitor->Cancel(5);
  delete _monitor;
  _monitor = NULL;
}

bool cDBusMonitor::SendSignal(DBusMessage *msg)
{
  DBusConnection *conn = NULL;
  int retry = 0;
  while (conn == NULL) {
        isyslog("dbus2vdr: retrieving connection for sending signal");
        _mutex.Lock();
        if (_monitor != NULL) {
           conn = _monitor->_conn;
           if ((conn != NULL) && _monitor->nameAcquired) {
              _mutex.Unlock();
              break;
              }
           }
        _mutex.Unlock();
        retry++;
        if (retry > 5) {
           esyslog("dbus2vdr: retrieving connection for sending signal timeout/%d", retry);
           return false;
           }
        dsyslog("dbus2vdr: retrieving connection for sending signal (waiting/%d)", retry);
        cCondWait::SleepMs(1000);
        }

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, msg, &serial)) {
     esyslog("dbus2vdr: out of memory while sending signal");
     return false;
     }
  dbus_connection_flush(conn);
  dbus_message_unref(msg);
  dsyslog("dbus2vdr: signal sent");
  return true;
}

void cDBusMonitor::Action(void)
{
  DBusError err;
  dbus_error_init(&err);
  started = true;
  isyslog("dbus2vdr: monitor started on bus %s", DBUS_VDR_BUSNAME);

  int reconnectLogCount = 0;
  bool isLocked = false;
  while (true) {
        if (!Running())
           break;
        if (_conn == NULL) {
           if (!isLocked) {
              _mutex.Lock();
              isLocked = true;
              }
           // don't get too verbose...
           if (reconnectLogCount < 5)
              isyslog("dbus2vdr: try to connect to system bus");
           else if (reconnectLogCount > 15) // ...and too quiet
              reconnectLogCount = 0;
           DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
           if (dbus_error_is_set(&err)) {
              esyslog("dbus2vdr: connection error: %s", err.message);
              dbus_error_free(&err);
              }
           if (conn == NULL) {
              cCondWait::SleepMs(1000);
              reconnectLogCount++;
              continue;
              }
           dbus_connection_set_exit_on_disconnect(conn, false);

           int ret = dbus_bus_request_name(conn, DBUS_VDR_BUSNAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
           if (dbus_error_is_set(&err)) {
              esyslog("dbus2vdr: name error: %s", err.message);
              dbus_error_free(&err);
              }
           if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
              esyslog("dbus2vdr: not primary owner for bus %s", DBUS_VDR_BUSNAME);
              cCondWait::SleepMs(1000);
              continue;
              }
           _conn = conn;
           isyslog("dbus2vdr: connect to system bus successful");
           reconnectLogCount = 0;
           }
        if (isLocked) {
           isLocked = false;
           _mutex.Unlock();
           }
        dbus_connection_read_write(_conn, 1000);
        if (!Running())
           break;
        DBusMessage *msg = dbus_connection_pop_message(_conn);
        if (msg == NULL)
           continue;
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
           nameAcquired = true;
           continue;
           }

        if ((strcmp(interface, "org.freedesktop.DBus.Local") == 0)
         && (strcmp(object, "/org/freedesktop/DBus/Local") == 0)
         && (strcmp(member, "Disconnected") == 0)) {
           isyslog("dbus2vdr: disconnected from system bus, will try to reconnect");
           _conn = NULL;
           nameAcquired = false;
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

        if (!cDBusMessageDispatcher::Dispatch(_conn, msg)) {
           isyslog("dbus2vdr: don't know what to do...");
           cDBusHelper::SendReply(_conn, msg, -1, "unknown message");
           dbus_message_unref(msg);
           }
        }
  cDBusMessageDispatcher::Stop();
  isyslog("dbus2vdr: monitor stopped on bus %s", DBUS_VDR_BUSNAME);
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
                               if (cDBusMonitor::SendSignal(msg)) {
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
  cUpstartSignal *onesignal =  new cUpstartSignal(action, "vdr-plugin");;
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
