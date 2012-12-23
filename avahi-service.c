#include "avahi-service.h"
#include "avahi-client.h"

#include <uuid/uuid.h>

#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/timeval.h>


cAvahiService::cAvahiService(cAvahiClient *avahi_client, const char *caller, const char *name, AvahiProtocol protocol, const char *type, int port, int subtypes_len, const char **subtypes, int txts_len, const char **txts)
 :_avahi_client(avahi_client)
 ,_caller(caller)
 ,_group(NULL)
 ,_name(NULL)
 ,_protocol(protocol)
 ,_type(NULL)
 ,_port(port)
 ,_subtypes(NULL)
 ,_txts(NULL)
{
  uuid_t id;
  char   sid[40];
  uuid_generate(id);
  uuid_unparse_lower(id, sid);
  _id = sid;
  _name = avahi_strdup(name);
  _type = avahi_strdup(type);
  if (subtypes_len > 0)
     _subtypes = avahi_string_list_new_from_array(subtypes, subtypes_len);
  if (txts_len > 0)
     _txts = avahi_string_list_new_from_array(txts, txts_len);
  dsyslog("dbus2vdr/avahi-service: instanciated service '%s' of type %s listening on port %d (id %s)", _name, _type, _port, *_id);
  _avahi_client->NotifyCaller(*_caller, "service-created", *_id, NULL);
}

cAvahiService::~cAvahiService(void)
{
  _avahi_client->NotifyCaller(*_caller, "service-deleted", *_id, NULL);
  if (_txts != NULL)
     avahi_string_list_free(_txts);
  _txts = NULL;
  if (_subtypes != NULL)
     avahi_string_list_free(_subtypes);
  _subtypes = NULL;
  if (_type != NULL)
     avahi_free(_type);
  _type = NULL;
  if (_name != NULL)
     avahi_free(_name);
  _name = NULL;
}

void cAvahiService::GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state, void *userdata)
{
  if (userdata == NULL)
     return;
  ((cAvahiService*)userdata)->GroupCallback(group, state);
}

void cAvahiService::GroupCallback(AvahiEntryGroup *group, AvahiEntryGroupState state)
{
  if (_group != group) {
     isyslog("dbus2vdr/avahi-service: unexpected group callback");
     return;
     }

  AvahiClient *client = avahi_entry_group_get_client(group);

  switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
     {
       _avahi_client->NotifyCaller(*_caller, "service-started", *_id, NULL);
       isyslog("dbus2vdr/avahi-service: service '%s' successfully established (id %s)", _name, *_id);
       break;
     }
    case AVAHI_ENTRY_GROUP_COLLISION:
     {
       char *n;
       n = avahi_alternative_service_name(_name);
       avahi_free(_name);
       _name = n;
       isyslog("dbus2vdr/avahi-service: service name collision, renaming service to '%s'", _name);
       Create(client);
       break;
     }
    case AVAHI_ENTRY_GROUP_FAILURE:
     {
       esyslog("dbus2vdr/avahi-service: entry group failure: %s", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(group))));
       _avahi_client->ServiceError(this);
       break;
     }
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
       break;
    }
}

void cAvahiService::Create(AvahiClient *client)
{
  char *n;
  int ret;

  if (_group == NULL) {
      _group = avahi_entry_group_new(client, GroupCallback, this);
      if (_group == NULL) {
         esyslog("dbus2vdr/avahi-service: avahi_entry_group_new failed: %s", avahi_strerror(avahi_client_errno(client)));
         goto fail;
         }
      }
  if (avahi_entry_group_is_empty(_group)) {
     ret = avahi_entry_group_add_service(_group, AVAHI_IF_UNSPEC, _protocol, (AvahiPublishFlags)0, _name, _type, NULL, NULL, _port, NULL);
     if (ret  < 0) {
        if (ret == AVAHI_ERR_COLLISION)
            goto collision;
        esyslog("dbus2vdr/avahi-service: failed to add service '%s' of type '%s': %s", _name, _type, avahi_strerror(ret));
        goto fail;
        }

     if ((_subtypes != NULL) && (avahi_string_list_length(_subtypes) > 0)) {
        AvahiStringList *l = _subtypes;
        while (l != NULL) {
              const char *subtype = (const char*)avahi_string_list_get_text(l);
              size_t sublen = avahi_string_list_get_size(l);
              if ((subtype != NULL) && (sublen > 0) && (subtype[0] != 0)) {
                 ret = avahi_entry_group_add_service_subtype(_group, AVAHI_IF_UNSPEC, _protocol, (AvahiPublishFlags)0, _name, _type, NULL, subtype);
                 if (ret < 0)
                    esyslog("dbus2vdr/avahi-service: failed to add subtype %s on '%s' of type '%s': %s", subtype, _name, _type, avahi_strerror(ret));
                 }
              l = avahi_string_list_get_next(l);
              }
        }

     if ((_txts != NULL) && (avahi_string_list_length(_txts) > 0)) {
        ret = avahi_entry_group_update_service_txt_strlst(_group, AVAHI_IF_UNSPEC, _protocol, (AvahiPublishFlags)0, _name, _type, NULL, _txts);
        if (ret < 0)
           esyslog("dbus2vdr/avahi-service: failed to add txt records on '%s' of type '%s': %s", _name, _type, avahi_strerror(ret));
        }

     ret = avahi_entry_group_commit(_group);
     if (ret < 0) {
        esyslog("dbus2vdr/avahi-service: failed to commit entry group of '%s': %s", _name, avahi_strerror(ret));
        goto fail;
        }
     dsyslog("dbus2vdr/avahi-service: created service '%s' (id %s)", _name, *_id);
     }
  return;

collision:
  n = avahi_alternative_service_name(_name);
  avahi_free(_name);
  _name = n;
  isyslog("dbus2vdr/avahi-service: service name collision, renaming service to '%s'", _name);
  Reset();
  Create(client);
  return;

fail:
  if (_group != NULL)
     avahi_entry_group_free(_group);
  _group = NULL;
  _avahi_client->ServiceError(this);
}

void cAvahiService::Reset(void)
{
  if (_group != NULL) {
     avahi_entry_group_reset(_group);
     dsyslog("dbus2vdr/avahi-service: reset service '%s' (id %s)", _name, *_id);
     }
}

void cAvahiService::Delete(void)
{
  if (_group != NULL) {
     avahi_entry_group_free(_group);
     _avahi_client->NotifyCaller(*_caller, "service-stopped", *_id, NULL);
     dsyslog("dbus2vdr/avahi-service: deleted service '%s' (id %s)", _name, *_id);
     _group = NULL;
     }
}
