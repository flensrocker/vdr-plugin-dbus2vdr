#include "avahi-client.h"
#include "avahi-browser.h"
#include "avahi-service.h"

#include <avahi-common/error.h>
#include <avahi-common/malloc.h>

#include <vdr/plugin.h>


cAvahiClient::cAvahiClient(void)
 :_simple_poll(NULL)
 ,_client(NULL)
 ,_started(false)
{
  Start();
  while (!_started)
        cCondWait::SleepMs(10);
}

cAvahiClient::~cAvahiClient(void)
{
  Stop();
  _browsers.Clear();
  _services.Clear();
}

cAvahiBrowser *cAvahiClient::GetBrowser(const char *id) const
{
  if (id == NULL)
     return NULL;
  cAvahiBrowser *browser = _browsers.First();
  while ((browser != NULL) && (strcmp(*browser->Id(), id) != 0))
        browser = _browsers.Next(browser);
  return browser;
}

cAvahiService *cAvahiClient::GetService(const char *id) const
{
  if (id == NULL)
     return NULL;
  cAvahiService *service = _services.First();
  while ((service != NULL) && (strcmp(*service->Id(), id) != 0))
        service = _services.Next(service);
  return service;
}

void  cAvahiClient::BrowserError(cAvahiBrowser *browser)
{
  esyslog("dbus2vdr/avahi-client: browser error");
}

void  cAvahiClient::ServiceError(cAvahiService *service)
{
  esyslog("dbus2vdr/avahi-client: service error");
}

bool cAvahiClient::ServerIsRunning(void)
{
  bool runs = false;
  if (_client != NULL)
     runs = (avahi_client_get_state(_client) == AVAHI_CLIENT_S_RUNNING);
  return runs;
}

void cAvahiClient::ClientCallback(AvahiClient *client, AvahiClientState state, void *userdata)
{
  if (userdata == NULL)
     return;
  ((cAvahiClient*)userdata)->ClientCallback(client, state);
}

void cAvahiClient::ClientCallback(AvahiClient *client, AvahiClientState state)
{
  if (_client != client) {
     isyslog("dbus2vdr/avahi-client: unexpected client callback");
     return;
     }

  switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
     {
      Lock();
      for (cAvahiBrowser *browser = _browsers.First(); browser ; browser = _browsers.Next(browser))
          browser->Create(client);
      for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
          service->Create(client);
      Unlock();
      break;
     }
    case AVAHI_CLIENT_FAILURE:
     {
      esyslog("dbus2vdr/avahi: client failure: %s", avahi_strerror(avahi_client_errno(client)));
      avahi_simple_poll_quit(_simple_poll);
      break;
     }
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
     {
      Lock();
      for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
          service->Reset();
      Unlock();
      break;
     }
    case AVAHI_CLIENT_CONNECTING:
      break;
    }
}

void  cAvahiClient::NotifyCaller(const char *caller, const char *event, const char *id, const char *data) const
{
  if ((caller == NULL) || (event == NULL) || (id == NULL))
     return;
  cPlugin *plugin = cPluginManager::GetPlugin(caller);
  if (plugin == NULL)
     return;
  cString call = cString::sprintf("event=%s,id=%s%s%s", event, id, (data != NULL ? "," : ""), (data != NULL ? data : ""));
  plugin->Service("avahi4vdr-event", (void*)(*call));
  isyslog("dbus2vdr/avahi-client: notify %s on event %s", caller, *call);
}

cString cAvahiClient::CreateBrowser(const char *caller, AvahiProtocol protocol, const char *type)
{
  Lock();
  cAvahiBrowser *browser = new cAvahiBrowser(this, caller, protocol, type);
  if (browser == NULL) {
     Unlock();
     return "";
     }
  cString id = browser->Id();
  _browsers.Add(browser);
  if (ServerIsRunning())
     browser->Create(_client);
  Unlock();
  return id;
}

void    cAvahiClient::DeleteBrowser(const char *id)
{
  Lock();
  cAvahiBrowser *browser = GetBrowser(id);
  if (browser != NULL) {
     browser->Delete();
     _browsers.Del(browser);
     }
  Unlock();
}

cString cAvahiClient::CreateService(const char *caller, const char *name, AvahiProtocol protocol, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts)
{
  Lock();
  cAvahiService *service = new cAvahiService(this, caller, name, protocol, type, port, subtypes_len, subtypes, txts_len, txts);
  if (service == NULL) {
     Unlock();
     return "";
     }
  cString id = service->Id();
  _services.Add(service);
  if (ServerIsRunning())
     service->Create(_client);
  Unlock();
  return id;
}

void cAvahiClient::DeleteService(const char *id)
{
  Lock();
  cAvahiService *service = GetService(id);
  if (service != NULL) {
     service->Delete();
     _services.Del(service);
     }
  Unlock();
}

void cAvahiClient::Stop(void)
{
  Cancel(-1);
  Lock();
  if (_simple_poll != NULL)
     avahi_simple_poll_quit(_simple_poll);
  Unlock();
  Cancel(5);
}

void cAvahiClient::Action(void)
{
  _started = true;
  isyslog("dbus2vdr/avahi: publisher started");

  int avahiError = 0;
  int reconnectLogCount = 0;
  while (Running()) {
        if (_simple_poll == NULL) {
           // don't get too verbose...
           if (reconnectLogCount < 5)
              isyslog("dbus2vdr/avahi: create simple_poll");
           else if (reconnectLogCount > 15) // ...and too quiet
              reconnectLogCount = 0;

           Lock();
           _simple_poll = avahi_simple_poll_new();
           if (_simple_poll == NULL) {
              Unlock();
              esyslog("dbus2vdr/avahi: error on creating simple_poll");
              cCondWait::SleepMs(1000);
              reconnectLogCount++;
              continue;
              }
           Unlock();
           reconnectLogCount = 0;
           }

        if (_client == NULL) {
           // don't get too verbose...
           if (reconnectLogCount < 5)
              isyslog("dbus2vdr/avahi: create client");
           else if (reconnectLogCount > 15) // ...and too quiet
              reconnectLogCount = 0;

           Lock();
           _client = avahi_client_new(avahi_simple_poll_get(_simple_poll), (AvahiClientFlags)0, ClientCallback, this, &avahiError);
           if (_client == NULL) {
              Unlock();
              esyslog("dbus2vdr/avahi: error on creating client: %s", avahi_strerror(avahiError));
              cCondWait::SleepMs(1000);
              reconnectLogCount++;
              continue;
              }
           Unlock();
           reconnectLogCount = 0;
           }

        avahi_simple_poll_loop(_simple_poll);

        Lock();
        for (cAvahiBrowser *browser = _browsers.First(); browser ; browser = _browsers.Next(browser))
            browser->Delete();
        for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
            service->Delete();
        if (_client != NULL)
           avahi_client_free(_client);
        _client = NULL;
        if (_simple_poll != NULL)
           avahi_simple_poll_free(_simple_poll);
        _simple_poll = NULL;
        Unlock();
        }
}
