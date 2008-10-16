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
jsdir = $(LIBDIR)/javascript
export OSRF_JAVA_DEPSDIR = @OSRF_JAVA_DEPSDIR@
etcdir = $(ETCDIR)


AM_LDFLAGS = $(DEF_LDFLAGS)
AM_CFLAGS = $(DEF_CFLAGS)

if BUILDPYTHON
MAYBE_PY = python
endif 

if BUILDJAVA
MAYBE_JA = java
endif

SUBDIRS = libopensrf c-apps router srfsh jserver gateway $(MAYBE_PY) $(MAYBE_JA)

dist_bin_SCRIPTS = @top_srcdir@/bin/osrf_ctl.sh @top_srcdir@/bin/opensrf-perl.pl
bin_SCRIPTS = @top_srcdir@/bin/osrf_config

dist_sysconf_DATA = @top_srcdir@/examples/opensrf.xml.example @top_srcdir@/examples/opensrf_core.xml.example @top_srcdir@/examples/srfsh.xml.example 

install-exec-local:
	mkdir -p $(VAR)
	mkdir -p $(PID)
	mkdir -p $(LOG)
	mkdir -p $(SOCK)
	mkdir -p $(jsdir)
	make install-perl

install-exec-hook:
	sed -i 's|LOCALSTATEDIR|$(VAR)|g' '$(DESTDIR)@sysconfdir@/opensrf.xml.example'
	sed -i 's|SYSCONFDIR|$(ETCDIR)|g' '$(DESTDIR)@sysconfdir@/opensrf.xml.example'
	sed -i 's|LOCALSTATEDIR|$(VAR)|g' '$(DESTDIR)@sysconfdir@/opensrf_core.xml.example'
	sed -i 's|SYSCONFDIR|$(ETCDIR)|g' '$(DESTDIR)@sysconfdir@/opensrf_core.xml.example'
	sed -i 's|LOCALSTATEDIR|$(VAR)|g' '$(DESTDIR)@sysconfdir@/srfsh.xml.example'
	sed -i 's|SYSCONFDIR|$(ETCDIR)|g' '$(DESTDIR)@sysconfdir@/srfsh.xml.example'
	sed -i 's|SYSCONFDIR|$(ETCDIR)|g' '@abs_top_srcdir@/examples/math_bench.pl'
	sed -i 's|LIBDIR|$(LIBDIR)|g' '@abs_top_srcdir@/examples/multisession-test.pl'
	sed -i 's|SYSCONFDIR|$(ETCDIR)|g' '@abs_top_srcdir@/doc/dokuwiki-doc-stubber.pl'
	cp -r @srcdir@/javascript/* $(jsdir)/


install-perl:
	cd ./perl && perl Makefile.PL || make -s install-perl-fail
	make -C perl
	make -C perl test || make -s install-perl-fail
	make -C perl install

install-perl-fail:
	echo
	echo ">>> Installation of Perl modules has failed. The most likely"
	echo ">>> possibility is that a dependency is not pre-installed"
	echo ">>> or that a test has failed."
	echo ">>> See the messages above this one for more information."
	echo
	exit 1

uninstall-hook:
	rm @includedir@/opensrf/apachetools.h
	rm -R $(jsdir)

