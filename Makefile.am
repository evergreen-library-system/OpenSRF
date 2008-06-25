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


export PREFIX                   = @prefix@
export TMP                      = @TMP@
export LIBXML2_HEADERS          = @LIBXML2_HEADERS@
export APR_HEADERS              = @APR_HEADERS@
export ETCDIR                   = @sysconfdir@
export APXS2                    = @APXS2@
export APACHE2_HEADERS          = @APACHE2_HEADERS@
export DEF_CFLAGS		= -D_LARGEFILE64_SOURCE -pipe -g -Wall -O2 -fPIC -I@abs_top_builddir@/include/ -I$(LIBXML2_HEADERS) -I$(APACHE2_HEADERS) -I$(APR_HEADERS) @INCLUDES@
export DEF_LDFLAGS		= -Wl, -rpath=$(LIBDIR) -L$(TMPDIR) @LIBDIRS@
export DEF_LDLIBS		= -lobjson -lopensrf
export PID			= @localstatedir@/run/opensrf
export SOCK			= @localstatedir@/lock/opensrf
export LOG			= @localstatedir@/log/opensrf

AM_CFLAGS = $(DEF_CFLAGS)
LDFLAGS = $(DEF_LDFLAGS) -L./src/

SUBDIRS = src

jserver:
	make -s -C src jserver

jserver-install:
	make -s -C src jserver-install

javascript-install:
	make -s -C src javascript-install

