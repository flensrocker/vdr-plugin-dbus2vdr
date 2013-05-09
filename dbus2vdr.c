/*
 * dbus2vdr.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <getopt.h>
#include <signal.h>

#include <glib-object.h>

#include "common.h"
#include "connection.h"
#include "channel.h"
#include "epg.h"
#include "helper.h"
#include "plugin.h"
#include "monitor.h"
#include "osd.h"
#include "recording.h"
#include "remote.h"
#include "setup.h"
#include "shutdown.h"
#include "skin.h"
#include "status.h"
#include "timer.h"
#include "vdr.h"

#include <vdr/osdbase.h>
#include <vdr/plugin.h>

static const char *VERSION        = "10-gdbus";
static const char *DESCRIPTION    = trNOOP("control vdr via D-Bus");
static const char *MAINMENUENTRY  = NULL;

class cPluginDbus2vdr : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  bool enable_osd;
  int  send_upstart_signals;
  bool enable_session;
  bool enable_network;
  bool first_main_thread;

  cDBusConnection *system_bus;
  cDBusConnection *session_bus;
  cDBusConnection *network_bus;

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
  enable_osd = false;
  send_upstart_signals = -1;
  enable_session = false;
  enable_network = false;
  first_main_thread = true;
  g_type_init();
  system_bus = NULL;
  session_bus = NULL;
  network_bus = NULL;
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
         "  --poll-timeout\n"
         "    timeout in milliseconds for dbus_connection_read_write_dispatch\n"
         "  --session\n"
         "    connect to session D-Bus daemon\n"
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
    {"poll-timeout", required_argument, 0, 'p'},
    {"session", no_argument, 0, 's' | 0x100},
    {"network", no_argument, 0, 'n'},
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
                cDBusShutdownActions::SetShutdownHooksDir(optarg);
                }
             break;
           }
          case 's' | 0x100:
           {
             enable_session = true;
             isyslog("dbus2vdr: enable session support");
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
                cDBusShutdownActions::SetShutdownHooksWrapper(optarg);
                }
             break;
           }
          case 'p':
           {
             if ((optarg != NULL) && isnumber(optarg)) {
                isyslog("dbus2vdr: use poll-timeout %s", optarg);
                cDBusMonitor::PollTimeoutMs = strtol(optarg, NULL, 10);
                }
             break;
           }
          case 'n':
           {
             enable_network = true;
             isyslog("dbus2vdr: enable network support");
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
  cDBusDispatcherShutdown::StartupTime = time(NULL);
  return true;
}

bool cPluginDbus2vdr::Start(void)
{
  cDBusHelper::SetConfigDirectory(cPlugin::ConfigDirectory("dbus2vdr"));
  // Start any background activities the plugin shall perform.

  cString busname;
#if VDRVERSNUM < 10704
  busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#else
  if (InstanceId > 0)
     busname = cString::sprintf("%s%d", DBUS_VDR_BUSNAME, InstanceId);
  else
     busname = cString::sprintf("%s", DBUS_VDR_BUSNAME);
#endif
  if (enable_session) {
     session_bus = new cDBusConnection(*busname, G_BUS_TYPE_SESSION);
     session_bus->AddObject(new cDBusChannels);
     session_bus->AddObject(new cDBusEpg);
     cDBusPlugin::AddAllPlugins(session_bus);
     session_bus->AddObject(new cDBusPluginManager);
     session_bus->AddObject(new cDBusRecordings);
     session_bus->AddObject(new cDBusRemote);
     session_bus->AddObject(new cDBusSetup);
     session_bus->AddObject(new cDBusShutdown);
     session_bus->AddObject(new cDBusSkin);
     session_bus->AddObject(new cDBusStatus);
     session_bus->AddObject(new cDBusTimers);
     session_bus->AddObject(new cDBusVdr);
     session_bus->Start();
     }
  
  new cDBusDispatcherEpg;
  new cDBusDispatcherOsd;
  new cDBusDispatcherPlugin;
  new cDBusDispatcherRecording;
  new cDBusDispatcherRemote;
  new cDBusDispatcherSetup;
  new cDBusDispatcherShutdown;
  new cDBusDispatcherSkin;
  new cDBusDispatcherTimer;
  new cDBusDispatcherVdr;
  if (enable_network) {
     new cDBusDispatcherRecordingConst(busNetwork);
     new cDBusDispatcherTimerConst(busNetwork);
     }
  cDBusMonitor::StartMonitor(enable_network);
  
  if (enable_osd)
     new cDBusOsdProvider();

  if (cDBusVdr::SetStatus(cDBusVdr::statusStart))
     cDBusDispatcherVdr::SendStatus(cDBusVdr::statusStart);

  return true;
}

void cPluginDbus2vdr::Stop(void)
{
  cDBusRemote::MainMenuAction = NULL;

  // Stop any background activities the plugin is performing.
  if (send_upstart_signals == 1) {
     send_upstart_signals++;
     cDBusMonitor::SendUpstartSignal("stopped");
     }

  if (cDBusVdr::SetStatus(cDBusVdr::statusStop))
     cDBusDispatcherVdr::SendStatus(cDBusVdr::statusStop);

  cDBusMonitor::StopUpstartSender();
  cDBusMonitor::StopMonitor();
  cDBusMessageDispatcher::Shutdown();

  if (network_bus != NULL) {
     delete network_bus;
     network_bus = NULL;
     }
  if (session_bus != NULL) {
     delete session_bus;
     session_bus = NULL;
     }
  if (system_bus != NULL) {
     delete system_bus;
     system_bus = NULL;
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

     if (cDBusVdr::SetStatus(cDBusVdr::statusReady))
        cDBusDispatcherVdr::SendStatus(cDBusVdr::statusReady);

     if (send_upstart_signals == 0) {
        send_upstart_signals++;
        isyslog("dbus2vdr: raise SIGSTOP for Upstart");
        raise(SIGSTOP);
        cDBusMonitor::SendUpstartSignal("started");
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
