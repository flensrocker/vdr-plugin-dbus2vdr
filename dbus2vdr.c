/*
 * dbus2vdr.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <getopt.h>
#include <signal.h>

#include <dbus/dbus.h>
#include <glib-object.h>

#include "common.h"
#include "connection.h"
#include "channel.h"
#include "device.h"
#include "epg.h"
#include "helper.h"
#include "mainloop.h"
#include "network.h"
#include "nulldevice.h"
#include "plugin.h"
#include "osd.h"
#include "recording.h"
#include "remote.h"
#include "sd-daemon.h"
#include "setup.h"
#include "shutdown.h"
#include "skin.h"
#include "status.h"
#include "timer.h"
#include "vdr.h"

#include <vdr/osdbase.h>
#include <vdr/plugin.h>

#include "avahi-helper.h"


static const char *VERSION        = "31";
static const char *DESCRIPTION    = trNOOP("control vdr via D-Bus");
static const char *MAINMENUENTRY  = NULL;

int dbus2vdr_SysLogLevel = 1;
int dbus2vdr_SysLogTarget = LOG_USER;

cString dbus2vdr_Busname = "";
cString dbus2vdr_DBusSessionBusAddress = "";
cString dbus2vdr_UpstartSession = "";

static bool ParseSysLogLevel(const char *Arg)
{
  if (Arg == NULL)
     return false;
  char *arg = strdup(Arg);
  char *p = strchr(arg, '.');
  if (p)
     *p = 0;
  if (isnumber(arg)) {
     int l = atoi(arg);
     if (0 <= l && l <= 4) {
        dbus2vdr_SysLogLevel = l;
        if (!p) {
           free(arg);
           return true;
           }
        if (isnumber(p + 1)) {
           l = atoi(p + 1);
           if (0 <= l && l <= 7) {
              int targets[] = { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7 };
              dbus2vdr_SysLogTarget = targets[l];
              free(arg);
              return true;
              }
           }
        }
     }
  if (p)
     *p = '.';
  esyslog("dbus2vdr: invalid log level: %s", Arg);
  free(arg);
  return false;
}

static cString SetSessionEnv(const char *Name, cString &Store, const char *Option, int &ReplyCode, int *HasBeenSet)
{
  if (Option && *Option) {
     if (setenv(Name, Option, 1) < 0) {
        ReplyCode = 550;
        cString ret = cString::sprintf("can't set %s to %s", Name, Option);
        esyslog("dbus2vdr: %s", *ret);
        return ret;
        }
     Store = Option;
     ReplyCode = 900;
     cString ret = cString::sprintf("set %s to %s", Name, Option);
     isyslog("dbus2vdr: %s", *ret);
     if (HasBeenSet != NULL)
        *HasBeenSet = 1;
     return ret;
     }
  if (unsetenv(Name) < 0) {
     ReplyCode = 550;
     cString ret = cString::sprintf("can't unset %s", Name);
     esyslog("dbus2vdr: %s", *ret);
     return ret;
     }
  Store = "";
  ReplyCode = 900;
  cString ret = cString::sprintf("unset %s ok", Name);
  isyslog("dbus2vdr: %s", *ret);
  if (HasBeenSet != NULL)
     *HasBeenSet = 0;
  return ret;
}

static void AddAllObjects(cDBusConnection *Connection, bool EnableOSD)
{
  Connection->AddObject(new cDBusChannels);
  Connection->AddObject(new cDBusDevices);
  Connection->AddObject(new cDBusEpg);
  if (EnableOSD)
     Connection->AddObject(new cDBusOsdObject);
  cDBusPlugin::AddAllPlugins(Connection);
  Connection->AddObject(new cDBusPluginManager);
  Connection->AddObject(new cDBusRecordings);
  Connection->AddObject(new cDBusRemote);
  Connection->AddObject(new cDBusSetup);
  Connection->AddObject(new cDBusShutdown);
  Connection->AddObject(new cDBusSkin);
  Connection->AddObject(new cDBusStatus(false));
  Connection->AddObject(new cDBusTimers);
  Connection->AddObject(new cDBusVdr);
}

static void AddAllWatchers(cDBusConnection *Connection, cList<cDBusWatcher> *Watchers)
{
  cDBusWatcher *w;
  while ((w = Watchers->First()) != NULL) {
        Connection->AddWatcher(w);
        Watchers->Del(w, false);
        }
}

class cPluginDbus2vdr : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  bool _enable_mainloop;
  bool _enable_osd;
  int  _enable_upstart;
  bool _enable_systemd;
  bool _enable_system;
  bool _enable_session;
  bool _enable_network;
  bool _enable_nulldevice;
  bool _force_nulldevice;
  bool _first_main_thread;

  cDBusMainLoop   *_main_loop;
  cDBusConnection *_system_bus;
  cDBusConnection *_session_bus;
  cDBusNetwork    *_network_bus;

  cList<cDBusWatcher> _system_watchers;
  cList<cDBusWatcher> _session_watchers;

  void StartSessionBus(void);
  void StopSessionBus(void);

public:
  cPluginDbus2vdr(void);
  virtual ~cPluginDbus2vdr();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual bool Service(const char *Id, void *Data = NULL);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

cPluginDbus2vdr::cPluginDbus2vdr(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  _enable_mainloop = true;
  _enable_osd = false;
  _enable_upstart = 0;
  _enable_systemd = false;
  _enable_system = true;
  _enable_session = false;
  _enable_network = false;
  _enable_nulldevice = false;
  _force_nulldevice = false;
  _first_main_thread = true;
  _main_loop = NULL;
  _system_bus = NULL;
  _session_bus = NULL;
  _network_bus = NULL;

  const char *tmp = getenv("DBUS_SESSION_BUS_ADDRESS");
  if (tmp)
     dbus2vdr_DBusSessionBusAddress = tmp;
  tmp = getenv("UPSTART_SESSION");
  if (tmp)
     dbus2vdr_UpstartSession = tmp;
#if (GLIB_MAJOR_VERSION < 2) || ((GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION < 36))
  g_type_init();
#endif
}

cPluginDbus2vdr::~cPluginDbus2vdr()
{
  // Clean up after yourself!
}

void cPluginDbus2vdr::StartSessionBus(void)
{
  if (_session_bus != NULL)
     return;

  if (_enable_session && *dbus2vdr_DBusSessionBusAddress && !isempty(*dbus2vdr_DBusSessionBusAddress)) {
     gchar *session_address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, NULL, NULL);
     if (session_address != NULL) {
        _session_bus = new cDBusConnection(*dbus2vdr_Busname, "Session", session_address, NULL);
        AddAllObjects(_session_bus, _enable_osd);
        AddAllWatchers(_session_bus, &_session_watchers);
        _session_bus->Connect(FALSE);
        g_free(session_address);
        }
     }
}

void cPluginDbus2vdr::StopSessionBus(void)
{
  if (_session_bus != NULL) {
     delete _session_bus;
     _session_bus = NULL;
     }
}

const char *cPluginDbus2vdr::CommandLineHelp(void)
{
  return "  --shutdown-hooks=/path/to/dir/with/shutdown-hooks\n"
         "    directory with shutdown-hooks to be called by ConfirmShutdown\n"
         "    usually it's /usr/share/vdr/shutdown-hooks\n"
         "  --shutdown-hooks-wrapper=/path/to/shutdown-hooks-wrapper\n"
         "    path to a program that will call the shutdown-hooks with suid\n"
         "  --osd\n"
         "    creates an OSD provider which will save the OSD as PNG files\n"
         "  --systemd\n"
         "    use sd_notify to notify systemd\n"
         "  --upstart\n"
         "    enable Upstart started/stopped events\n"
         "  --session\n"
         "    connect to session D-Bus daemon\n"
         "  --no-system\n"
         "    don't connect to system D-Bus daemon\n"
         "  --no-mainloop\n"
         "    don't start GMainLoop (don't use this option if you don't understand)\n"
         "  --network\n"
         "    enable network support for peer2peer communication\n"
         "    a local dbus-daemon has to be started manually\n"
         "    it has to store its address at $PLUGINDIR/network-address.conf\n"
         "  --nulldevice[=force]\n"
         "    create a primary device which does nothing\n"
         "    useful to suspend in- and output\n"
         "    may force vdr to set this device as primary device on startup\n"
         "  --log=n\n"
         "    set plugin's loglevel\n";
}

bool cPluginDbus2vdr::ProcessArgs(int argc, char *argv[])
{
  static struct option options[] =
  {
    {"shutdown-hooks", required_argument, 0, 's'},
    {"shutdown-hooks-wrapper", required_argument, 0, 'w'},
    {"osd", no_argument, 0, 'o'},
    {"upstart", no_argument, 0, 'u'},
    {"session", no_argument, 0, 's' | 0x100},
    {"no-system", no_argument, 0, 's' | 0x200},
    {"systemd", no_argument, 0, 's' | 0x400},
    {"network", no_argument, 0, 'n'},
    {"no-mainloop", no_argument, 0, 'n' | 0x100},
    {"nulldevice", optional_argument, 0, 'd'},
    {"log", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };

  while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "d:l:nop:s:uw:", options, &option_index);
        if (c == -1)
           break;
        switch (c) {
          case 'l':
           {
             ParseSysLogLevel(optarg);
             break;
           }
          case 'o':
           {
             _enable_osd = true;
             isyslog("dbus2vdr: enable osd");
             break;
           }
          case 's':
           {
             if (optarg != NULL) {
                isyslog("dbus2vdr: use shutdown-hooks in %s", optarg);
                cDBusShutdown::SetShutdownHooksDir(optarg);
                }
             break;
           }
          case 's' | 0x100:
           {
             _enable_session = true;
             isyslog("dbus2vdr: enable session-bus");
             break;
           }
          case 's' | 0x200:
           {
             _enable_system = false;
             isyslog("dbus2vdr: disable system-bus support");
             break;
           }
          case 's' | 0x400:
           {
             _enable_systemd = true;
             isyslog("dbus2vdr: enable systemd notify support");
             break;
           }
          case 'u':
           {
             isyslog("dbus2vdr: enable Upstart support");
             _enable_upstart = 1;
             break;
           }
          case 'w':
           {
             if (optarg != NULL) {
                isyslog("dbus2vdr: use shutdown-hooks-wrapper %s", optarg);
                cDBusShutdown::SetShutdownHooksWrapper(optarg);
                }
             break;
           }
          case 'n':
           {
             _enable_network = true;
             isyslog("dbus2vdr: enable network support");
             break;
           }
          case 'n' | 0x100:
           {
             _enable_mainloop = false;
             isyslog("dbus2vdr: disable mainloop");
             break;
           }
          case 'd':
           {
             _enable_nulldevice = true;
             if (optarg)
                _force_nulldevice = true;
             isyslog("dbus2vdr: enable %snulldevice", _force_nulldevice ? "and force " : "");
             break;
           }
          }
        }
  return true;
}

bool cPluginDbus2vdr::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  if (!dbus_threads_init_default())
     esyslog("dbus2vdr: dbus_threads_init_default returns an error - not good!");
  cDBusShutdown::StartupTime = time(NULL);
#if VDRVERSNUM < 10704
  dbus2vdr_Busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#else
  if (InstanceId > 0)
     dbus2vdr_Busname = cString::sprintf("%s%d", DBUS_VDR_BUSNAME, InstanceId);
  else
     dbus2vdr_Busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#endif
  if (_enable_nulldevice) {
     cNullDevice *nulldev = new cNullDevice();
     cDBusDevices::_nulldeviceIndex = nulldev->CardIndex();
     if (_force_nulldevice)
        cDBusDevices::SetPrimaryDeviceRequest(cDBusDevices::_nulldeviceIndex);
     }
  return true;
}

bool cPluginDbus2vdr::Start(void)
{
  cDBusHelper::SetConfigDirectory(cPlugin::ConfigDirectory("dbus2vdr"));
  // Start any background activities the plugin shall perform.
  if (_enable_mainloop) {
     _main_loop = new cDBusMainLoop(NULL);
     cPluginManager::CallAllServices("dbus2vdr-MainLoopStarted", NULL);
     }

  if (_enable_system) {
     gchar *system_address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
     if (system_address != NULL) {
        _system_bus = new cDBusConnection(*dbus2vdr_Busname, "System", system_address, NULL);
        AddAllObjects(_system_bus, _enable_osd);
        AddAllWatchers(_system_bus, &_system_watchers);
        _system_bus->Connect(TRUE);
        g_free(system_address);
        }
     }

  StartSessionBus();

  if (_enable_network) {
     cString filename = cString::sprintf("%s/network-address.conf", cPlugin::ConfigDirectory(Name()));
     _network_bus = new cDBusNetwork(*dbus2vdr_Busname, *filename, NULL);
     _network_bus->Start();
     
     cDBusNetworkClient::StartClients(NULL);
     }

  // emit status "Start" on the various notification channels
  cDBusVdr::SetStatus(cDBusVdr::statusStart);
  if (_enable_systemd)
     sd_notify(0, "STATUS=Start\n");

  return true;
}

void cPluginDbus2vdr::Stop(void)
{
  cDBusRemote::MainMenuAction = NULL;

  // emit status "Stop" on the various notification channels
  if (_enable_systemd)
     sd_notify(0, "STATUS=Stop\n");
  cDBusVdr::SetStatus(cDBusVdr::statusStop);

  cDBusNetworkClient::StopClients();
  if (_network_bus != NULL) {
     delete _network_bus;
     _network_bus = NULL;
     }
  StopSessionBus();
  if (_system_bus != NULL) {
     delete _system_bus;
     _system_bus = NULL;
     }
  cDBusObject::FreeThreadPool();
  cDBusConnection::FreeThreadPool();
  if (_main_loop != NULL) {
     cPluginManager::CallAllServices("dbus2vdr-MainLoopStopped", NULL);
     delete _main_loop;
     _main_loop = NULL;
     }
}

void cPluginDbus2vdr::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginDbus2vdr::MainThreadHook(void)
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
  if (_first_main_thread) {
     _first_main_thread = false;

     // emit status "Ready" on the various notification channels
     cDBusVdr::SetStatus(cDBusVdr::statusReady);
     if (_enable_systemd)
        sd_notify(0, "READY=1\nSTATUS=Ready\n");
     if (_enable_upstart) {
        isyslog("dbus2vdr: raise SIGSTOP for Upstart");
        raise(SIGSTOP);
        }
     }

  int requestPrimaryDevice = cDBusDevices::RequestedPrimaryDevice(true);
  if (requestPrimaryDevice >= 0) {
     cControl::Shutdown();
     cDevice::SetPrimaryDevice(requestPrimaryDevice + 1);
     }
}

cString cPluginDbus2vdr::Active(void)
{
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginDbus2vdr::WakeupTime(void)
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginDbus2vdr::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return cDBusRemote::MainMenuAction;
}

cMenuSetupPage *cPluginDbus2vdr::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginDbus2vdr::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

bool cPluginDbus2vdr::Service(const char *Id, void *Data)
{
  if (strcmp(Id, "avahi4vdr-event") == 0) {
     isyslog("dbus2vdr: avahi4vdr-event: %s", (const char*)Data);
     cAvahiHelper options((const char*)Data);
     const char *event = options.Get("event");
     const char *browser_id = options.Get("id");

     if ((event != NULL)
      && (browser_id != NULL)
      && (_network_bus != NULL)
      && (cDBusNetworkClient::AvahiBrowserId() != NULL)
      && (strcmp(cDBusNetworkClient::AvahiBrowserId(), browser_id) == 0)) {
        if (strcmp(event, "browser-service-resolved") == 0) {
           const char *name = options.Get("name");
           const char *host = options.Get("host");
           const char *protocol = options.Get("protocol");
           const char *address = options.Get("address");
           const char *port = options.Get("port");
           const char *local = options.Get("local");
           const char *busname = NULL;
           const char *txt = NULL;
           int txt_nr = 0;
           while (true) {
                 txt = options.Get("txt", txt_nr);
                 if (txt == NULL)
                    break;
                 if (strncmp(txt, "busname=", 8) == 0)
                    busname = txt + 8;
                 txt_nr++;
                 }
           if (((local == NULL) || (strcasecmp(local, "true") != 0))
            && (name != NULL)
            && (host != NULL)
            && (protocol != NULL)
            && (strcasecmp(protocol, "ipv4") == 0)
            && (address != NULL)
            && (port != NULL)
            && isnumber(port)
            && (busname != NULL))
              new cDBusNetworkClient(name, host, address, atoi(port), busname);
           }
        else if (strcmp(event, "browser-service-removed") == 0) {
           const char *name = options.Get("name");
           const char *protocol = options.Get("protocol");
           const char *local = options.Get("local");
           if ((local == NULL) || (strcasecmp(local, "true") != 0)
            && (name != NULL)
            && (protocol != NULL)
            && (strcasecmp(protocol, "ipv4") == 0))
              cDBusNetworkClient::RemoveClient(name);
           }
        }
     return true;
     }
  return false;
}

const char **cPluginDbus2vdr::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  return NULL;
}

cString cPluginDbus2vdr::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if (Command == NULL)
     return NULL;

  // Process SVDRP commands this plugin implements
  if (strcmp(Command, "RunsMainLoop") == 0) {
     if (_enable_mainloop) {
        ReplyCode = 900;
        return "dbus2vdr runs the default GMainLoop";
        }
     ReplyCode = 901;
     return "dbus2vdr does not run the default GMainLoop";
     }
  else if (strcmp(Command, "SetSysLogLevel") == 0) {
     if (ParseSysLogLevel(Option)) {
        ReplyCode = 900;
        return cString::sprintf("SysLogLevel set to %s", Option);
        }
     ReplyCode = 501;
     return cString::sprintf("unknown SysLogLevel %s", Option);
     }
  else if (strcmp(Command, "SetDBusSessionBusAddress") == 0) {
     int hasBeenSet = -1;
     cString msg = SetSessionEnv("DBUS_SESSION_BUS_ADDRESS", dbus2vdr_DBusSessionBusAddress, Option, ReplyCode, &hasBeenSet);
     if (hasBeenSet == 1) {
        StopSessionBus(); // if the address changes without previous unset
        StartSessionBus();
        }
     else if (hasBeenSet == 0)
        StopSessionBus();
     return msg;
     }
  else if (strcmp(Command, "SetUpstartSession") == 0) {
     return SetSessionEnv("UPSTART_SESSION", dbus2vdr_UpstartSession, Option, ReplyCode, 0);
     }
  else if (strcmp(Command, "WatchBusname") == 0) {
     cAvahiHelper options(Option);
     const char *caller = options.Get("caller");   // the name of the calling plugin
     const char *watch = options.Get("watch");     // the name to watch
     const char *busname = options.Get("busname"); // system or session

     if (caller == NULL) {
        ReplyCode = 501;
        return "error=missing caller";
        }
     if (watch == NULL) {
        ReplyCode = 501;
        return "error=missing watch";
        }
     if (busname == NULL) {
        ReplyCode = 501;
        return "error=missing busname";
        }

     if (strcmp(busname, "system") == 0) {
        cDBusWatcher *watcher = new cDBusWatcher(caller, watch);
        guint id = watcher->Id();
        if (_system_bus == NULL)
           _system_watchers.Add(watcher);
        else if (_system_bus->GetConnectStatus() < 3)
           _system_bus->AddWatcher(watcher);
        else
           _system_bus->Watch(watcher);
        ReplyCode = 900;
        return cString::sprintf("id=%d", id);
        }
     else if (strcmp(busname, "session") == 0) {
        cDBusWatcher *watcher = new cDBusWatcher(caller, watch);
        guint id = watcher->Id();
        if (_session_bus == NULL)
           _session_watchers.Add(watcher);
        else if (_session_bus->GetConnectStatus() < 3)
           _session_bus->AddWatcher(watcher);
        else
           _session_bus->Watch(watcher);
        ReplyCode = 900;
        return cString::sprintf("id=%d", id);
        }

     ReplyCode = 501;
     return "error=unknown busname";
     }
  else if (strcmp(Command, "UnwatchBusname") == 0) {
     cAvahiHelper options(Option);
     const char *id_str = options.Get("id");           // id returned by WatchBusname
     const char *busname = options.Get("busname"); // system or session

     if (id_str == NULL) {
        ReplyCode = 501;
        return "error=missing id";
        }
     else if (!isnumber(id_str)) {
        ReplyCode = 501;
        return "error=id is not a number";
        }
     if (busname == NULL) {
        ReplyCode = 501;
        return "error=missing busname";
        }

     cDBusConnection *conn = NULL;
     if (strcmp(busname, "system") == 0)
        conn = _system_bus;
     else if (strcmp(busname, "session") == 0)
        conn = _session_bus;
     if (conn == NULL) {
        ReplyCode = 501;
        return "error=unknown busname";
        }

     ReplyCode = 900;
     guint id = strtol(id_str, NULL, 10);
     conn->Unwatch(id);
     return cString::sprintf("message=unwatch id %d", id);
     }
  return NULL;
}

VDRPLUGINCREATOR(cPluginDbus2vdr); // Don't touch this!
