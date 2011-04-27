/*
 * dbus2vdr.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include "epg.h"
#include "plugin.h"
#include "monitor.h"
#include "remote.h"
#include "timer.h"

#include <vdr/plugin.h>

static const char *VERSION        = "0.0.2f";
static const char *DESCRIPTION    = "expose methods for controlling vdr via DBus";
static const char *MAINMENUENTRY  = NULL;

class cPluginDbus2vdr : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginDbus2vdr(void);
  virtual ~cPluginDbus2vdr();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
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
}

cPluginDbus2vdr::~cPluginDbus2vdr()
{
  // Clean up after yourself!
}

const char *cPluginDbus2vdr::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginDbus2vdr::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginDbus2vdr::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  return true;
}

bool cPluginDbus2vdr::Start(void)
{
  // Start any background activities the plugin shall perform.
  new cDBusDispatcherEPG;
  new cDBusDispatcherPlugin;
  new cDBusDispatcherRemote;
  new cDBusDispatcherTimer;
  cDBusMonitor::StartMonitor();
  return true;
}

void cPluginDbus2vdr::Stop(void)
{
  // Stop any background activities the plugin is performing.
  cDBusMonitor::StopMonitor();
  cDBusMessageDispatcher::Shutdown();
}

void cPluginDbus2vdr::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginDbus2vdr::MainThreadHook(void)
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
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
  return NULL;
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
