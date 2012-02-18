#include "shutdown-wrapper.h"

#include "libvdr-exitpipe.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>


dbus2vdrShutdownWrapper::dbus2vdrShutdownWrapper(const char *Dir)
:_shutdownHooksDir(Dir == NULL ? "" : Dir)
{
}

dbus2vdrShutdownWrapper::~dbus2vdrShutdownWrapper(void)
{
}

int SendReply(int  returncode, const char *message)
{
 if (message != NULL) {
  fprintf(stdout, "%s\n", message);
  fflush(stdout);
  }
 return returncode;
}


int dbus2vdrShutdownWrapper::ConfirmShutdown(const char *Args)
{
  if (strlen(*_shutdownHooksDir) > 0) {
     cStringList hooks;
     cReadDir rd(*_shutdownHooksDir);
     while (struct dirent *file = rd.Next()) {
           if ((strcmp(file->d_name, ".") != 0) && (strcmp(file->d_name, "..") != 0))
              hooks.Append(strdup(file->d_name));
           }
     if (hooks.Size() > 0) {
        cString params(Args == NULL ? "" : Args);

        hooks.Sort();
        cString shutdowncmd;
        for (int i = 0; i < hooks.Size(); i++) {
            cString cmd = cString::sprintf("%s/%s", *_shutdownHooksDir, hooks[i]);
            if (access(*cmd, X_OK) != 0)
               cmd = cString::sprintf("/bin/sh %s/%s", *_shutdownHooksDir, hooks[i]);
            isyslog("dbus2vdr-shutdown-wrapper: asking shutdown-hook %s", *cmd);
            char *tmp = NULL;
            cExitPipe p;
            int ret = -1;
            if (p.Open(*cString::sprintf("%s %s", *cmd, *params), "r")) {
               int l = 0;
               int c;
               while ((c = fgetc(p)) != EOF) {
                     if (l % 20 == 0) {
                        if (char *NewBuffer = (char *)realloc(tmp, l + 21))
                           tmp = NewBuffer;
                        else {
                           esyslog("dbus2vdr-shutdown-wrapper: out of memory");
                           break;
                           }
                        }
                     tmp[l++] = char(c);
                     }
               if (tmp)
                  tmp[l] = 0;
               ret = p.Close();
               }
            else
               esyslog("dbus2vdr-shutdown-wrapper: can't open pipe for command '%s'", *cmd);

            cString result(stripspace(tmp), true); // for automatic free
            isyslog("dbus2vdr-shutdown-wrapper: result(%d) = %s", ret, *result);
            if (ret != 0) {
               if (*result)
                  return SendReply(992, *result);
               return SendReply(999, "shutdown-hook returned a non-zero exit code");
               }

            if (*result) {
               static const char *message = "TRY_AGAIN=\"";
               if ((strlen(*result) > strlen(message)) && startswith(*result, message)) {
                  cString s_try_again = tmp + strlen(message);
                  s_try_again.Truncate(-1);
                  if ((strlen(*s_try_again) > 0) && isnumber(*s_try_again)) {
                     int try_again = strtol(s_try_again, NULL, 10);
                     if (try_again > 0) {
                        return SendReply(991, *result);
                        }
                     }
                  }
               }

            if (*result) {
               static const char *message = "SHUTDOWNCMD=\"";
               if ((strlen(*result) > strlen(message)) && startswith(*result, message))
                  shutdowncmd = result;
               }
            }
        if (*shutdowncmd && (strlen(*shutdowncmd) > 0)) {
           return SendReply(0, *shutdowncmd);
           }
        }
     }

  return SendReply(0, "vdr is ready for shutdown");
}


int main(int argc, char *argv[])
{
  const char *args = "";
  if (argc > 1) {
     dbus2vdrShutdownWrapper wrapper(argv[1]);
     if (argc > 2)
        args = argv[2];
     return wrapper.ConfirmShutdown(args);
     }
  return 0;
}
