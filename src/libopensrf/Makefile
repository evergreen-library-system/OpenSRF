CFLAGS	+=  -DASSUME_STATELESS  -DOSRF_STRICT_PARAMS -rdynamic -fno-strict-aliasing #-DOSRF_JSON_ALLOW_COMMENTS 
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

JSON_TARGETS = osrf_json_object.o osrf_json_parser.o osrf_json_tools.o osrf_legacy_json.o osrf_json_xml.o

all: opensrf

# Build the OpenSRF C binary
opensrf:	opensrf.o libopensrf.so
	@echo $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -lopensrf opensrf.o -o $@


# Build the OpenSRF library
libopensrf.so:	$(TARGETS) 
	make -f Makefile.json 
	if [ ! -z "$(OSRF_LEGACY_JSON)" ]; then\
		$(CC) -shared -W1 $(LDFLAGS) $(LDLIBS) $(TARGETS) -o $(TMPDIR)/libopensrf.so;\
	else\
		$(CC) -shared -W1 $(LDFLAGS) $(LDLIBS) $(TARGETS) $(JSON_TARGETS) -o $(TMPDIR)/libopensrf.so;\
	fi; 


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

clean:
	/bin/rm -f *.o *.so opensrf

