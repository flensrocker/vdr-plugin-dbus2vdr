#!/usr/bin/env python
# -*- coding: utf-8 -*-

import dbus
bus = dbus.SystemBus()
Timers = bus.get_object('de.tvdr.vdr', '/Timers')
vdrtimers = Timers.List(dbus_interface = 'de.tvdr.vdr.timer')

titles = []
for t in vdrtimers:
  fields = t.split(':')
  if len(fields) >= 8 and fields[0].isnumeric() and int(fields[0]) >= 8:
    titles.append(fields[7])

if len(titles) == 0:
  print u"no active recordings"
elif len(titles) > 0:
  print u"number of active recordings: {0}".format(len(titles))
  for title in titles:
    print title
