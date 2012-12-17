#ifndef __DBUS2VDR_PUBLISH_H
#define __DBUS2VDR_PUBLISH_H

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/strlst.h>

#include <vdr/thread.h>


class cAvahiPublish : public cThread
{
private:
  AvahiSimplePoll *_simple_poll;
  AvahiClient     *_client;
  AvahiEntryGroup *_group;
  char *_name;
  char *_type;
  int _port;
  AvahiStringList *_subtypes;
  AvahiStringList *_txts;
  bool _started;

  static void ModifyCallback(AvahiTimeout *e, void *userdata);
  void ModifyCallback(AvahiTimeout *e);
  
  static void ClientCallback(AvahiClient *client, AvahiClientState state, void *userdata);
  void ClientCallback(AvahiClient *client, AvahiClientState state);
  void CreateServices(AvahiClient *client);

  static void GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);
  void GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state);

protected:
  virtual void Action(void);

public:
  cAvahiPublish(const char *name, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts);
  virtual ~cAvahiPublish(void);

  void Modify(const char *name, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts);
  void Stop(void);
};

#endif
