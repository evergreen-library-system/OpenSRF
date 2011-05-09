"""
Trigger nosetests to give us a complete coverage statement

nosetests only reports on the modules that are imported in
the tests it finds; this file serves as a placeholder until
we actually provide unit test coverage of the core files.
"""

import osrf.app
import osrf.cache
import osrf.conf
import osrf.const
import osrf.ex
import osrf.gateway
# Triggers an exception if mod_python is not installed
#import osrf.http_translator
import osrf.json
import osrf.log
import osrf.net_obj
import osrf.net
import osrf.server
import osrf.ses
import osrf.set
import osrf.stack
import osrf.system
import osrf.xml_obj
import unittest

if __name__ == '__main__':
    unittest.main()
