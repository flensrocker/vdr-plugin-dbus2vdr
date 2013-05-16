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
#include "epg.h"
#include "helper.h"
#include "network.h"
#include "plugin.h"
#include "osd.h"
#include "recording.h"
#include "remote.h"
#include "setup.h"
#include "shutdown.h"
#include "skin.h"
#include "status.h"
#include "timer.h"
#include "upstart.h"
#include "vdr.h"

#include <vdr/osdbase.h>
#include <vdr/plugin.h>

static const char *VERSION        = "10-gdbus";
static const char *DESCRIPTION    = trNOOP("control vdr via D-Bus");
static const char *MAINMENUENTRY  = NULL;

class cPluginDbus2vdr : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  bool enable_mainloop;
  bool enable_osd;
  int  send_upstart_signals;
  bool enable_system;
  bool enable_session;
  bool enable_network;
  bool first_main_thread;

  cDBusMainLoop   *main_loop;
  cDBusConnection *system_bus;
  cDBusConnection *session_bus;
  cDBusNetwork    *network_handler;

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
  enable_mainloop = true;
  enable_osd = false;
  send_upstart_signals = -1;
  enable_system = true;
  enable_session = false;
  enable_network = false;
  first_main_thread = true;
  g_type_init();
  main_loop = NULL;
  system_bus = NULL;
  session_bus = NULL;
  network_handler = NULL;
}

cPluginDbus2vdr::~cPluginDbus2vdr()
{
  // Clean up after yourself!
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
         "    it has to store its address at $PLUGINDIR/network-address.conf\n";
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
    {"network", no_argument, 0, 'n'},
    {"no-mainloop", no_argument, 0, 'n' | 0x100},
    {0, 0, 0, 0}
  };

  while (true) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "nop:s:uw:", options, &option_index);
        if (c == -1)
           break;
        switch (c) {
          case 'o':
           {
             enable_osd = true;
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
             enable_session = true;
             isyslog("dbus2vdr: enable session-bus");
             break;
           }
          case 's' | 0x200:
           {
             enable_system = false;
             isyslog("dbus2vdr: disable system-bus support");
             break;
           }
          case 'u':
           {
             isyslog("dbus2vdr: enable Upstart support");
             send_upstart_signals = 0;
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
             enable_network = true;
             isyslog("dbus2vdr: enable network support");
             break;
           }
          case 'n' | 0x100:
           {
             enable_mainloop = false;
             isyslog("dbus2vdr: disable mainloop");
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
  return true;
}

static void AddAllObjects(cDBusConnection *Connection, bool EnableOSD)
{
  Connection->AddObject(new cDBusChannels);
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
  Connection->AddObject(new cDBusStatus);
  Connection->AddObject(new cDBusTimers);
  Connection->AddObject(new cDBusVdr);
}

bool cPluginDbus2vdr::Start(void)
{
  cDBusHelper::SetConfigDirectory(cPlugin::ConfigDirectory("dbus2vdr"));
  // Start any background activities the plugin shall perform.
  if (enable_mainloop)
     main_loop = new cDBusMainLoop(NULL);

  cString busname;
#if VDRVERSNUM < 10704
  busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#else
  if (InstanceId > 0)
     busname = cString::sprintf("%s%d", DBUS_VDR_BUSNAME, InstanceId);
  else
     busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#endif
  if (enable_system) {
     system_bus = new cDBusConnection(*busname, G_BUS_TYPE_SYSTEM, NULL);
     AddAllObjects(system_bus, enable_osd);
     system_bus->Connect(TRUE);
     }

  if (enable_session) {
     session_bus = new cDBusConnection(*busname, G_BUS_TYPE_SESSION, NULL);
     AddAllObjects(session_bus, enable_osd);
     session_bus->Connect(TRUE);
     }

  // TODO build handler for dbus2vdr-daemon connection
  if (enable_network) {
     //static const char *address = "unix:abstract=/de/tvdr/vdr/plugins/dbus2vdr";
     //static const char *address = "unix:path=/tmp/dbus2vdr";
     static const char *address = "tcp:host=hdvdr,port=36356";
     network_handler = new cDBusNetwork(address, NULL);
  //   cString filename = cString::sprintf("%s/network-address.conf", cPlugin::ConfigDirectory("dbus2vdr"));
  //   network_bus = new cDBusConnection(*busname, *filename);
  //   network_bus->AddObject(new cDBusRecordingsConst);
  //   network_bus->AddObject(new cDBusTimersConst);
     network_handler->Start();
     }
  
  cDBusVdr::SetStatus(cDBusVdr::statusStart);

  return true;
}

void cPluginDbus2vdr::Stop(void)
{
  cDBusRemote::MainMenuAction = NULL;

  // Stop any background activities the plugin is performing.
  if (send_upstart_signals == 1) {
     send_upstart_signals++;
     cDBusUpstart::EmitPluginEvent(system_bus, "stopped");
     }

  cDBusVdr::SetStatus(cDBusVdr::statusStop);

  if (network_handler != NULL) {
     delete network_handler;
     network_handler = NULL;
     }
  if (session_bus != NULL) {
     delete session_bus;
     session_bus = NULL;
     }
  if (system_bus != NULL) {
     delete system_bus;
     system_bus = NULL;
     }
  if (main_loop != NULL) {
     delete main_loop;
     main_loop = NULL;
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
  if (first_main_thread) {
     first_main_thread = false;

     cDBusVdr::SetStatus(cDBusVdr::statusReady);

     if (send_upstart_signals == 0) {
        send_upstart_signals++;
        isyslog("dbus2vdr: raise SIGSTOP for Upstart");
        raise(SIGSTOP);
        cDBusUpstart::EmitPluginEvent(system_bus, "started");
        }
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
     return true;
     }
  // Handle custom service requests from other plugins
  return false;
}

const char **cPluginDbus2vdr::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  return NULL;
}

cString cPluginDbus2vdr::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  // Process SVDRP commands this plugin implements
  return NULL;
}

VDRPLUGINCREATOR(cPluginDbus2vdr); // Don't touch this!
