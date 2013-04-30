#ifndef __DBUS2VDR_OBJECT_H
#define __DBUS2VDR_OBJECT_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusConnection;
class cDBusObject;

typedef void (*cDBusMethodFunc)(GVariant *Parameters, GDBusMethodInvocation *Invocation);

class cDBusMethod : public cListObject
{
private:
  friend class cDBusObject;

  const char *_name;
  cDBusMethodFunc _method;

  cDBusMethod(const char *Name, cDBusMethodFunc Method)
   :_name(Name),_method(Method)
  {
  };
};

class cDBusObject : public cListObject
{
private:
  friend class cDBusConnection;

  static void  handle_method_call(GDBusConnection       *connection,
                                  const gchar           *sender,
                                  const gchar           *object_path,
                                  const gchar           *interface_name,
                                  const gchar           *method_name,
                                  GVariant              *parameters,
                                  GDBusMethodInvocation *invocation,
                                  gpointer               user_data);

  gchar             *_path;
  GDBusNodeInfo     *_introspection_data;
  guint              _registration_id;
  cDBusConnection   *_connection;
  cList<cDBusMethod> _methods;

  static const GDBusInterfaceVTable _interface_vtable;
  
  void  SetConnection(cDBusConnection *Connection) { _connection = Connection; };
  void  Register(void);
  void  Unregister(void);

protected:
  void  AddMethod(const char *Name, cDBusMethodFunc Method);

public:
  cDBusObject(const char *Path, const char *XmlNodeInfo);
  virtual ~cDBusObject(void);

  const gchar  *Path(void) const { return _path; };

  virtual void  HandleMethodCall(GDBusConnection       *connection,
                                 const gchar           *sender,
                                 const gchar           *object_path,
                                 const gchar           *interface_name,
                                 const gchar           *method_name,
                                 GVariant              *parameters,
                                 GDBusMethodInvocation *invocation);
};

#endif
