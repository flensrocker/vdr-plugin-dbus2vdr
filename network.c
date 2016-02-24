#include "network.h"

#include "common.h"
#include "recording.h"
#include "status.h"
#include "timer.h"
#include "vdr.h"

#include <dbus/dbus.h>
#include <vdr/tools.h>

#include "avahi-helper.h"


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
    default:
      strcat(fmt, "UNKNOWN");
      caret += strlen("UNKNOWN");
      fmt[caret] = ':';
      break;
  }
#undef dc

  return fmt;
}

gboolean  cDBusNetwork::do_connect(gpointer user_data)
{
  if (user_data == NULL)
     return FALSE;

  cDBusNetwork *net = (cDBusNetwork*)user_data;
  d4syslog("dbus2vdr: %s: do_connect", net->Name());

  if (g_file_test(net->_filename, G_FILE_TEST_EXISTS)) {
     if (net->_address != NULL)
        delete net->_address;
     net->_address = cDBusNetworkAddress::LoadFromFile(net->_filename);
     if (net->_address != NULL) {
        net->_avahi_name = strescape(*cString::sprintf("dbus2vdr on %s", *net->_address->Host), "\\,");
        if (net->_connection != NULL)
           delete net->_connection;
        net->_connection = new cDBusConnection(net->_busname, net->Name(), net->_address->Address(), net->_context);
        net->_connection->SetNameCallbacks(on_name_acquired, on_name_lost, net);
        net->_connection->AddObject(new cDBusRecordingsConst);
        net->_connection->AddObject(new cDBusStatus(true));
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
  d4syslog("dbus2vdr: %s: do_disconnect", net->Name());

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
  d4syslog("dbus2vdr: %s: on_file_changed %s", net->Name(), fn(first));
#undef fn

  char *event_name = decode_event(event);
  switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
     {
      d4syslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      GSource *source = g_idle_source_new();
      g_source_set_priority(source, G_PRIORITY_DEFAULT);
      g_source_set_callback(source, do_connect, user_data, NULL);
      g_source_attach(source, net->_monitor_context);
      break;
     }
    case G_FILE_MONITOR_EVENT_DELETED:
     {
      d4syslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      GSource *source = g_idle_source_new();
      g_source_set_priority(source, G_PRIORITY_DEFAULT);
      g_source_set_callback(source, do_disconnect, user_data, NULL);
      g_source_attach(source, net->_monitor_context);
      break;
     }
    default:
     {
      d4syslog("dbus2vdr: %s: on_file_changed got event %s", net->Name(), event_name);
      break;
     }
    }
  g_free(event_name);
}

void cDBusNetwork::on_name_acquired(cDBusConnection *Connection, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)UserData;
  d4syslog("dbus2vdr: %s: on_name_acquired", net->Name());
  if ((net->_address != NULL) && (net->_avahi4vdr != NULL)) {
     int replyCode = 0;
     cString parameter = cString::sprintf("caller=dbus2vdr,name=%s,type=_dbus._tcp,port=%d,subtype=_vdr_dbus2vdr._sub._dbus._tcp,txt=busname=%s", *net->_avahi_name, net->_address->Port, net->_busname);
     net->_avahi_id = net->_avahi4vdr->SVDRPCommand("CreateService", *parameter, replyCode);
     d4syslog("dbus2vdr: %s: avahi service created (id %s)", net->Name(), *net->_avahi_id);
     }
}

void cDBusNetwork::on_name_lost(cDBusConnection *Connection, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetwork *net = (cDBusNetwork*)UserData;
  d4syslog("dbus2vdr: %s: on_name_lost", net->Name());
  if ((net->_avahi4vdr != NULL) && (*net->_avahi_id != NULL)) {
     int replyCode = 0;
     net->_avahi4vdr->SVDRPCommand("DeleteService", *net->_avahi_id, replyCode);
     d4syslog("dbus2vdr: %s: avahi service deleted (id %s)", net->Name(), *net->_avahi_id);
     net->_avahi_id = NULL;
     }
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
  _avahi4vdr = cPluginManager::GetPlugin("avahi4vdr");
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

  d4syslog("dbus2vdr: %s: ~cDBusNetwork", Name());
}

void  cDBusNetwork::Start(void)
{
  if (_monitor_loop != NULL)
     return;

  isyslog("dbus2vdr: %s: starting", Name());
  _monitor_context = g_main_context_new();
  _monitor_loop = new cDBusMainLoop(_monitor_context);

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

  isyslog("dbus2vdr: %s: started", Name());
}

void  cDBusNetwork::Stop(void)
{
  if (_monitor_context == NULL)
     return;

  isyslog("dbus2vdr: %s: stopping", Name());

  if (_signal_handler_id > 0) {
     g_signal_handler_disconnect(_monitor, _signal_handler_id);
     _signal_handler_id = 0;
     }
  if (_monitor != NULL) {
     g_object_unref(_monitor);
     _monitor = NULL;
     }
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
  if (_monitor_context != NULL) {
     g_main_context_unref(_monitor_context);
     _monitor_context = NULL;
     }

  isyslog("dbus2vdr: %s: stopped", Name());
}


cDBusNetworkAddress *cDBusNetworkAddress::LoadFromFile(const char *Filename)
{
  d4syslog("dbus2vdr: loading network address from file %s", Filename);
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


GMainContext *cDBusNetworkClient::_context = NULL;
cList<cDBusNetworkClient> cDBusNetworkClient::_clients;
cPlugin      *cDBusNetworkClient::_avahi4vdr = NULL;
cString       cDBusNetworkClient::_avahi_browser_id;

void  cDBusNetworkClient::OnConnect(cDBusConnection *Connection, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetworkClient *client = (cDBusNetworkClient*)UserData;
  d4syslog("dbus2vdr: NetworkClient: OnConnect %s", client->Name());
  if (client->_signal_timer_change == NULL) {
     client->_signal_timer_change = new cDBusSignal(client->_busname, "/Status", DBUS_VDR_STATUS_INTERFACE, "TimerChange", NULL, OnTimerChange, UserData);
     client->_connection->Subscribe(client->_signal_timer_change);
     }
}

void  cDBusNetworkClient::OnDisconnect(cDBusConnection *Connection, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetworkClient *client = (cDBusNetworkClient*)UserData;
  d4syslog("dbus2vdr: NetworkClient: OnDisconnect %s", client->Name());
  if ((client->_signal_timer_change != NULL) && (client->_connection != NULL)) {
     client->_connection->Unsubscribe(client->_signal_timer_change);
     client->_signal_timer_change = NULL;
     }
}

void  cDBusNetworkClient::OnTimerChange(const gchar *SenderName, const gchar *ObjectPath, const gchar *Interface, const gchar *Signal, GVariant *Parameters, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetworkClient *client = (cDBusNetworkClient*)UserData;
  d4syslog("dbus2vdr: NetworkClient: timer changed on %s", client->Name());
  if (client->_connection != NULL)
     client->_connection->CallMethod(new cDBusMethodCall(client->_busname, "/Timers", DBUS_VDR_TIMER_INTERFACE, "List", NULL, OnTimerList, client));
}

void  cDBusNetworkClient::OnTimerList(GVariant *Reply, gpointer UserData)
{
  if (UserData == NULL)
     return;

  cDBusNetworkClient *client = (cDBusNetworkClient*)UserData;
  d4syslog("dbus2vdr: NetworkClient: get timer from %s", client->Name());
}

cDBusNetworkClient::cDBusNetworkClient(const char *Name, const char *Host, const char *Address, int Port, const char *Busname)
{
  _name = g_strdup(Name);
  _host = g_strdup(Host);
  _address = g_strdup(Address);
  _port = Port;
  _busname = g_strdup(Busname);
  _signal_timer_change = NULL;

  cDBusNetworkAddress address(_address, _port);
  _connection = new cDBusConnection(NULL, Name, address.Address(), _context);
  _connection->SetConnectCallbacks(OnConnect, OnDisconnect, this);
  _connection->Connect(FALSE);

  _clients.Add(this);
  isyslog("dbus2vdr: new network client for '%s' ['%s'] with address '%s' on port %d with busname %s", _name, _host, _address, _port, _busname);
}

cDBusNetworkClient::~cDBusNetworkClient(void)
{
  isyslog("dbus2vdr: remove network client '%s'", _name);
  if (_busname != NULL) {
     g_free(_busname);
     _busname = NULL;
     }
  if (_name != NULL) {
     g_free(_name);
     _name = NULL;
     }
  if (_host != NULL) {
     g_free(_host);
     _host = NULL;
     }
  if (_address != NULL) {
     g_free(_address);
     _address = NULL;
     }
}

int  cDBusNetworkClient::Compare(const cListObject &ListObject) const
{
  cDBusNetworkClient &other = (cDBusNetworkClient&)ListObject;
  return g_strcmp0(_name, other._name);
}

bool  cDBusNetworkClient::StartClients(GMainContext *Context)
{
  _avahi4vdr = cPluginManager::GetPlugin("avahi4vdr");
  if (_avahi4vdr == NULL)
     return false;

  if (*_avahi_browser_id == NULL) {
     _context = Context;
     int replyCode = 0;
     cString reply = _avahi4vdr->SVDRPCommand("CreateBrowser", "caller=dbus2vdr,protocol=IPv4,type=_vdr_dbus2vdr._sub._dbus._tcp", replyCode);
     cAvahiHelper options(reply);
     _avahi_browser_id = options.Get("id");
     }
  return true;
}

void  cDBusNetworkClient::StopClients(void)
{
  if (_avahi4vdr != NULL) {
     if ((*_avahi_browser_id != NULL) && (strlen(*_avahi_browser_id) > 0)) {
        int replyCode = 0;
        _avahi4vdr->SVDRPCommand("DeleteBrowser", *_avahi_browser_id, replyCode);
        }
     _avahi_browser_id = NULL;
     }

  cDBusNetworkClient *c;
  while ((c = _clients.First()) != NULL)
        _clients.Del(c, true);
}

void  cDBusNetworkClient::RemoveClient(const char *Name)
{
  for (cDBusNetworkClient *c = _clients.First(); c; c = _clients.Next(c)) {
      if (g_strcmp0(c->Name(), Name) == 0) {
         _clients.Del(c, true);
         break;
         }
      }
}
