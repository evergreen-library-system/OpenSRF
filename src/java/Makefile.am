DISTCLEANFILES = Makefile.in Makefile

MEMCACHE=java_memcached-release_2.0.1.jar
JSON=json.jar

JAVAC=javac -J-Xmx256m
JAVA=java -Xmx256m 
JAVA_LIBDIR = .lib
JAVA_LIBS = .:$(OSRF_JAVA_DEPSDIR)/$(MEMCACHE):$(OSRF_JAVA_DEPSDIR)/$(JSON)
JAVA_SRC = \
	org/opensrf/net/xmpp/*.java \
	org/opensrf/util/*.java \
	org/opensrf/*.java \
	org/opensrf/test/*.java 

#------------------------------------------------------------------

all-local:	verify_deps dirs jar

verify_deps:
	@if [ ! -e "$(OSRF_JAVA_DEPSDIR)/$(MEMCACHE)" ]; then echo -e "\nmissing dependency $(MEMCACHE)!\n" && exit 1; fi
	@if [ ! -e "$(OSRF_JAVA_DEPSDIR)/$(JSON)" ]; then echo -e "\nmissing dependency $(JSON)!\n" && exit 1; fi

dirs:
	mkdir -p $(JAVA_LIBDIR)

opensrf:
	$(JAVAC) -d $(JAVA_LIBDIR) -cp $(JAVA_LIBS) $(JAVA_SRC) 2>&1 

jar:	opensrf
	rm -f opensrf.jar
	jar cf opensrf.jar -C $(JAVA_LIBDIR) org

# only prints the first 30 lines of errors
slim:
	mkdir -p $(JAVA_LIBDIR)
	$(JAVAC) -d $(JAVA_LIBDIR) -cp $(JAVA_LIBS) $(JAVA_SRC) 2>&1 | head -n 30
	@echo -e "\nTruncating at 30 lines"

check:
	mkdir -p $(JAVA_LIBDIR)
	$(JAVAC) -Xlint:unchecked -d $(JAVA_LIBDIR) -cp $(JAVA_LIBS) $(JAVA_SRC) 2>&1 | head -n 30
	@echo -e "\nTruncating at 30 lines"

run:
	$(JAVA) -cp $(JAVA_LIBS):opensrf.jar $(JAVA_EXE) $(JAVA_ARGS)

docs:
	find . -name *.java > files;
	javadoc -classpath $(JAVA_LIBS) -d doc @files;
	rm files;

install-data-local:
	mkdir -p $(LIBDIR)/java
	cp opensrf.jar $(LIBDIR)/java

dep_clean:
	rm -rf deps
