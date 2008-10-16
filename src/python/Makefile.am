# makefile for OpenSRF Python modules and scripts

all-local:
	@echo $@
	python @srcdir@/setup.py build

# ------------------------------------------------------------------------------
# INSTALL
# ------------------------------------------------------------------------------
install-data-local:	
	@echo $@
	python @srcdir@/setup.py install

distclean-local:
	rm @builddir@/OpenSRF.egg-info/PKG-INFO
	rm @builddir@/OpenSRF.egg-info/requires.txt
	rm @builddir@/OpenSRF.egg-info/top_level.txt
	rm @builddir@/OpenSRF.egg-info/SOURCES.txt
	rm @builddir@/OpenSRF.egg-info/dependency_links.txt
	rm @builddir@/build/scripts-2.5/srfsh.py
	rm @builddir@/dist/OpenSRF-1.0.0-py2.5.egg

