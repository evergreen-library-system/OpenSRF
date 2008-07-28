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


EXTRA_DIST = @srcdir@/apachetools.c @srcdir@/apachetools.h @srcdir@/osrf_json_gateway.c @srcdir@/osrf_http_translator.c

options=-D_LARGEFILE64_SOURCE -Wall -I@abs_top_srcdir@/include/ -I$(LIBXML2_HEADERS) -I$(APACHE2_HEADERS) -I$(APR_HEADERS) -L$(LIBDIR) -L@top_builddir@/src/libopensrf

all-local:
	$(APXS2) -c $(DEF_LDLIBS) $(options) @srcdir@/osrf_json_gateway.c
	$(APXS2) -c $(DEF_LDLIBS) $(options) @srcdir@/osrf_http_translator.c

install-exec-local: 
	$(APXS2) -i -a @srcdir@/osrf_json_gateway.la
	$(APXS2) -i -a @srcdir@/osrf_http_translator.la

clean-local:
	rm -f @srcdir@/osrf_http_translator.la @srcdir@/osrf_http_translator.lo @srcdir@/osrf_http_translator.slo @srcdir@/osrf_json_gateway.la @srcdir@/osrf_json_gateway.lo @srcdir@/osrf_json_gateway.slo
