#include "helper.h"
#include "bus.h"

#include <vdr/plugin.h>
#include <sys/wait.h>
#include <unistd.h>


void cDBusHelper::AddArg(DBusMessageIter &args, int type, const void *value)
{
  if (value == NULL)
     return;
  if (!dbus_message_iter_append_basic(&args, type, value))
     esyslog("dbus2vdr: AddArg: out of memory while appending argument");
}

void cDBusHelper::AddKeyValue(DBusMessageIter &array, const char *key, int type, const char *vtype, void *value)
{
  DBusMessageIter element;
  DBusMessageIter variant;

  if (!dbus_message_iter_open_container(&array, DBUS_TYPE_STRUCT, NULL, &element))
     esyslog("dbus2vdr: AddKeyValue: can't open struct container");
  AddArg(element, DBUS_TYPE_STRING, &key);
  if (!dbus_message_iter_open_container(&element, DBUS_TYPE_VARIANT, vtype, &variant))
     esyslog("dbus2vdr: AddKeyValue: can't open variant container");
  AddArg(variant, type, value);
  if (!dbus_message_iter_close_container(&element, &variant))
     esyslog("dbus2vdr: AddKeyValue: can't close variant container");
  if (!dbus_message_iter_close_container(&array, &element))
     esyslog("dbus2vdr: AddKeyValue: can't close struct container");
}

int  cDBusHelper::GetNextArg(DBusMessageIter& args, int type, void *value)
{
  if (dbus_message_iter_get_arg_type(&args) != type)
     return -1;
  dbus_message_iter_get_basic(&args, value);
  if (dbus_message_iter_next(&args))
     return 1;
  return 0;
}

void  cDBusHelper::SendReply(DBusConnection *conn, DBusMessage *reply)
{
  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
}

void  cDBusHelper::SendReply(DBusConnection *conn, DBusMessage *msg, int  returncode, const char *message)
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &returncode))
     esyslog("dbus2vdr: SendReply: out of memory while appending the return-code");

  if (message != NULL) {
     if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
        esyslog("dbus2vdr: SendReply: out of memory while appending the reply-message");
     }

  cDBusHelper::SendReply(conn, reply);
}

void  cDBusHelper::SendReply(DBusConnection *conn, DBusMessage *msg, const char *message)
{
  DBusMessage *reply = dbus_message_new_method_return(msg);
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);

  if (message != NULL) {
     if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &message))
        esyslog("dbus2vdr: SendReply: out of memory while appending the reply-message");
     }

  cDBusHelper::SendReply(conn, reply);
}

cDBusTcpAddress *cDBusHelper::GetNetworkAddress(void)
{
  cString filename = cString::sprintf("%s/network-address.conf", cPlugin::ConfigDirectory("dbus2vdr"));
  dsyslog("dbus2vdr: loading network address from file %s", *filename);
  FILE *f = fopen(*filename, "r");
  if (f == NULL)
     return NULL;
  cReadLine r;
  char *line = r.Read(f);
  fclose(f);
  if (line == NULL)
     return NULL;
  DBusError err;
  dbus_error_init(&err);
  DBusAddressEntry **addresses = NULL;
  int len = 0;
  if (!dbus_parse_address(line, &addresses, &len, &err)) {
     esyslog("dbus2vdr: errorparsing address %s: %s", line, err.message);
     dbus_error_free(&err);
     return NULL;
     }
  cDBusTcpAddress *address = NULL;
  for (int i = 0; i < len; i++) {
      if (addresses[i] != NULL) {
         if (strcmp(dbus_address_entry_get_method(addresses[i]), "tcp") == 0) {
            const char *host = dbus_address_entry_get_value(addresses[i], "host");
            const char *port = dbus_address_entry_get_value(addresses[i], "port");
            if ((host != NULL) && (port != NULL) && isnumber(port)) {
               address = new cDBusTcpAddress(host, atoi(port));
               break;
               }
            }
         }
      }
  dbus_address_entries_free(addresses);
  return address;
}


cExitPipe::cExitPipe(void)
{
  pid = -1;
  f = NULL;
}

cExitPipe::~cExitPipe()
{
  Close();
}

bool cExitPipe::Open(const char *Command, const char *Mode)
{
  int fd[2];

  if (pipe(fd) < 0) {
     LOG_ERROR;
     return false;
     }
  if ((pid = fork()) < 0) { // fork failed
     LOG_ERROR;
     close(fd[0]);
     close(fd[1]);
     return false;
     }

  const char *mode = "w";
  int iopipe = 0;

  if (pid > 0) { // parent process
     if (strcmp(Mode, "r") == 0) {
        mode = "r";
        iopipe = 1;
        }
     close(fd[iopipe]);
     if ((f = fdopen(fd[1 - iopipe], mode)) == NULL) {
        LOG_ERROR;
        close(fd[1 - iopipe]);
        }
     return f != NULL;
     }
  else { // child process
     int iofd = STDOUT_FILENO;
     if (strcmp(Mode, "w") == 0) {
        mode = "r";
        iopipe = 1;
        iofd = STDIN_FILENO;
        }
     close(fd[iopipe]);
     if (dup2(fd[1 - iopipe], iofd) == -1) { // now redirect
        LOG_ERROR;
        close(fd[1 - iopipe]);
        _exit(-1);
        }
     else {
        int MaxPossibleFileDescriptors = getdtablesize();
        for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
            close(i); //close all dup'ed filedescriptors
        if (execl("/bin/sh", "sh", "-c", Command, NULL) == -1) {
           LOG_ERROR_STR(Command);
           close(fd[1 - iopipe]);
           _exit(-1);
           }
        }
     _exit(0);
     }
}

int cExitPipe::Close(void)
{
  int ret = -1;

  if (f) {
     fclose(f);
     f = NULL;
     }

  if (pid > 0) {
     int status = 0;
     int i = 5;
     while (i > 0) {
           ret = waitpid(pid, &status, WNOHANG);
           if (ret < 0) {
              if (errno != EINTR && errno != ECHILD) {
                 LOG_ERROR;
                 break;
                 }
              }
           else if (ret == pid)
              break;
           i--;
           cCondWait::SleepMs(100);
           }
     if (!i) {
        kill(pid, SIGKILL);
        ret = -1;
        }
     else if (ret == -1 || !WIFEXITED(status))
        ret = -1;
     else if (WIFEXITED(status))
        ret = WEXITSTATUS(status);
     pid = -1;
     }

  return ret;
}
