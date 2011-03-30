#!/usr/bin/env python

from datetime import datetime
import dbus
bus = dbus.SystemBus()
Timers = bus.get_object('de.tvdr.vdr', '/Timers')
(replycode, timerid, seconds, start, stop, title) = Timers.Next(dbus_interface = 'de.tvdr.vdr.timer')
if replycode == 250:
  print "Next timer \"{0}\" starts in {1} seconds at {2}".format(title, seconds, datetime.fromtimestamp(start))
else:
  print "No active timer"
