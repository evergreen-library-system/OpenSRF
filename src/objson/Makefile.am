AUTOMAKE_OPTIONS = foreign -Wall

AM_CPPFLAGS = -I$(top_srcdir)/../../include

MAINTAINERCLEANFILES= Makefile.in

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

lib_LTLIBRARIES = libobjson.la
libobjson_la_SOURCES = json2xml.c json_parser.c object.c xml2json.c
libobjson_la_LDFLAGS = -version-info 1:0

bin_PROGRAMS = objson_test
objson_test_SOURCES = objson_test.c ../libopensrf/md5.c ../libopensrf/utils.c
objson_test_LDADD = libobjson.la
