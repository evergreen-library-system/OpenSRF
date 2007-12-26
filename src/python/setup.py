#!/usr/bin/env python
from distutils.core import setup
import os, os.path

dir = os.path.dirname(__file__)

setup(name='OpenSRF',
    version='1.0',
# requires is not actually implemented in distutils
#    requires=['memcache', 'pyxmpp', 'simplejson'],
    description='OpenSRF Python Modules',
    author='Bill Erickson',
    author_email='open-ils-dev@list.georgialibraries.org',
    url='http://www.open-ils.org/',
    packages=['osrf'],
    package_dir={'': dir},
    scripts=[os.path.join(dir, 'srfsh.py')]
)
