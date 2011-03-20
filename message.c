#include "message.h"


cMutex               _msgQueueMutex;
cList<cDBusMessage>  _msgQueue;
cDBusMessageHandler *_msgQueueHandler = NULL;

cDBusMessageHandler::cDBusMessageHandler(void)
{
  SetDescription("dbus2vdr message handler");
  Start();
}

cDBusMessageHandler::~cDBusMessageHandler(void)
{
  Cancel(3);
  _msgQueue.Clear();
}

void cDBusMessageHandler::Action(void)
{
  while (Running()) {
        cMutexLock lock(&_msgQueueMutex);
        while (_msgQueue.Count() > 0) {
              if (!Running())
                 return;
              cDBusMessage *msg = _msgQueue.Get(0);
              if (msg != NULL) {
                 _msgQueue.Del(msg, false);
                 msg->Process();
                 delete msg;
                 }
              }
     }
}

void cDBusMessage::Dispatch(cDBusMessage *msg)
{
  if (msg == NULL)
     return;
  cMutexLock lock(&_msgQueueMutex);
  _msgQueue.Add(msg);
  if (_msgQueueHandler == NULL)
     _msgQueueHandler = new cDBusMessageHandler;
}

void cDBusMessage::StopMessageQueue(void)
{
  if (_msgQueueHandler != NULL)
     delete _msgQueueHandler;
  _msgQueueHandler = NULL;
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
