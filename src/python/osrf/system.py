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

from osrf.conf import Config, get, get_no_ex
from osrf.net import Network, set_network_handle, get_network_handle
import osrf.stack, osrf.log, osrf.set, osrf.cache
import sys


def connect(configFile, configContext, init_cache=False):
    """ Connects to the opensrf network 
        @param configFile The OpenSRF config
        @param configContext The path to the configuration element in the XML config.
            e.g. 'config.opensrf'
        @param init_cache If true, connect to the cache servers
    """

    if get_network_handle():
        ''' This thread already has a handle '''
        return

    # parse the config file
    configParser = Config(configFile, configContext)
    configParser.parseConfig()
    
    # set up logging
    osrf.log.initialize(
        osrf.conf.get('loglevel'), 
        osrf.conf.get_no_ex('syslog'),
        osrf.conf.get_no_ex('logfile'))

    # connect to the opensrf network
    network = Network(
        host = osrf.conf.get('domains.domain'),
        port = osrf.conf.get('port'),
        username = osrf.conf.get('username'), 
        password = osrf.conf.get('passwd'))

    network.set_receive_callback(osrf.stack.push)
    osrf.net.set_network_handle(network)
    network.connect()

    # load the domain-wide settings file
    osrf.set.load(osrf.conf.get('domains.domain'))

    if init_cache:
        connect_cache()


def connect_cache():
    ''' Initializes the cache connections '''
    cache_servers = osrf.set.get('cache.global.servers.server')
    if cache_servers:
        if not isinstance(cache_servers, list):
            cache_servers = [cache_servers]
        if not osrf.cache.CacheClient.get_client():
            osrf.cache.CacheClient.connect(cache_servers)


