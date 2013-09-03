# Copyright (C) 2008 Equinox Software, Inc.
# Kevin Beswick <kevinbeswick00@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.


# Declare some directory variables

export OPENSRF	= opensrf
export BINDIR	= @bindir@
export LIBDIR	= @libdir@
export OSRF_JAVA_DEPSDIR = @OSRF_JAVA_DEPSDIR@
etcdir = $(ETCDIR)

DISTCLEANFILES = Makefile.in Makefile

AM_LDFLAGS = $(DEF_LDFLAGS)
AM_CFLAGS = $(DEF_CFLAGS)

if BUILDPYTHON
MAYBE_PY = python
endif 

if BUILDJAVA
MAYBE_JA = java
endif

if INSTALLJAVASCRIPT
MAYBE_JAVASCRIPT = javascript
jsdir = $(prefix)/lib/javascript
js_SCRIPTS = javascript/DojoSRF.js javascript/JSON_v1.js javascript/md5.js javascript/opensrf.js javascript/opensrf_xhr.js javascript/opensrf_xmpp.js
endif

if BUILDCORE
MAYBE_CORE = libopensrf c-apps router srfsh gateway perl
if BUILDPYTHON
dist_bin_SCRIPTS = @top_srcdir@/bin/opensrf-perl.pl @top_srcdir@/src/python/opensrf.py @top_srcdir@/src/python/srfsh.py
else
dist_bin_SCRIPTS = @top_srcdir@/bin/opensrf-perl.pl
endif
bin_SCRIPTS = @top_srcdir@/bin/osrf_config @top_srcdir@/bin/osrf_ctl.sh 
dist_sysconf_DATA = @top_srcdir@/examples/opensrf.xml.example @top_srcdir@/examples/opensrf_core.xml.example @top_srcdir@/examples/srfsh.xml.example 
endif

SUBDIRS = $(MAYBE_CORE) $(MAYBE_PY) $(MAYBE_JA)

install-exec-local:
	$(MKDIR_P) $(DESTDIR)$(VAR)
	$(MKDIR_P) $(DESTDIR)$(PID)
	$(MKDIR_P) $(DESTDIR)$(LOG)
	$(MKDIR_P) $(DESTDIR)$(SOCK)
	$(MKDIR_P) $(DESTDIR)$(jsdir)

install-exec-hook:
	if [ "$(MAYBE_CORE)" ]; then \
		for f in opensrf.xml.example opensrf_core.xml.example srfsh.xml.example ; do \
			mv "$(DESTDIR)@sysconfdir@/$${f}" "$(DESTDIR)@sysconfdir@/$${f}.bak" ; \
			sed -e 's|LOCALSTATEDIR|$(VAR)|g' -e 's|SYSCONFDIR|$(ETCDIR)|g' \
				"$(DESTDIR)@sysconfdir@/$${f}.bak" > "$(DESTDIR)@sysconfdir@/$${f}" ; \
			rm "$(DESTDIR)@sysconfdir@/$${f}.bak" ; \
		done; \
	fi
	cp -r @srcdir@/javascript/* $(DESTDIR)$(jsdir)/
	if [ ! -e @bindir@/osrf_control ]; then \
		ln -s @bindir@/opensrf-perl.pl @bindir@/osrf_control; \
	fi

uninstall-hook:
	rm -f @includedir@/opensrf/apachetools.h

