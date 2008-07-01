# Copyright (C) 2007-2008 Equinox Software, Inc.
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


AM_CFLAGS = $(DEF_CFLAGS) -DORSF_LOG_PARAMS 
AM_LDFLAGS = $(DEF_LDFLAGS) -L../libopensrf/.libs/ -L.  

noinst_PROGRAMS = timejson
lib_LTLIBRARIES = libosrf_dbmath.la libosrf_math.la libosrf_version.la

timejson_SOURCES = timejson.c
timejson_LDADD = -lobjson -lopensrf
libosrf_dbmath_la_SOURCES = osrf_dbmath.c 
libosrf_dbmath_la_LIBADD = -lobjson -lopensrf
libosrf_math_la_SOURCES = osrf_math.c
libosrf_math_la_LIBADD = -lobjson -lopensrf
libosrf_version_la_SOURCES = osrf_version.c 
libosrf_version_la_LIBADD = -lobjson -lopensrf

## rename libraries (libtool requires the prefix lib, but we dont use that here)
## remove extra library files created by libtool
install-exec-hook:
	cp $(LIBDIR)/libosrf_math.so $(LIBDIR)/osrf_math.so
	cp $(LIBDIR)/libosrf_dbmath.so $(LIBDIR)/osrf_dbmath.so
	cp $(LIBDIR)/libosrf_version.so $(LIBDIR)/osrf_version.so	
	rm $(LIBDIR)/libosrf*
