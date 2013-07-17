#!/usr/bin/env python

import dbus
import sys

instance = ''
if len(sys.argv) > 1:
  instance = sys.argv[1]

bus = dbus.SystemBus()
Remote = bus.get_object('de.tvdr.vdr{0}'.format(instance), '/Remote')
(code, message) = Remote.SwitchChannel('', dbus_interface = 'de.tvdr.vdr.remote')
pos = message.find(' ')
if pos >= 0:
  message = message[:pos]
print message
