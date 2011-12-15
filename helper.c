#include "helper.h"

#include <vdr/tools.h>
#include <sys/wait.h>
#include <unistd.h>


int  cDBusHelper::GetNextArg(DBusMessageIter& args, int type, void *value)
{
  if (dbus_message_iter_get_arg_type(&args) != type)
     return -1;
  dbus_message_iter_get_basic(&args, value);
  if (dbus_message_iter_next(&args))
     return 1;
  return 0;
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

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
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

  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(conn, reply, &serial))
     esyslog("dbus2vdr: SendReply: out of memory while sending the reply");
  dbus_message_unref(reply);
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
