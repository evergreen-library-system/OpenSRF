# makefile for OpenSRF Python modules and scripts

DISTCLEANFILES = Makefile.in Makefile

all-local:
	@echo $@
	python @srcdir@/setup.py build

# ------------------------------------------------------------------------------
# INSTALL
# ------------------------------------------------------------------------------
install-data-local:	
	@echo $@
	python @srcdir@/setup.py install --root $(DESTDIR)///

distclean-local:
	rm -f @builddir@/OpenSRF.egg-info/PKG-INFO
	rm -f @builddir@/OpenSRF.egg-info/requires.txt
	rm -f @builddir@/OpenSRF.egg-info/top_level.txt
	rm -f @builddir@/OpenSRF.egg-info/SOURCES.txt
	rm -f @builddir@/OpenSRF.egg-info/dependency_links.txt
	rm -f @builddir@/build/scripts-2.5/srfsh.py
	rm -f @builddir@/dist/OpenSRF-1.0.0-py2.5.egg

