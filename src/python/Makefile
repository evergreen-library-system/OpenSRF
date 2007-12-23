# makefile for OpenSRF Python modules and scripts

install: 	python-install

# --------------------------------------------------------------------------------
# INSTALL
# --------------------------------------------------------------------------------
python-build:
	@echo $@
	python setup.py build

python-install:	python-build
	@echo $@
	python setup.py install

# --------------------------------------------------------------------------------
# CLEAN	
# --------------------------------------------------------------------------------
clean:
	/bin/rm -rf build 

