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

export TMPDIR	= $(TMP)/opensrf
export OPENSRF	= opensrf
export BINDIR	= @bindir@
export LIBDIR	= @libdir@
export PERLDIR	= $(LIBDIR)/perl5
export JSDIR	= $(LIBDIR)/javascript
export INCLUDEDIR= @includedir@
export OSRF_JAVA_DEPSDIR = @OSRF_JAVA_DEPSDIR@

AM_LDFLAGS = $(DEF_LDFLAGS)
AM_CFLAGS = $(DEF_CFLAGS)

if DEBUG
AM_CFLAGS += -DNDEBUG
endif

if BUILDJSON
LDADD = -lobjson
BJSON=1
endif

if BUILDPYTHON
MAYBE_PY = python
endif 

if BUILDJAVA
MAYBE_JA = java
endif

SUBDIRS = libopensrf c-apps router srfsh jserver gateway $(MAYBE_PY) $(MAYBE_JA)

install-data-local:
	mkdir -p $(JSDIR)
	mkdir -p $(PERLDIR)
	mkdir -p $(ETCDIR)
	cp -r ../include/opensrf $(INCLUDEDIR)
	mv $(BINDIR)/opensrf $(BINDIR)/opensrf-c
	cp ../bin/osrf_config $(BINDIR)
	cp ../bin/osrf_ctl.sh $(BINDIR)
	cp ../examples/opensrf.xml.example $(ETCDIR)
	cp ../examples/opensrf_core.xml.example $(ETCDIR)
	cp ../examples/srfsh.xml.example $(ETCDIR)
	cp -r ../include/objson $(INCLUDEDIR)
	cp -r javascript/* $(JSDIR)/
	cp -r perlmods/* $(PERLDIR)/
	mv $(INCLUDEDIR)/apachetools.h $(INCLUDEDIR)/opensrf/apachetools.h


