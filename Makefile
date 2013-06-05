#
# Makefile for a Video Disk Recorder plugin
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN = dbus2vdr

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell pkg-config --variable=$(1) vdr || pkg-config --variable=$(1) ../../../vdr.pc))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
CONFDIR = $(call PKGCFG,configdir)
PLGCONFDIR = $(CONFDIR)/plugins/$(PLUGIN)
PLGCFG = $(call PKGCFG,plgcfg)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags) $(shell pkg-config --cflags dbus-1 glib-2.0 gio-2.0) $(shell libpng-config --cflags)
export LDADD    += $(shell pkg-config --libs dbus-1 glib-2.0 gio-2.0) $(shell libpng-config --ldflags)

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Includes and Defines (add further entries here):

INCLUDES +=

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

### The object files (add further files here):

OBJS = $(PLUGIN).o channel.o connection.o epg.o helper.o mainloop.o network.o object.o osd.o plugin.o recording.o remote.o sd-daemon.o setup.o shutdown.o skin.o status.o timer.o upstart.o vdr.o
SWOBJS = libvdr-exitpipe.o libvdr-i18n.o libvdr-thread.o libvdr-tools.o shutdown-wrapper.o

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

shutdown-wrapper: $(SWOBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SWOBJS) -ljpeg -lrt -pthread -o $@

$(SOFILE): $(OBJS) shutdown-wrapper
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LDADD) -o $@

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install-cfg: shutdown-wrapper
	install -D etc/dbus2vdr.conf $(DESTDIR)/etc/init/dbus2vdr.conf
	install -D etc/de.tvdr.vdr.conf $(DESTDIR)/etc/dbus-1/system.d/de.tvdr.vdr.conf
	install -D etc/network.conf $(DESTDIR)$(PLGCONFDIR)/network.conf
	install -D bin/vdr-dbus-send.sh $(DESTDIR)/usr/share/vdr-plugin-dbus2vdr/vdr-dbus-send.sh
	install -D shutdown-wrapper $(DESTDIR)/usr/share/vdr-plugin-dbus2vdr/shutdown-wrapper

install: install-lib install-i18n install-cfg

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(SWOBJS) shutdown-wrapper $(DEPFILE) *.so *.tgz core* *~
