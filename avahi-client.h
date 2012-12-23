#ifndef __DBUS2VDR_AVAHI_CLIENT_H
#define __DBUS2VDR_AVAHI_CLIENT_H

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/strlst.h>

#include <vdr/thread.h>
#include <vdr/tools.h>

class cAvahiBrowser;
class cAvahiService;


class cAvahiClient : public cThread
{
friend class cAvahiBrowser;
friend class cAvahiService;

private:
  AvahiSimplePoll *_simple_poll;
  AvahiClient     *_client;
  cList<cAvahiBrowser> _browsers;
  cList<cAvahiService> _services;
  bool _started;

  cAvahiBrowser *GetBrowser(const char *id) const;
  cAvahiService *GetService(const char *id) const;

  void  BrowserError(cAvahiBrowser *browser);
  void  ServiceError(cAvahiService *service);

  static void ClientCallback(AvahiClient *client, AvahiClientState state, void *userdata);
  void ClientCallback(AvahiClient *client, AvahiClientState state);

  void  NotifyCaller(const char *caller, const char *event, const char *id, const char *data) const;

protected:
  virtual void Action(void);

public:
  cAvahiClient(void);
  virtual ~cAvahiClient(void);

  bool  ServerIsRunning(void);

  cString CreateBrowser(const char *caller, AvahiProtocol protocol, const char *type, bool ignore_local);
  void    DeleteBrowser(const char *id);

  cString CreateService(const char *caller, const char *name, AvahiProtocol protocol, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts);
  void    DeleteService(const char *id);

  void Stop(void);
};

#endif
