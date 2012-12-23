#ifndef __DBUS2VDR_AVAHI_BROWSER_H
#define __DBUS2VDR_AVAHI_BROWSER_H

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <vdr/tools.h>


class cAvahiClient;

class cAvahiBrowser : public cListObject
{
friend class cAvahiClient;

private:
  cAvahiClient        *_avahi_client;
  AvahiServiceBrowser *_browser;
  cString              _caller;
  cString              _id;
  AvahiProtocol        _protocol;
  char                *_type;
  bool                 _ignore_local;

  void Create(AvahiClient *client);
  void Delete(void);

  static void BrowserCallback(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name,
                              const char *type, const char *domain, AvahiLookupResultFlags flags, void* userdata);
  void BrowserCallback(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name,
                       const char *type, const char *domain, AvahiLookupResultFlags flags);
  static void ResolverCallback(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                               const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address,
                               uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata);
  void ResolverCallback(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                        const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address,
                        uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags);

public:
  cAvahiBrowser(cAvahiClient *avahi_client, const char *caller, AvahiProtocol protocol, const char *type, bool ignore_local);
  virtual ~cAvahiBrowser(void);

  cString  Id(void) const { return _id; }
  cString  Caller(void) const { return _caller; }
};

#endif
