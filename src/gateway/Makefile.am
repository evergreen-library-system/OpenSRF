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
#
if APACHE_MIN_24
HAVE_APACHE_MIN_24 = -DAPACHE_MIN_24
endif

EXTRA_DIST = @srcdir@/apachetools.c @srcdir@/apachetools.h \
	@srcdir@/osrf_json_gateway.c @srcdir@/osrf_http_translator.c  \
	@srcdir@/osrf_websocket_translator.c

AM_CFLAGS = -D_LARGEFILE64_SOURCE $(HAVE_APACHE_MIN_24) -Wall -I@abs_top_srcdir@/include/ -I$(LIBXML2_HEADERS) -I$(APACHE2_HEADERS) -I$(APR_HEADERS)
AM_LDFLAGS = -L$(LIBDIR) -L@top_builddir@/src/libopensrf
AP_LIBEXECDIR = `$(APXS2) -q LIBEXECDIR`

DISTCLEANFILES = Makefile.in Makefile

install-exec-local: 
	d=`$(APXS2) -q SYSCONFDIR` && \
		if ! grep mod_placeholder $${d}/httpd.conf 2>&1 >/dev/null ; \
		then echo -e "#\n#LoadModule mod_placeholder /usr/lib/apache2/modules/mod_placeholder.so" \
		>> $${d}/httpd.conf; \
	fi
	$(APXS2) -c $(DEF_LDLIBS) $(AM_CFLAGS) $(AM_LDFLAGS) @srcdir@/osrf_json_gateway.c apachetools.c apachetools.h libopensrf.so
	$(APXS2) -c $(DEF_LDLIBS) $(AM_CFLAGS) $(AM_LDFLAGS) @srcdir@/osrf_http_translator.c apachetools.c apachetools.h libopensrf.so
	$(APXS2) -c $(DEF_LDLIBS) $(AM_CFLAGS) $(AM_LDFLAGS) @srcdir@/osrf_websocket_translator.c apachetools.c apachetools.h libopensrf.so
	$(MKDIR_P) $(DESTDIR)$(AP_LIBEXECDIR)
	$(APXS2) -i -S LIBEXECDIR=$(DESTDIR)$(AP_LIBEXECDIR) -a @srcdir@/osrf_json_gateway.la
	$(APXS2) -i -S LIBEXECDIR=$(DESTDIR)$(AP_LIBEXECDIR) -a @srcdir@/osrf_http_translator.la
	$(APXS2) -i -S LIBEXECDIR=$(DESTDIR)$(AP_LIBEXECDIR) -a @srcdir@/osrf_websocket_translator.la

clean-local:
	rm -f @srcdir@/osrf_http_translator.la @srcdir@/osrf_http_translator.lo @srcdir@/osrf_http_translator.slo @srcdir@/osrf_json_gateway.la @srcdir@/osrf_json_gateway.lo @srcdir@/osrf_json_gateway.slo  @srcdir@/osrf_websocket_translator.la @srcdir@/osrf_websocket_translator.lo @srcdir@/osrf_websocket_translator.slo
