#ifndef __DBUS2VDR_OBJECT_H
#define __DBUS2VDR_OBJECT_H

#include <gio/gio.h>

#include <vdr/thread.h>
#include <vdr/tools.h>


class cDBusConnection;
class cDBusObject;

typedef void (*cDBusMethodFunc)(cDBusObject *Object, GVariant *Parameters, GDBusMethodInvocation *Invocation);

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

  static void  do_work(gpointer data, gpointer user_data);
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
  GArray            *_registration_ids;
  cDBusConnection   *_connection;
  cList<cDBusMethod> _methods;

  static const GDBusInterfaceVTable _interface_vtable;
  
  void  SetConnection(cDBusConnection *Connection) { _connection = Connection; };
  void  Register(void);
  void  Unregister(void);

protected:
  void  SetPath(const char *Path);
  void  AddMethod(const char *Name, cDBusMethodFunc Method);

public:
  static void  FreeThreadPool(void);

  cDBusObject(const char *Path, const char *XmlNodeInfo);
  virtual ~cDBusObject(void);

  cDBusConnection *Connection(void) const { return _connection; };
  const gchar  *Path(void) const { return _path; };
};

#endif
