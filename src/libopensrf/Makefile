# ------------------------------------------------------------------
# To build the standalone JSON lib libosrf_json.so:
# 	$ make libosrf_json.so
# To build the standalone JSON lib libosrf_json.so with XML utils 
# 	support, use something like the following:
# 	$ CFLAGS="-DOSRF_JSON_ENABLE_XML_UTILS -I/usr/include/libxml2" LDLIBS=-lxml2 make libosrf_json.so
#
# The compiler flag -DOSRF_JSON_ALLOW_COMMENTS tells the parser to 
# allow legacy JSON comments like /* comment */
# ------------------------------------------------------------------

CFLAGS	+=  -DASSUME_STATELESS  -DOSRF_STRICT_PARAMS -rdynamic -fno-strict-aliasing -I../../include -fPIC -Wall 
LDLIBS	+= -lxml2 -ldl -lmemcache 
export OSRF_INC = ../../include/opensrf/

TARGETS = 	osrf_message.o \
			osrf_app_session.o \
			osrf_stack.o \
			osrf_system.o \
			osrf_settings.o \
			osrf_prefork.o \
			osrfConfig.o \
			osrf_application.o \
			osrf_cache.o \
			osrf_transgroup.o \
			osrf_list.o \
			osrf_hash.o \
			xml_utils.o \
			transport_message.o\
			transport_session.o\
			transport_client.o\
			md5.o\
			log.o\
			utils.o\
			socket_bundle.o\
			sha.o\
			string_array.o

JSON_TARGETS = 	osrf_json_object.o\
				osrf_json_parser.o \
				osrf_json_tools.o \
				osrf_legacy_json.o \
				osrf_json_xml.o

# use these when building the standalone JSON module
JSON_DEPS = osrf_list.o\
			osrf_hash.o\
			utils.o\
			log.o\
			md5.o\
			string_array.o


all: opensrf

# Build the OpenSRF C binary
opensrf:	opensrf.o libopensrf.so
	@echo $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -lopensrf opensrf.o -o $@


# Build the OpenSRF library
libopensrf.so:	$(TARGETS) json
	if [ ! -z "$(OSRF_LEGACY_JSON)" ]; then\
		$(CC) -shared -W1 $(LDFLAGS) $(LDLIBS) $(TARGETS) -o $(TMPDIR)/libopensrf.so;\
	else\
		$(CC) -shared -W1 $(LDFLAGS) $(LDLIBS) $(TARGETS) $(JSON_TARGETS) -o $(TMPDIR)/libopensrf.so;\
	fi; 


json:	$(JSON_TARGETS) $(JSON_DEPS)
	if [ ! -z "$(OSRF_LEGACY_JSON)" ]; then \
		$(CC) -shared -W1  $(CFLAGS) -D OSRF_JSON_ENABLE_XML_UTILS \
			$(LDFLAGS) $(JSON_TARGETS) $(JSON_DEPS) -o $(TMPDIR)/libobjson.so;\
	fi;

libosrf_json.so:	$(JSON_TARGETS) $(JSON_DEPS)
	$(CC) -shared -W1 $(CFLAGS) -D OSRF_JSON_ENABLE_XML_UTILS \
		$(LDFLAGS) $(LDLIBS) $(JSON_TARGETS) $(JSON_DEPS) -o $@


osrf_json_test:	osrf_json_test.o $(JSON_TARGETS) $(JSON_DEPS)
	$(CC) $(CFLAGS) osrf_json_test.o $(JSON_TARGETS) $(JSON_DEPS) -o $@


opensrf.o:	opensrf.c
transport_message.o:	transport_message.c $(OSRF_INC)/transport_message.h	
transport_session.o:	transport_session.c $(OSRF_INC)/transport_session.h	
transport_client.o:	transport_client.c $(OSRF_INC)/transport_client.h	
osrf_message.o:	osrf_message.c $(OSRF_INC)/osrf_message.h
osrf_app_session.o:	osrf_app_session.c $(OSRF_INC)/osrf_app_session.h
osrf_stack.o:	osrf_stack.c $(OSRF_INC)/osrf_stack.h
osrf_system.o:	osrf_system.c $(OSRF_INC)/osrf_system.h
osrf_settings.o:	osrf_settings.c $(OSRF_INC)/osrf_settings.h
osrf_prefork.o:	osrf_prefork.c $(OSRF_INC)/osrf_prefork.h
osrfConfig.o:	osrfConfig.c $(OSRF_INC)/osrfConfig.h xml_utils.o
osrf_application.o: osrf_application.c $(OSRF_INC)/osrf_application.h
osrf_cache.o:	osrf_cache.c $(OSRF_INC)/osrf_cache.h
osrf_list.o:	osrf_list.c $(OSRF_INC)/osrf_list.h
osrf_hash.o:	osrf_hash.c $(OSRF_INC)/osrf_hash.h
md5.o:	md5.c $(OSRF_INC)/md5.h
log.o:	log.c $(OSRF_INC)/log.h	
utils.o:	utils.c $(OSRF_INC)/utils.h
socket_bundle.o:	socket_bundle.c $(OSRF_INC)/socket_bundle.h
sha.o:	sha.c $(OSRF_INC)/sha.h
string_array.o:	string_array.c $(OSRF_INC)/string_array.h
osrf_json_object.o:	osrf_json_object.c $(OSRF_INC)/osrf_json.h $(OSRF_INC)/osrf_json_utils.h
osrf_json_parser.o:	osrf_json_parser.c $(OSRF_INC)/osrf_json.h $(OSRF_INC)/osrf_json_utils.h
osrf_json_tools.o:	osrf_json_tools.c $(OSRF_INC)/osrf_json.h $(OSRF_INC)/osrf_json_utils.h
osrf_legacy_json.o:	osrf_legacy_json.c $(OSRF_INC)/osrf_json.h $(OSRF_INC)/osrf_json_utils.h
osrf_json_xml.o:	osrf_json_xml.c $(OSRF_INC)/osrf_json.h $(OSRF_INC)/osrf_json_xml.h
osrf_json_test.o:	osrf_json_test.c


clean:
	/bin/rm -f *.o *.so opensrf osrf_json_test







# ------------------------------------------------------------------
# To build a standalone version of libosrf_json, something 
# like the following should work:
# $ CFLAGS="-fPIC -I ../../include" OSRF_INC="../../include/opensrf" make -f Makefile.json standalone
#
# -- with XML utils included --
# $ CFLAGS="-fPIC -I /usr/include/libxml2 -I ../../include -D OSRF_JSON_ENABLE_XML_UTILS" \
# 	OSRF_INC="../../include/opensrf" LDLIBS="-lxml2" \
# 	make -f Makefile.json standalone
#
# ------------------------------------------------------------------

#CFLAGS="-I ../../include" OSRF_INC="../../include/opensrf" make -f Makefile.json clean osrf_json_test
#osrf_json_test:	osrf_json_test.o $(TARGETS) $(EXT_TARGETS)
#	$(CC) $(CFLAGS) osrf_json_test.o $(TARGETS) $(EXT_TARGETS) -o $@
#osrf_json_test.o:	osrf_json_test.c
#
#clean:
#	rm -f osrf_json*.o osrf_legacy_json.o libosrf_json.so osrf_json_test

