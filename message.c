#include "message.h"
#include "helper.h"
#include "monitor.h"

// ----- cDBusMessageHandler: Thead which handles one cDBusMessage ------------

class cDBusMessageHandler : public cThread, cListObject
{
public:
  virtual ~cDBusMessageHandler(void);

  static void NewHandler(cDBusMessage *msg);
  static void DeleteHandler(void);

protected:
  virtual void Action(void);

  cDBusMessage *_msg;

private:
  static cMutex   _listMutex;
  static cCondVar _listCondVar;
  static cList<cDBusMessageHandler> _activeHandler;
  static cList<cDBusMessageHandler> _finishedHandler;

  cDBusMessageHandler(cDBusMessage *msg);
};


cMutex cDBusMessageHandler::_listMutex;
cCondVar cDBusMessageHandler::_listCondVar; // broadcast on moving handler from active to finished list
cList<cDBusMessageHandler> cDBusMessageHandler::_activeHandler;
cList<cDBusMessageHandler> cDBusMessageHandler::_finishedHandler;

cDBusMessageHandler::cDBusMessageHandler(cDBusMessage *msg)
:_msg(msg)
{
  isyslog("dbus2vdr: starting new message handler %p", this);
  SetDescription("dbus2vdr message handler");
  Start();
}

cDBusMessageHandler::~cDBusMessageHandler(void)
{
  Cancel(3);
  if (_msg != NULL)
     delete _msg;
}

void cDBusMessageHandler::NewHandler(cDBusMessage *msg)
{
  cMutexLock lock(&_listMutex);
  if (_finishedHandler.Count() > 0) {
     cDBusMessageHandler *h = _finishedHandler.Get(0);
     isyslog("dbus2vdr: %d idle message handler, reusing %p", _finishedHandler.Count(), h);
     _finishedHandler.Del(h, false);
     h->_msg = msg;
     _activeHandler.Add(h);
     h->Start();
     }
  else
     _activeHandler.Add(new cDBusMessageHandler(msg));
}

void cDBusMessageHandler::DeleteHandler(void)
{
  cMutexLock lock(&_listMutex);
  while (_activeHandler.Count() > 0) {
        isyslog("dbus2vdr: waiting for %d message handlers to finish", _activeHandler.Count());
        _listCondVar.Wait(_listMutex);
        }
  isyslog("dbus2vdr: deleting %d message handlers", _finishedHandler.Count());
  _finishedHandler.Clear();
}

void cDBusMessageHandler::Action(void)
{
  if (_msg != NULL) {
     _msg->Process();
     delete _msg;
     _msg = NULL;
     }
  cMutexLock lock(&_listMutex);
  isyslog("dbus2vdr: moving message handler %p from active to finished", this);
  _activeHandler.Del(this, false);
  _finishedHandler.Add(this);
  _listCondVar.Broadcast();
}

// ----- cDBusMessageDispatcher -----------------------------------------------

cList<cDBusMessageDispatcher> cDBusMessageDispatcher::_dispatcher;

void cDBusMessageDispatcher::AddPath(const char *path)
{
  if (path != NULL)
     _paths.Append(strdup(path));
}

void cDBusMessageDispatcher::AddAction(const char *name, cDBusMessageActionFunc action)
{
  if ((name != NULL) && (action != NULL))
     _actions.Add(new cDBusMessageAction(name, action));
}

bool cDBusMessageDispatcher::Dispatch(cDBusMonitor *monitor, DBusConnection* conn, DBusMessage* msg)
{
  const char *interface = dbus_message_get_interface(msg);
  if (interface == NULL)
     return false;
  const char *path = dbus_message_get_path(msg);
  if (path == NULL)
     return false;
  cString errText;
  DBusMessage *errorMsg;
  eBusType monitorType = monitor->GetType();
  for (cDBusMessageDispatcher *d = _dispatcher.First(); d; d = _dispatcher.Next(d)) {
      if (d->_busType != monitorType)
         continue;
      if (strcmp(d->_interface, interface) == 0) {
         cDBusMessage *m = NULL;
         if ((d->_paths.Size() > 0) && (d->_actions.Count() > 0)) {
            if (d->_paths.Find(path) >= 0) {
               for (cDBusMessageAction *a = d->_actions.First(); a; a = d->_actions.Next(a)) {
                   if (dbus_message_is_method_call(msg, d->_interface, a->Name)) {
                      m = new cDBusMessage(conn, msg, a->Action);
                      break;
                      }
                   }
               }
            }
         if (m == NULL)
            m = d->CreateMessage(conn, msg);
         if (m == NULL)
            continue; // try next dispatcher
         cDBusMessageHandler::NewHandler(m);
         return true;
         }
      }
  errText = cString::sprintf("unknown interface %s on object %s", interface, path);
  esyslog("dbus2vdr: %s", *errText);
  errorMsg = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_INTERFACE, *errText);
  if (errorMsg != NULL)
     cDBusHelper::SendReply(conn, errorMsg);
  return false;
}

bool cDBusMessageDispatcher::Introspect(DBusMessage *msg, cString &Data)
{
  for (cDBusMessageDispatcher *d = _dispatcher.First(); d; d = _dispatcher.Next(d)) {
      if (d->OnIntrospect(msg, Data))
         return true;
      }
  return false;
}

void cDBusMessageDispatcher::Stop(eBusType type)
{
  for (cDBusMessageDispatcher *d = _dispatcher.First(); d; d = _dispatcher.Next(d)) {
      if (d->_busType == type)
         d->OnStop();
      }
}

void cDBusMessageDispatcher::Shutdown(void)
{
  cDBusMessageHandler::DeleteHandler();
  _dispatcher.Clear();
}

cDBusMessageDispatcher::cDBusMessageDispatcher(eBusType busType, const char *interface)
:_busType(busType)
,_interface(interface)
{
  isyslog("dbus2vdr: new message dispatcher for interface %s", _interface);
  _dispatcher.Add(this);
}

cDBusMessageDispatcher::~cDBusMessageDispatcher(void)
{
  isyslog("dbus2vdr: deleting message dispatcher for interface %s", _interface);
}

// ----- cDBusMessage: base class for dbus messages ---------------------------

cDBusMessage::cDBusMessage(DBusConnection *conn, DBusMessage *msg)
:_conn(conn)
,_msg(msg)
,_action(NULL)
{
}

cDBusMessage::cDBusMessage(DBusConnection *conn, DBusMessage *msg, cDBusMessageActionFunc action)
:_conn(conn)
,_msg(msg)
,_action(action)
{
}

cDBusMessage::~cDBusMessage(void)
{
  if (_msg)
     dbus_message_unref(_msg);
}

void cDBusMessage::Process(void)
{
  if (_action != NULL)
     _action(_conn, _msg);
  else
     esyslog("dbus2vdr: no action in message, looks like an error by the developer...");
}
