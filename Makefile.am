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

DOC_FILES = doc/Application-HOWTO.txt \
	    doc/dokuwiki-doc-stubber.pl \
	    doc/OpenSRF-Messaging-Protocol.html \
	    doc/Persist-API.html \
	    doc/Roadmap.txt

EXAMPLES_FILES = examples/fieldmapper2cdbi.xsl \
		 examples/fieldmapper2javascript.xsl \
		 examples/fieldmapper2perl.xsl \
		 examples/gen-fieldmapper.xml \
		 examples/math_bench.pl \
		 examples/multisession-test.pl \
		 examples/register.pl \
		 examples/srfsh_config.xsd \
		 examples/math_xul_client/math \
		 examples/math_xul_client/install.js

strn_compat_FILES = src/ports/strn_compat/strndup.c \
		    src/ports/strn_compat/strndup.h \
		    src/ports/strn_compat/strnlen.c \
		    src/ports/strn_compat/strnlen.h

python_FILES = src/python/opensrf.py \
	       src/python/setup.py \
	       src/python/srfsh.py \
	       src/python/osrf

java_FILES = src/java/deps.inc \
	     src/java/deps.sh \
	     src/java/org

libosrf_FILES = src/libopensrf/basic_client.c \
		src/libopensrf/osrf_big_hash.c \
		src/libopensrf/osrf_big_list.c \
		src/libopensrf/osrfConfig.c


EXTRA_DIST = $(DOC_FILES) $(EXAMPLES_FILES) $(libosrf_FILES) $(strn_compat_FILES) $(python_FILES) $(java_FILES) autogen.sh src/extras src/gateway/fieldmapper-c-xml-out.pl DCO-1.1.txt LICENSE.txt src/perlmods src/javascript

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
	mv @srcdir@/src/gateway/apachetools.h @includedir@/opensrf/apachetools.h

