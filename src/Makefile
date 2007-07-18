# TOP level 'src' makefile for OpenSRF


export TMPDIR			= $(TMP)/opensrf
export OPENSRF			= opensrf
export BINDIR			= $(PREFIX)/bin
export LIBDIR			= $(PREFIX)/lib
export PERLDIR			= $(LIBDIR)/perl5
export INCLUDEDIR		= $(PREFIX)/include

export LDLIBS			+= 
export LDFLAGS			+= -Wl,-rpath=$(LIBDIR) -L $(TMPDIR) -L .
export CFLAGS			+= -pipe -g -Wall -O2 -fPIC -I ../../include/ -I$(LIBXML2_HEADERS) -I$(APACHE2_HEADERS) \
								-I$(LIBXML2_HEADERS)/libxml -I$(APR_HEADERS) 

all: 	prep \
	opensrf \
	router \
	srfsh \
	jserver \
	gateway

install: 	install-prep \
		opensrf-install \
		gateway-install	\
		router-install \
		srfsh-install \
		jserver-install \
		perl-install \
		objson-install


# --------------------------------------------------------------------------------
# BINARIES and LIBS
# --------------------------------------------------------------------------------

prep:
	mkdir -p $(TMPDIR)

objson/libobjson.so:	prep
	@echo $@
	make -C objson

opensrf:	objson/libobjson.so
	make -C libopensrf
	make -C c-apps

router: opensrf
	@echo $@
	make -C router 

srfsh: opensrf
	@echo $@
	make -C srfsh

gateway:	opensrf
	@echo $@
	make -C gateway

jserver:	opensrf
	@echo $@
	make -C jserver


# --------------------------------------------------------------------------------
# INSTALL
# --------------------------------------------------------------------------------

# make sure the install directories exist
install-prep:	
	@echo $@
	mkdir -p $(LIBDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(PERLDIR)
	mkdir -p $(INCLUDEDIR)
	mkdir -p $(ETCDIR)

objson-install:	install-prep 
	@echo $@
	make -C objson install

# installs libopensrf.so, opensrf-c, headers, example configs, and osrf_ctl.sh
opensrf-install:	objson-install
	@echo $@
	cp $(TMPDIR)/libopensrf.so $(LIBDIR)/libopensrf.so
	cp -r ../include/opensrf $(INCLUDEDIR)
	cp libopensrf/opensrf $(BINDIR)/opensrf-c
	make -C c-apps install
	cp ../bin/osrf_ctl.sh $(BINDIR)
	cp ../examples/opensrf.xml.example $(ETCDIR)
	cp ../examples/opensrf_core.xml.example $(ETCDIR)
	cp ../examples/srfsh.xml.example $(ETCDIR)

gateway-install:	install-prep opensrf-install	
	@echo $@
	make -C gateway install

router-install:	install-prep opensrf-install
	@echo $@
	make -C router install

srfsh-install:	install-prep opensrf-install 
	@echo $@
	make -C srfsh install

jserver-install:	install-prep 
	@echo $@
	make -C jserver install

perl-install:	install-prep
	@echo $@
	cp -r perlmods/* $(PERLDIR)/


# --------------------------------------------------------------------------------
# CLEAN	
# --------------------------------------------------------------------------------
clean:
	make -C libopensrf clean
	make -C router clean
	make -C gateway clean
	make -C jserver clean
	make -C objson clean
	make -C srfsh clean
	make -C c-apps clean
	/bin/rm -rf $(TMPDIR) *.o


