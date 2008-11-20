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


if !DEBUG
MAYBE_DEBUG = -DNDEBUG
endif

export PREFIX                   = @prefix@
export TMP                      = @TMP@
export LIBXML2_HEADERS          = @LIBXML2_HEADERS@
export APR_HEADERS              = @APR_HEADERS@
export ETCDIR                   = @sysconfdir@
export APXS2                    = @APXS2@
export APACHE2_HEADERS          = @APACHE2_HEADERS@
export DEF_CFLAGS		= -D_LARGEFILE64_SOURCE $(MAYBE_DEBUG) -pipe -g -Wall -O2 -fPIC -I@abs_top_srcdir@/include/ -I$(LIBXML2_HEADERS) -I$(APACHE2_HEADERS) -I$(APR_HEADERS) @INCLUDES@
export DEF_LDLIBS		= -lopensrf
export VAR			= @localstatedir@
export PID			= @localstatedir@/run/opensrf
export SOCK			= @localstatedir@/lock/opensrf
export LOG			= @localstatedir@/log/opensrf
export srcdir			= @srcdir@

AM_CFLAGS = $(DEF_CFLAGS)

DOC_FILES = @srcdir@/doc/Application-HOWTO.txt \
	    @srcdir@/doc/dokuwiki-doc-stubber.pl \
	    @srcdir@/doc/OpenSRF-Messaging-Protocol.html \
	    @srcdir@/doc/Persist-API.html \
	    @srcdir@/doc/Roadmap.txt \
		@srcdir@/AUTHORS \
		@srcdir@/ChangeLog \
		@srcdir@/INSTALL \
		@srcdir@/README

EXAMPLES_FILES = @srcdir@/examples/fieldmapper2cdbi.xsl \
		 @srcdir@/examples/fieldmapper2javascript.xsl \
		 @srcdir@/examples/fieldmapper2perl.xsl \
		 @srcdir@/examples/gen-fieldmapper.xml \
		 @srcdir@/examples/math_bench.pl \
		 @srcdir@/examples/math_client.py \
		 @srcdir@/examples/multisession-test.pl \
		 @srcdir@/examples/register.pl \
		 @srcdir@/examples/srfsh_config.xsd \
		 @srcdir@/examples/math_xul_client/math \
		 @srcdir@/examples/math_xul_client/install.js

strn_compat_FILES = @srcdir@/src/ports/strn_compat/strndup.c \
		    @srcdir@/src/ports/strn_compat/strndup.h \
		    @srcdir@/src/ports/strn_compat/strnlen.c \
		    @srcdir@/src/ports/strn_compat/strnlen.h

python_FILES = @srcdir@/src/python/opensrf.py \
	       @srcdir@/src/python/setup.py \
	       @srcdir@/src/python/srfsh.py \
	       @srcdir@/src/python/osrf

java_FILES = @srcdir@/src/java/deps.inc \
	     @srcdir@/src/java/deps.sh \
	     @srcdir@/src/java/org

libosrf_FILES = @srcdir@/src/libopensrf/basic_client.c \
		@srcdir@/src/libopensrf/osrf_big_hash.c \
		@srcdir@/src/libopensrf/osrf_big_list.c \
		@srcdir@/src/libopensrf/osrfConfig.c


EXTRA_DIST = $(DOC_FILES) $(EXAMPLES_FILES) $(libosrf_FILES) $(strn_compat_FILES) $(python_FILES) $(java_FILES) @srcdir@/autogen.sh @srcdir@/src/extras @srcdir@/COPYING @srcdir@/DCO-1.1.txt @srcdir@/LICENSE.txt @srcdir@/src/perl @srcdir@/src/javascript

opensrfincludedir = @includedir@/opensrf

OSRFINC=@srcdir@/include/opensrf

opensrfinclude_HEADERS = $(OSRFINC)/log.h \
				$(OSRFINC)/md5.h \
				$(OSRFINC)/osrf_application.h \
				$(OSRFINC)/osrf_app_session.h \
				$(OSRFINC)/osrf_big_hash.h \
				$(OSRFINC)/osrf_big_list.h \
				$(OSRFINC)/osrf_cache.h \
				$(OSRFINC)/osrfConfig.h \
				$(OSRFINC)/osrf_hash.h \
				$(OSRFINC)/osrf_json.h \
				$(OSRFINC)/osrf_json_utils.h \
				$(OSRFINC)/osrf_json_xml.h \
				$(OSRFINC)/osrf_legacy_json.h \
				$(OSRFINC)/osrf_list.h \
				$(OSRFINC)/osrf_message.h \
				$(OSRFINC)/osrf_prefork.h \
				$(OSRFINC)/osrf_settings.h \
				$(OSRFINC)/osrf_stack.h \
				$(OSRFINC)/osrf_system.h \
				$(OSRFINC)/osrf_transgroup.h \
				$(OSRFINC)/sha.h \
				$(OSRFINC)/socket_bundle.h \
				$(OSRFINC)/string_array.h \
				$(OSRFINC)/transport_client.h \
				$(OSRFINC)/transport_message.h \
				$(OSRFINC)/transport_session.h \
				$(OSRFINC)/utils.h \
				$(OSRFINC)/xml_utils.h



SUBDIRS = src

jserver:
	make -s -C src jserver

jserver-install:
	make -s -C src jserver-install

javascript-install:
	make -s -C src javascript-install

install-data-hook:
	cp @srcdir@/src/gateway/apachetools.h @includedir@/opensrf/apachetools.h

