#include "message.h"


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

bool cDBusMessageDispatcher::Dispatch(DBusConnection* conn, DBusMessage* msg)
{
  const char *interface = dbus_message_get_interface(msg);
  if (interface == NULL)
     return false;
  for (cDBusMessageDispatcher *d = _dispatcher.First(); d; d = _dispatcher.Next(d)) {
      if (strcmp(d->_interface, interface) == 0) {
         cDBusMessage *m = d->CreateMessage(conn, msg);
         if (m == NULL)
            return false;
         cDBusMessageHandler::NewHandler(m);
         return true;
         }
      }
  return false;
}

bool cDBusMessageDispatcher::Introspect(cString &Data)
{
  for (cDBusMessageDispatcher *d = _dispatcher.First(); d; d = _dispatcher.Next(d)) {
      if (d->OnIntrospect(Data))
         return true;
      }
  return false;
}

void cDBusMessageDispatcher::Shutdown(void)
{
  cDBusMessageHandler::DeleteHandler();
  _dispatcher.Clear();
}

cDBusMessageDispatcher::cDBusMessageDispatcher(const char *interface)
:_interface(interface)
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
{
}

cDBusMessage::~cDBusMessage(void)
{
  if (_msg)
     dbus_message_unref(_msg);
}
