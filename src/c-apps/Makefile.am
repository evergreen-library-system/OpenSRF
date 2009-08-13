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
AM_LDFLAGS = $(DEF_LDFLAGS) -L@top_builddir@/src/libopensrf

DISTCLEANFILES = Makefile.in Makefile

noinst_PROGRAMS = timejson
lib_LTLIBRARIES = osrf_dbmath.la osrf_math.la osrf_version.la

timejson_SOURCES = timejson.c
timejson_LDADD = -lopensrf
osrf_dbmath_la_SOURCES = osrf_dbmath.c 
osrf_dbmath_la_LDFLAGS = $(AM_LDFLAGS) -module
osrf_dbmath_la_LIBADD = -lopensrf
osrf_math_la_SOURCES = osrf_math.c
osrf_math_la_LDFLAGS = $(AM_LDFLAGS) -module
osrf_math_la_LIBADD =  -lopensrf
osrf_version_la_SOURCES = osrf_version.c 
osrf_version_la_LDFLAGS = $(AM_LDFLAGS) -module
osrf_version_la_LIBADD = -lopensrf
