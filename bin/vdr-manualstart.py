#!/usr/bin/env python

import dbus
import sys

bus = dbus.SystemBus()
Shutdown = bus.get_object('de.tvdr.vdr', '/Shutdown')
manual = Shutdown.ManualStart(dbus_interface = 'de.tvdr.vdr.shutdown')
if manual:
  print "manual vdr start"
  sys.exit(1)
else:
  print "timer vdr start"
  sys.exit(0)
