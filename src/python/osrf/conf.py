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


import osrf.net_obj
import osrf.ex
import osrf.xml_obj
import re

class Config(object):
    """Loads and parses the bootstrap config file"""

    config = None

    def __init__(self, file, context=None):
        self.file = file    
        self.context = context
        self.data = {}

    #def parseConfig(self,file=None):
    def parse_config(self):
        self.data = osrf.xml_obj.xml_file_to_object(self.file)
        Config.config = self
    
    def get_value(self, key, idx=None):
        if self.context:
            if re.search('/', key):
                key = "%s/%s" % (self.context, key)
            else:
                key = "%s.%s" % (self.context, key)

        val = osrf.net_obj.find_object_path(self.data, key, idx)
        if not val:
            raise osrf.ex.OSRFConfigException("Config value not found: " + key)
        return val


def get(key, idx=None):
    """Returns a bootstrap config value.

    key -- A string representing the path to the value in the config object
        e.g.  "domains.domain", "username"
    idx -- Optional array index if the searched value is an array member
    """
    return Config.config.get_value(key, idx)
                

def get_no_ex(key, idx=None):
    """ Returns a bootstrap config value without throwing an exception
        if the item is not found. 

    key -- A string representing the path to the value in the config object
        e.g.  "domains.domain", "username"
    idx -- Optional array index if the searched value is an array member
    """
    try:
        return Config.config.get_value(key, idx)
    except:
        return None

