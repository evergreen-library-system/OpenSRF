LDLIBS += -lopensrf
CFLAGS += -D_LARGEFILE64_SOURCE -DOSRF_LOG_PARAMS

all:	osrf_math.so osrf_dbmath.so osrf_version.so timejson

timejson.o: timejson.c
osrf_math.o: osrf_math.c
osrf_dbmath.o: osrf_dbmath.c
osrf_version.o: osrf_version.c

timejson: timejson.o
	$(CC) -shared -W1 $(LDLIBS) $(LDFLAGS) timejson.o -o $(TMPDIR)/timejson

osrf_math.so: osrf_math.o
	$(CC) -shared -W1 $(LDLIBS) $(LDFLAGS) osrf_math.o -o $(TMPDIR)/osrf_math.so

osrf_dbmath.so: osrf_dbmath.o
	$(CC) -shared -W1 $(LDLIBS) $(LDFLAGS) osrf_dbmath.o -o $(TMPDIR)/osrf_dbmath.so

osrf_version.so: osrf_version.o
	$(CC) -shared -W1 $(LDLIBS) $(LDFLAGS) osrf_version.o -o $(TMPDIR)/osrf_version.so

install:
	cp $(TMPDIR)/osrf_math.so $(LIBDIR)/
	cp $(TMPDIR)/osrf_dbmath.so $(LIBDIR)/
	cp $(TMPDIR)/osrf_version.so $(LIBDIR)/

clean:
	rm -f *.o *.so
