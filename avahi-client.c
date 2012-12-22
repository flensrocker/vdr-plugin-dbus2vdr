#include "avahi-client.h"
#include "avahi-service.h"

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>


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
  _services.Clear();
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
     esyslog("dbus2vdr/avahi-client: unexpected client callback");
     return;
     }

  switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
     {
      for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
          service->CreateService(client);
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
      for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
          service->ResetService();
       break;
     }
    case AVAHI_CLIENT_CONNECTING:
       break;
    }
}

void cAvahiClient::ModifyCallback(AvahiTimeout *e, void *userdata)
{
  if (userdata == NULL)
     return;
  cAvahiService *service = ((cAvahiService*)userdata);
  cAvahiClient  *client = service->_publisher;
  if (client->ServerIsRunning()) {
     service->ResetService();
     service->CreateService(client->_client);
     avahi_simple_poll_get(client->_simple_poll)->timeout_free(e);
     }
}

cString cAvahiClient::CreateService(const char *caller, const char *name, AvahiProtocol protocol, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts)
{
  Lock();
  cAvahiService *service = new cAvahiService(this, caller);
  if (service == NULL) {
     Unlock();
     return "";
     }
  _services.Add(service);
  if (service->Modify(name, protocol, type, port, subtypes_len, subtypes, txts_len, txts) && ServerIsRunning()) {
     struct timeval tv;
     avahi_simple_poll_get(_simple_poll)->timeout_new(avahi_simple_poll_get(_simple_poll), avahi_elapse_time(&tv, 1, 0), ModifyCallback, service);
     }
  cString id = service->Id();
  Unlock();
  return id;
}

void cAvahiClient::DeleteService(const char *id)
{
  Lock();
  cAvahiService *service = GetService(id);
  if (service != NULL) {
     service->DeleteService();
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
        for (cAvahiService *service = _services.First(); service ; service = _services.Next(service))
            service->DeleteService();
        if (_client != NULL)
           avahi_client_free(_client);
        _client = NULL;
        if (_simple_poll != NULL)
           avahi_simple_poll_free(_simple_poll);
        _simple_poll = NULL;
        Unlock();
        }
}
