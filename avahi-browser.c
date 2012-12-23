#include "avahi-browser.h"
#include "avahi-client.h"

#include <uuid/uuid.h>

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>


cAvahiBrowser::cAvahiBrowser(cAvahiClient *avahi_client, const char *caller, AvahiProtocol protocol, const char *type, bool ignore_local)
 :_avahi_client(avahi_client)
 ,_browser(NULL)
 ,_caller(caller)
 ,_protocol(protocol)
 ,_type(NULL)
 ,_ignore_local(ignore_local)
{
  uuid_t id;
  char   sid[40];
  uuid_generate(id);
  uuid_unparse_lower(id, sid);
  _id = sid;
  _type = avahi_strdup(type);
  dsyslog("dbus2vdr/avahi-browser: instanciated browser of type %s (id %s)", _type, *_id);
  _avahi_client->NotifyCaller(*_caller, "browser-created", *_id, NULL);
}

cAvahiBrowser::~cAvahiBrowser(void)
{
  _avahi_client->NotifyCaller(*_caller, "browser-deleted", *_id, NULL);
  if (_type != NULL)
     avahi_free(_type);
  _type = NULL;
}

void cAvahiBrowser::BrowserCallback(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name,
                     const char *type, const char *domain, AvahiLookupResultFlags flags, void* userdata)
{
  if (userdata == NULL)
     return;
  ((cAvahiBrowser*)userdata)->BrowserCallback(browser, interface, protocol, event, name, type, domain, flags);
}

void cAvahiBrowser::BrowserCallback(AvahiServiceBrowser *browser, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char *name,
                     const char *type, const char *domain, AvahiLookupResultFlags flags)
{
  if (_browser != browser) {
     isyslog("dbus2vdr/avahi-browser: unexpected browser callback");
     return;
     }

  switch (event) {
    case AVAHI_BROWSER_FAILURE:
     {
       esyslog("dbus2vdr/avahi-browser: failure %s", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(browser))));
       _avahi_client->BrowserError(this);
       return;
     }
    case AVAHI_BROWSER_NEW:
     {
      if (!_ignore_local || ((flags & AVAHI_LOOKUP_RESULT_LOCAL) == 0)) {
         isyslog("dbus2vdr/avahi-browser: new service '%s' of type %s (id %s)", name, type, *_id);
         if (avahi_service_resolver_new(_avahi_client->_client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, ResolverCallback, this) == NULL)
            esyslog("dbus2vdr/avahi-browser: failed to resolve service '%s' (id %s): %s", name, *_id, avahi_strerror(avahi_client_errno(_avahi_client->_client)));
         }
      break;
     }
    case AVAHI_BROWSER_REMOVE:
     {
      if (!_ignore_local || ((flags & AVAHI_LOOKUP_RESULT_LOCAL) == 0)) {
         isyslog("dbus2vdr/avahi-browser: remove of service '%s' of type %s (id %s)", name, type, *_id);
         _avahi_client->NotifyCaller(*_caller, "browser-service-removed", *_id, NULL);
         }
      break;
     }
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
     {
      isyslog("dbus2vdr/avahi-browser: %s", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
      break;
     }
    }
}

void cAvahiBrowser::ResolverCallback(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                                     const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address,
                                     uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags, void* userdata)
{
  if (userdata == NULL)
     return;
  ((cAvahiBrowser*)userdata)->ResolverCallback(resolver, interface, protocol, event, name, type, domain, host_name, address, port, txt, flags);
}

void cAvahiBrowser::ResolverCallback(AvahiServiceResolver *resolver, AvahiIfIndex interface, AvahiProtocol protocol, AvahiResolverEvent event,
                                     const char *name, const char *type, const char *domain, const char *host_name, const AvahiAddress *address,
                                     uint16_t port, AvahiStringList *txt, AvahiLookupResultFlags flags)
{
  if (resolver == NULL)
     return;

  switch (event) {
    case AVAHI_RESOLVER_FAILURE:
       esyslog("dbus2vdr/avahi-resolver: failed to resolve service '%s' of type %s (id %s): %s", name, type, *_id, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(resolver))));
       break;
    case AVAHI_RESOLVER_FOUND:
     {
      isyslog("dbus2vdr/avahi-resolver: resolved service '%s' of type %s (id %s)", name, type, *_id);
      bool isLocal = false;
      if (flags & AVAHI_LOOKUP_RESULT_LOCAL)
         isLocal = true;

      /*char a[AVAHI_ADDRESS_STR_MAX], *t;
      avahi_address_snprint(a, sizeof(a), address);
      t = avahi_string_list_to_string(txt);
      fprintf(stderr,
              "\t%s:%u (%s)\n"
              "\tTXT=%s\n"
              "\tcookie is %u\n"
              "\tis_local: %i\n"
              "\tour_own: %i\n"
              "\twide_area: %i\n"
              "\tmulticast: %i\n"
              "\tcached: %i\n",
              host_name, port, a,
              t,
              avahi_string_list_get_service_cookie(txt),
              !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
              !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
              !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
              !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
              !!(flags & AVAHI_LOOKUP_RESULT_CACHED));

      avahi_free(t);*/
      _avahi_client->NotifyCaller(*_caller, "browser-service-resolved", *_id, NULL);
      break;
      }
    }

  avahi_service_resolver_free(resolver);
}

void cAvahiBrowser::Create(AvahiClient *client)
{
  if (_browser == NULL) {
     _browser = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, _type, NULL, (AvahiLookupFlags)0, BrowserCallback, this);
     if (_browser == NULL)
        esyslog("dbus2vdr/avahi-browser: failure on creating service browser on type %s (id %s): %s", _type, *_id, avahi_strerror(avahi_client_errno(client)));
     }
}

void cAvahiBrowser::Delete(void)
{
  if (_browser != NULL) {
     avahi_service_browser_free(_browser);
     _avahi_client->NotifyCaller(*_caller, "browser-stopped", *_id, NULL);
     dsyslog("dbus2vdr/avahi-browser: deleted browser on type '%s' (id %s)", _type, *_id);
     _browser = NULL;
     }
}
