# -----------------------------------------------------------------------
# Copyright (C) 2007  Georgia Public Library Service
# Bill Erickson <billserickson@gmail.com>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# -----------------------------------------------------------------------


from osrf.utils import *
from osrf.ex import *
import re

class osrfConfig(object):
    """Loads and parses the bootstrap config file"""

    config = None

    def __init__(self, file, context=None):
        self.file = file    
        self.context = context
        self.data = {}

    #def parseConfig(self,file=None):
    def parseConfig(self):
        self.data = osrfXMLFileToObject(self.file)
        osrfConfig.config = self
    
    def getValue(self, key, idx=None):
        if self.context:
            if re.search('/', key):
                key = "%s/%s" % (self.context, key)
            else:
                key = "%s.%s" % (self.context, key)

        val = osrfObjectFindPath(self.data, key, idx)
        if not val:
            raise osrfConfigException("Config value not found: " + key)
        return val


def osrfConfigValue(key, idx=None):
    """Returns a bootstrap config value.

    key -- A string representing the path to the value in the config object
        e.g.  "domains.domain", "username"
    idx -- Optional array index if the searched value is an array member
    """
    return osrfConfig.config.getValue(key, idx)
                

def osrfConfigValueNoEx(key, idx=None):
    """ Returns a bootstrap config value without throwing an exception
        if the item is not found. 

    key -- A string representing the path to the value in the config object
        e.g.  "domains.domain", "username"
    idx -- Optional array index if the searched value is an array member
    """
    try:
        return osrfConfig.config.getValue(key, idx)
    except:
        return None

