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

from osrf.conf import osrfConfig, osrfConfigValue, osrfConfigValueNoEx
from osrf.net import osrfNetwork, osrfSetNetworkHandle
from osrf.stack import osrfPushStack
from osrf.log import *
from osrf.set import osrfLoadSettings
import sys


def osrfConnect(configFile, configContext):
    """ Connects to the opensrf network """

    if osrfGetNetworkHandle():
        ''' This thread already has a handle '''
        return

    # parse the config file
    configParser = osrfConfig(configFile, configContext)
    configParser.parseConfig()
    
    # set up logging
    osrfInitLog(
        osrfConfigValue('loglevel'), 
        osrfConfigValueNoEx('syslog'),
        osrfConfigValueNoEx('logfile'))

    # connect to the opensrf network
    network = osrfNetwork(
        host=osrfConfigValue('domains.domain'),
        port=osrfConfigValue('port'),
        username=osrfConfigValue('username'), 
        password=osrfConfigValue('passwd'))
    network.setRecvCallback(osrfPushStack)
    osrfSetNetworkHandle(network)
    network.connect()

    # load the domain-wide settings file
    osrfLoadSettings(osrfConfigValue('domains.domain'))





