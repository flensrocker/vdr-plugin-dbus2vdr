#include "object.h"
#include "common.h"
#include "connection.h"


const GDBusInterfaceVTable cDBusObject::_interface_vtable =
{
  cDBusObject::handle_method_call,
  NULL,
  NULL
};

class cWorkerData
{
public:
  static GThreadPool *_thread_pool;
  
  cDBusObject *_object;
  GDBusMethodInvocation *_invocation;
  
  cWorkerData(cDBusObject *Object, GDBusMethodInvocation *Invocation)
  {
    _object = Object;
    _invocation = Invocation;
  };
};

GThreadPool *cWorkerData::_thread_pool = NULL;

void  cDBusObject::FreeThreadPool(void)
{
  if (cWorkerData::_thread_pool != NULL) {
     g_thread_pool_free(cWorkerData::_thread_pool, FALSE, TRUE);
     cWorkerData::_thread_pool = NULL;
     isyslog("dbus2vdr: thread-pool for handling method-calls stopped");
     }
}

void  cDBusObject::do_work(gpointer data, gpointer user_data)
{
  if (data == NULL)
     return;

  cWorkerData *workerData = (cWorkerData*)data;
  const gchar *method_name = g_dbus_method_invocation_get_method_name(workerData->_invocation);
  d4syslog("dbus2vdr: do_work on %s.%s", workerData->_object->Path(), method_name);
  bool err = true;
  for (cDBusMethod *m = workerData->_object->_methods.First(); m; m = workerData->_object->_methods.Next(m)) {
      if (g_strcmp0(m->_name, method_name) == 0) {
         m->_method(workerData->_object, g_dbus_method_invocation_get_parameters(workerData->_invocation), workerData->_invocation);
         err = false;
         break;
         }
      }
  if (err) {
     g_dbus_method_invocation_return_error(workerData->_invocation, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
                                           "method '%s.%s' on object '%s' is not implemented yet",
                                           g_dbus_method_invocation_get_interface_name(workerData->_invocation),
                                           g_dbus_method_invocation_get_method_name(workerData->_invocation),
                                           g_dbus_method_invocation_get_object_path(workerData->_invocation));
     }
  delete workerData;
}

void  cDBusObject::handle_method_call(GDBusConnection *connection, const gchar *sender, const gchar *object_path, const gchar *interface_name, const gchar *method_name, GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
  if (user_data == NULL)
     return;

  d4syslog("dbus2vdr: handle_method_call: sender '%s', object '%s', interface '%s', method '%s'", sender, object_path, interface_name, method_name);
  cWorkerData *workerData = new cWorkerData((cDBusObject*)user_data, invocation);
  if (cWorkerData::_thread_pool == NULL) {
     GError *err = NULL;
     cWorkerData::_thread_pool = g_thread_pool_new(do_work, NULL, 10, FALSE, &err);
     if (err != NULL) {
        esyslog("dbus2vdr: g_thread_pool_new reports: %s", err->message);
        g_error_free(err);
        if (cWorkerData::_thread_pool != NULL)
           g_thread_pool_free(cWorkerData::_thread_pool, TRUE, FALSE);
        cWorkerData::_thread_pool = NULL;
        do_work(workerData, NULL);
        return;
        }
     isyslog("dbus2vdr: thread-pool for handling method-calls started");
     }
  g_thread_pool_push(cWorkerData::_thread_pool, workerData, NULL);
}

cDBusObject::cDBusObject(const char *Path, const char *XmlNodeInfo)
{
  _path = g_strdup(Path);
  _registration_ids = NULL;
  _connection = NULL;

  GError *err = NULL;
  _introspection_data = g_dbus_node_info_new_for_xml(XmlNodeInfo, &err);
  if (err != NULL) {
     esyslog("dbus2vdr: g_dbus_node_info_new_for_xml reports: %s", err->message);
     g_error_free(err);
     }
}

cDBusObject::~cDBusObject(void)
{
  if (_introspection_data != NULL) {
     g_dbus_node_info_unref(_introspection_data);
     _introspection_data = NULL;
     }

  if (_path != NULL) {
     g_free(_path);
     _path = NULL;
     }
  if (_registration_ids != NULL) {
     g_array_free(_registration_ids, TRUE);
     _registration_ids = NULL;
     }
}

void  cDBusObject::Register(void)
{
  if ((_connection == NULL) || (_connection->GetConnection() == NULL))
     return;

  if (_registration_ids != NULL)
     Unregister();
  int len = 0;
  while (_introspection_data->interfaces[len] != NULL)
        len++;
  if (_registration_ids != NULL)
     g_array_free(_registration_ids, TRUE);
  _registration_ids = g_array_new(FALSE, TRUE, sizeof(guint));
  for (int i = 0; i < len; i++) {
      guint id = g_dbus_connection_register_object(_connection->GetConnection(), Path(), _introspection_data->interfaces[i], &_interface_vtable, this, NULL, NULL);
      g_array_append_val(_registration_ids, id);
      d4syslog("dbus2vdr: %s: register object %s with interface %s on id %d", _connection->Name(), Path(), _introspection_data->interfaces[i]->name, id);
      }
}

void  cDBusObject::Unregister(void)
{
  if (_registration_ids != NULL) {
     if ((_connection != NULL) && (_connection->GetConnection() != NULL)) {
        for (guint i = 0; i < _registration_ids->len; i++) {
            guint id = g_array_index(_registration_ids, guint, i);
            if (id != 0) {
               d4syslog("dbus2vdr: %s: unregister object %s with interface %s on id %d", _connection->Name(), Path(), _introspection_data->interfaces[i]->name, id);
               g_dbus_connection_unregister_object(_connection->GetConnection(), id);
               }
            }
        }
     g_array_free(_registration_ids, TRUE);
     _registration_ids = NULL;
     }
}

void  cDBusObject::SetPath(const char *Path)
{
  if (_path != NULL) {
     g_free(_path);
     _path = NULL;
     }
  _path = g_strdup(Path);
}

void  cDBusObject::AddMethod(const char *Name, cDBusMethodFunc Method)
{
  if ((Name != NULL) && (Method != NULL))
     _methods.Add(new cDBusMethod(Name, Method));
}
