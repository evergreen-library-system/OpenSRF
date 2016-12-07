#!/usr/bin/env python

from setuptools import setup

setup(name='OpenSRF',
    version='2.5.0-alpha',
    install_requires=[
        'dnspython', # required by pyxmpp
    	'python-memcached',
        'pyxmpp>=1.0.0',
        'simplejson>=1.7.1'
    ],
    dependency_links = [
        "http://pyxmpp.jajcus.net/downloads/",
        "ftp://ftp.tummy.com/pub/python-memcached/python-memcached-latest.tar.gz"
    ],
    description='OpenSRF Python Modules',
    author='Bill Erickson',
    author_email='erickson@esilibrary.com',
    license="GPL",
    url='http://www.open-ils.org/',
    packages=['osrf', 'osrf.apps'],
    scripts=['srfsh.py']
)
