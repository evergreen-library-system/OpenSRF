# dependencies
STAX=stax-api-1.0.1.jar
WSTX=wstx-lgpl-3.2.1.jar
MEMCACHE=java_memcached-release_1.5.1.jar
JSON=json.zip

STAX_URL=http://woodstox.codehaus.org/$(STAX)
WSTX_URL=http://woodstox.codehaus.org/3.2.1/$(WSTX)
MEMCACHE_URL=http://img.whalin.com/memcached/jdk5/standard/$(MEMCACHE)
JSON_URL=http://www.json.org/java/$(JSON)


JAVAC=javac -J-Xmx256m
JAVA=java -Xmx256m 
JAVA_LIBDIR = .lib
JAVA_LIBS = .:$(JAVA_LIBDIR):ext/$(WSTX):ext/$(STAX):ext/$(MEMCACHE)
JAVA_SRC = \
	org/opensrf/net/xmpp/*.java \
	org/opensrf/util/*.java \
	org/opensrf/*.java \
	org/opensrf/test/*.java 


#------------------------------------------------------------------

all:	jar


dirs:
	mkdir -p $(JAVA_LIBDIR)

opensrf:	deps
	$(JAVAC) -d $(JAVA_LIBDIR) -cp $(JAVA_LIBS) $(JAVA_SRC) 2>&1 

jar:	opensrf
	rm -f opensrf.jar
	echo "creating opensrf.jar"
	jar cvf opensrf.jar -C .lib org/opensrf/

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
	@$(JAVA) -cp $(JAVA_LIBS) $(JAVA_EXE) $(JAVA_ARGS)

deps:
	mkdir -p ext
	mkdir -p $(JAVA_LIBDIR)
	if [ ! -f ext/$(STAX) ]; then wget '$(STAX_URL)' -O ext/$(STAX); fi 
	if [ ! -f ext/wstx-lgpl-3.2.1.jar ]; then wget '$(WSTX_URL)' -O ext/$(WSTX); fi
	if [ ! -f ext/java_memcached-release_1.5.1.jar ]; then wget '$(MEMCACHE_URL)' -O ext/$(MEMCACHE); fi
	if [ ! -f .tmp/$(JSON) ]; then mkdir -p .tmp && cd .tmp && wget '$(JSON_URL)' && unzip $(JSON); fi
	$(JAVAC) -d $(JAVA_LIBDIR) .tmp/org/json/*.java

docs:
	find . -name *.java > files;
	javadoc -classpath $(JAVA_LIBS) -d doc @files;
	rm files;

clean:	
	rm -rf $(JAVA_LIBDIR) opensrf.jar

dep_clean:
	rm -rf ext .tmp

 	
