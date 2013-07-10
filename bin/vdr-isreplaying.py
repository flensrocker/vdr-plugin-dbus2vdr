#!/usr/bin/env python

import dbus
import sys

bus = dbus.SystemBus()
Status = bus.get_object('de.tvdr.vdr', '/Status')
(name, filename, on) = Status.IsReplaying(dbus_interface = 'de.tvdr.vdr.status')
if on:
  print "Name: {0}".format(name)
  print "Filename: {0}".format(filename)
  sys.exit(1)
else:
  sys.exit(0)
