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

import osrf.conf
from osrf.net import Network, set_network_handle, get_network_handle
import osrf.stack, osrf.log, osrf.set, osrf.cache
import sys, os

class System(object):

    config_file = None
    config_context = None

    @staticmethod
    def net_connect(**kwargs):
        if get_network_handle():
            ''' This thread already has a handle '''
            return

        config_file = kwargs.get('config_file') or System.config_file
        config_context = kwargs.get('config_context') or System.config_context

        # store the last config file info for later
        System.config_file = config_file
        System.config_context = config_context

        # parse the config file
        config_parser = osrf.conf.Config(config_file, config_context)
        config_parser.parse_config()

        # set up logging
        osrf.log.initialize(
            osrf.conf.get('loglevel'), 
            osrf.conf.get_no_ex('syslog'),
            osrf.conf.get_no_ex('logfile'),
            osrf.conf.get_no_ex('client') == 'true',
            kwargs.get('service'))

        # connect to the opensrf network
        network = Network(
            host = osrf.conf.get('domain'),
            port = osrf.conf.get('port'),
            username = osrf.conf.get('username'), 
            password = osrf.conf.get('passwd'),
            resource = kwargs.get('resource'))

        network.set_receive_callback(osrf.stack.push)
        osrf.net.set_network_handle(network)
        network.connect()

        return network


    @staticmethod
    def connect(**kwargs):
        """ Connects to the opensrf network 
            Options:
                config_file
                config_context
                connect_cache
                resource
        """

        network = System.net_connect(**kwargs)

        # load the domain-wide settings file
        osrf.set.load(osrf.conf.get('domain'))

        if kwargs.get('connect_cache'):
            System.connect_cache()

        return network


    @staticmethod
    def connect_cache():
        ''' Initializes the cache connections '''
        cache_servers = osrf.set.get('cache.global.servers.server')
        if cache_servers:
            if not isinstance(cache_servers, list):
                cache_servers = [cache_servers]
            if not osrf.cache.CacheClient.get_client():
                osrf.cache.CacheClient.connect(cache_servers)

    ''' 
    @return 0 if child, pid if parent
    '''
    @staticmethod
    def daemonize(parentExit=True):
        pid = os.fork() 
        if pid == 0:
            os.chdir('/')
            os.setsid()
            sys.stdin.close()
            sys.stdout.close()
            sys.stderr.close()
        elif parentExit:
            os._exit(0)

        return pid


