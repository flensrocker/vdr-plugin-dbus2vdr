#include "network.h"

#include "recording.h"
#include "timer.h"
#include "vdr.h"

#include <dbus/dbus.h>
#include <vdr/tools.h>


static char *decode_event(GFileMonitorEvent ev)
{
  char *fmt = (char*)g_malloc0(1024);
  int caret = 0;

#define dc(x) \
  case G_FILE_MONITOR_EVENT_##x: \
  strcat(fmt, #x); \
  caret += strlen(#x); \
  fmt[caret] = ':'; \
  break;

  switch (ev) {
    dc(CHANGED);
    dc(CHANGES_DONE_HINT);
    dc(DELETED);
    dc(CREATED);
    dc(ATTRIBUTE_CHANGED);
    dc(PRE_UNMOUNT);
    dc(UNMOUNTED);
    dc(MOVED);
  }
#undef dc

  return fmt;
}

gboolean  cDBusNetwork::do_connect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: do_connect", net->Name());

  if (g_file_test(net->_filename, G_FILE_TEST_EXISTS)) {
     if (net->_address != NULL)
        delete net->_address;
     net->_address = cDBusNetworkAddress::LoadFromFile(net->_filename);
     if (net->_address != NULL) {
        if (net->_connection != NULL)
           delete net->_connection;
        net->_connection = new cDBusConnection(net->_busname, net->Name(), net->_address->Address(), net->_context);
        net->_connection->AddObject(new cDBusRecordingsConst);
        net->_connection->AddObject(new cDBusTimersConst);
        net->_connection->AddObject(new cDBusVdr);
        net->_connection->Connect(FALSE);
        }
     }

  return FALSE;
}

gboolean  cDBusNetwork::do_disconnect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  dsyslog("dbus2vdr: %s: do_disconnect", net->Name());

  if (net->_connection != NULL) {
     delete net->_connection;
     net->_connection = NULL;
     }
  if (net->_address != NULL) {
     delete net->_address;
     net->_address = NULL;
     }

  return FALSE;
}

void cDBusNetwork::on_file_changed(GFileMonitor *monitor, GFile *first, GFile *second, GFileMonitorEvent event, gpointer user_data)
{
  if (user_data == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  if (net->_monitor_context == NULL)
     return;

#define fn(x) ((x) ? g_file_get_basename (x) : "--")
  dsyslog("dbus2vdr: %s: on_file_changed %s", net->Name(), fn(first));
#undef fn

  char *event_name = decode_event(event);
  switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
     {
      dsyslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      GSource *source = g_idle_source_new();
      g_source_set_priority(source, G_PRIORITY_DEFAULT);
      g_source_set_callback(source, do_connect, user_data, NULL);
      g_source_attach(source, net->_monitor_context);
      break;
     }
    case G_FILE_MONITOR_EVENT_DELETED:
     {
      dsyslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      GSource *source = g_idle_source_new();
      g_source_set_priority(source, G_PRIORITY_DEFAULT);
      g_source_set_callback(source, do_disconnect, user_data, NULL);
      g_source_attach(source, net->_monitor_context);
      break;
     }
    default:
     {
      dsyslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      break;
     }
    }
  g_free(event_name);
}

cDBusNetwork::cDBusNetwork(const char *Busname, const char *Filename, GMainContext *Context)
{
  _busname = strdup(Busname);
  _filename = strdup(Filename);
  _context = Context;
  _monitor = NULL;
  _monitor_context = NULL;
  _monitor_loop = NULL;
  _signal_handler_id = 0;
  _address = NULL;
  _connection = NULL;
}

cDBusNetwork::~cDBusNetwork(void)
{
  Stop();

  if (_filename != NULL) {
     g_free(_filename);
     _filename = NULL;
     }
  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }

  dsyslog("dbus2vdr: %s: ~cDBusNetwork", Name());
}

void  cDBusNetwork::Start(void)
{
  if (_monitor_loop != NULL)
     return;

  isyslog("dbus2vdr: %s: starting", Name());
  _monitor_context = g_main_context_new();

  if (g_file_test(_filename, G_FILE_TEST_EXISTS)) {
     GSource *source = g_idle_source_new();
     g_source_set_priority(source, G_PRIORITY_DEFAULT);
     g_source_set_callback(source, do_connect, this, NULL);
     g_source_attach(source, _monitor_context);
     }

  GFile *file = g_file_new_for_path(_filename);
  _monitor = g_file_monitor_file(file, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
  if (_monitor != NULL)
     _signal_handler_id = g_signal_connect(_monitor, "changed", G_CALLBACK(on_file_changed), this);
  g_object_unref(file);

  _monitor_loop = new cDBusMainLoop(_monitor_context);

  isyslog("dbus2vdr: %s: started", Name());
}

void  cDBusNetwork::Stop(void)
{
  if (_monitor_context == NULL)
     return;

  isyslog("dbus2vdr: %s: stopping", Name());

  if (_monitor_loop != NULL) {
     delete _monitor_loop;
     _monitor_loop = NULL;
     }
  if (_connection != NULL) {
     delete _connection;
     _connection = NULL;
     }
  if (_address != NULL) {
     delete _address;
     _address = NULL;
     }
  if (_signal_handler_id > 0) {
     g_signal_handler_disconnect(_monitor, _signal_handler_id);
     _signal_handler_id = 0;
     }
  if (_monitor != NULL) {
     g_object_unref(_monitor);
     _monitor = NULL;
     }
  if (_monitor_context != NULL) {
     g_main_context_unref(_monitor_context);
     _monitor_context = NULL;
     }

  isyslog("dbus2vdr: %s: stopped", Name());
}


cDBusNetworkAddress *cDBusNetworkAddress::LoadFromFile(const char *Filename)
{
  dsyslog("dbus2vdr: loading network address from file %s", Filename);
  FILE *f = fopen(Filename, "r");
  if (f == NULL)
     return NULL;
  cReadLine r;
  char *line = r.Read(f);
  fclose(f);
  if (line == NULL)
     return NULL;
  DBusError err;
  dbus_error_init(&err);
  DBusAddressEntry **addresses = NULL;
  int len = 0;
  if (!dbus_parse_address(line, &addresses, &len, &err)) {
     esyslog("dbus2vdr: errorparsing address %s: %s", line, err.message);
     dbus_error_free(&err);
     return NULL;
     }
  cDBusNetworkAddress *address = NULL;
  for (int i = 0; i < len; i++) {
      if (addresses[i] != NULL) {
         if (strcmp(dbus_address_entry_get_method(addresses[i]), "tcp") == 0) {
            const char *host = dbus_address_entry_get_value(addresses[i], "host");
            const char *port = dbus_address_entry_get_value(addresses[i], "port");
            if ((host != NULL) && (port != NULL) && isnumber(port)) {
               address = new cDBusNetworkAddress(host, atoi(port));
               break;
               }
            }
         }
      }
  dbus_address_entries_free(addresses);
  return address;
}


const char *cDBusNetworkAddress::Address(void)
{
  if (*_address == NULL) {
     char *host = dbus_address_escape_value(*Host);
     _address = cString::sprintf("tcp:host=%s,port=%d", host, Port);
     dbus_free(host);
     }
  return *_address;
}
