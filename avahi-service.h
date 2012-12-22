#ifndef __DBUS2VDR_AVAHI_SERVICE_H
#define __DBUS2VDR_AVAHI_SERVICE_H

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/strlst.h>

#include <vdr/tools.h>


class cAvahiClient;

class cAvahiService : public cListObject
{
friend class cAvahiClient;

private:
  cAvahiClient    *_publisher;
  cString          _caller;
  cString          _id;
  AvahiEntryGroup *_group;
  char            *_name;
  AvahiProtocol    _protocol;
  char            *_type;
  int              _port;
  AvahiStringList *_subtypes;
  AvahiStringList *_txts;

  void CreateService(AvahiClient *client);
  void ResetService(void);
  void DeleteService(void);

  static void GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata);
  void GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state);

  bool Modify(const char *name, AvahiProtocol protocol, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts);

public:
  cAvahiService(cAvahiClient *publisher, const char *caller);
  virtual ~cAvahiService(void);

  cString  Id(void) const { return _id; }
  cString  Caller(void) const { return _caller; }
};

#endif
