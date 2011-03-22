#include "epg.h"
#include "common.h"
#include "helper.h"

#include <vdr/epg.h>


cDBusMessageEPG::cDBusMessageEPG(cDBusMessageEPG::eAction action, DBusConnection* conn, DBusMessage* msg)
:cDBusMessage(conn, msg)
,_action(action)
{
}

cDBusMessageEPG::~cDBusMessageEPG(void)
{
}

void cDBusMessageEPG::Process(void)
{
  switch (_action) {
    case dmePutFile:
      PutFile();
      break;
    }
}

void cDBusMessageEPG::PutFile(void)
{
  const char *filename = NULL;
  DBusMessageIter args;
  if (!dbus_message_iter_init(_msg, &args))
     esyslog("dbus2vdr: %s.PutFile: message misses an argument for the filename", DBUS_VDR_EPG_INTERFACE);
  else {
     if (cDBusHelper::GetNextArg(args, DBUS_TYPE_STRING, &filename) < 0)
        esyslog("dbus2vdr: %s.SVDRPCommand: 'filename' argument is not a string", DBUS_VDR_EPG_INTERFACE);
     }

  if (filename != NULL) {
     FILE *f = fopen(filename, "r");
     if (f) {
        if (cSchedules::Read(f))
           cSchedules::Cleanup(true);
        fclose(f);
        }
     else
        esyslog("dbus2vdr: %s.PutFile: error opening %s", DBUS_VDR_EPG_INTERFACE, filename);
     }

  cDBusHelper::SendVoidReply(_conn, _msg);
}


cDBusDispatcherEPG::cDBusDispatcherEPG(void)
:cDBusMessageDispatcher(DBUS_VDR_EPG_INTERFACE)
{
}

cDBusDispatcherEPG::~cDBusDispatcherEPG(void)
{
}

cDBusMessage *cDBusDispatcherEPG::CreateMessage(DBusConnection* conn, DBusMessage* msg)
{
  if ((conn == NULL) || (msg == NULL))
     return NULL;

  const char *object = dbus_message_get_path(msg);
  if ((object == NULL) || (strcmp(object, "/EPG") != 0))
     return NULL;

  if (dbus_message_is_method_call(msg, DBUS_VDR_EPG_INTERFACE, "PutFile"))
     return new cDBusMessageEPG(cDBusMessageEPG::dmePutFile, conn, msg);

  return NULL;
}
