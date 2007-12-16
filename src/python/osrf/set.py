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

from osrf.const import OSRF_APP_SETTINGS, OSRF_METHOD_GET_HOST_CONFIG
import osrf.ex
import osrf.net_obj

# global settings config object
__config = None

def get(path, idx=0):
    global __config
    val = osrf.net_obj.find_object_path(__config, path, idx)
    if not val:
        raise osrf.ex.OSRFConfigException("Config value not found: " + path)
    return val


def load(hostname):
    global __config

    from osrf.system import connect
    from osrf.ses import ClientSession

    ses = ClientSession(OSRF_APP_SETTINGS)
    req = ses.request(OSRF_METHOD_GET_HOST_CONFIG, hostname)
    resp = req.recv(timeout=30)
    __config = resp.content()
    req.cleanup()
    ses.cleanup()

