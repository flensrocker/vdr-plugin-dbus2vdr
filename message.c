#include "message.h"


cMutex cDBusMessageHandler::_listMutex;
cCondVar cDBusMessageHandler::_listCondVar;
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

void cDBusMessage::Dispatch(cDBusMessage *msg)
{
  if (msg == NULL)
     return;
  cDBusMessageHandler::NewHandler(msg);
}

void cDBusMessage::StopDispatcher(void)
{
  cDBusMessageHandler::DeleteHandler();
}

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
