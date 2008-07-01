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


AM_LDFLAGS = $(DEF_LDFLAGS)
AM_CFLAGS = $(DEF_CFLAGS) -DASSUME_STATELESS -L../libopensrf/

curdir = @abs_top_builddir@/src/gateway/

cur_LTLIBRARIES = libosrf_json_gateway.la
libosrf_json_gateway_la_SOURCES = apachetools.c apachetools.h osrf_json_gateway.c
libosrf_json_gateway_la_LIBADD = -lobjson -lopensrf

include_HEADERS = apachetools.h

install-exec-local: 
	cp .libs/libosrf_json_gateway.so osrf_json_gateway.so
	$(APXS2) -i -a -n osrf_json_gateway osrf_json_gateway.so
