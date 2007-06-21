# --------------------------------------------------------------------
# Copyright (C) 2005  Georgia Public Library Service 
# Bill Erickson <highfalutin@gmail.com>
# Mike Rylander <mrylander@gmail.com>

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# --------------------------------------------------------------------


# --------------------------------------------------------------------
#
# -DSTRICT_JSON_WRITE if not defined, 'null' objects will be written
#	as empty strings. e.g. [null, null] => [,,]
#
# -DSTRICT_JSON_READ if not defiend, empty array and object values
#	(not keys) will be considered null.
#	e.g. [,,{"key":}] will be parsed as [null, null, {"key":null}]
#
# --------------------------------------------------------------------

OBJS = utils.o json2xml.o object.o json_parser.o xml2json.o
CFLAGS += -DSTRICT_JSON_WRITE -DOSRF_DISABLE_MD5 
TARGETS = object.o json_parser.o json2xml.o xml2json.o ../libopensrf/utils.o ../libopensrf/md5.o
JSON_INC = ../../include/objson


all: libobjson.so

object.o:	object.c $(JSON_INC)/object.h
json_parser.o:	json_parser.c $(JSON_INC)/json_parser.h
json2xml.o:	json2xml.c $(JSON_INC)/json2xml.h
xml2json.o:	xml2json.c $(JSON_INC)/xml2json.h
../libopensrf/utils.o: ../libopensrf/utils.c ../../include/opensrf/utils.h
../libopensrf/md5.o: ../libopensrf/md5.c ../../include/opensrf/md5.h

install:
	cp -r ../../include/objson $(INCLUDEDIR)
	cp $(TMPDIR)/libobjson.so $(LIBDIR)/libobjson.so

libobjson.so:	$(TARGETS)
	$(CC) -shared -W1 $(TARGETS) -o $(TMPDIR)/libobjson.so

clean:
	/bin/rm -f *.o *.so 

