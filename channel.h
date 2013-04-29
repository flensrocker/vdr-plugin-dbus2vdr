#ifndef __DBUS2VDR_CHANNEL_H
#define __DBUS2VDR_CHANNEL_H

#include "message.h"
#include "object.h"


class cDBusChannels : public cDBusObject
{
public:
  static const char *XmlNodeInfo;

  cDBusChannels(void);
  virtual ~cDBusChannels(void);

  virtual void  HandleMethodCall(GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *method_name,
                                 GVariant              *parameters,
                                 GDBusMethodInvocation *invocation);
};


class cDBusDispatcherChannel : public cDBusMessageDispatcher
{
public:
  cDBusDispatcherChannel(void);
  virtual ~cDBusDispatcherChannel(void);

protected:
  virtual bool          OnIntrospect(DBusMessage *msg, cString &Data);
};

#endif
