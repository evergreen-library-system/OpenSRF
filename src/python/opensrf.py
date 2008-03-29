#!/usr/bin/python
# -----------------------------------------------------------------------
# Copyright (C) 2008  Equinox Software, Inc.
# Bill Erickson <erickson@esilibrary.com>
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
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA
# -----------------------------------------------------------------------

import sys, getopt, os, signal
import osrf.system, osrf.server, osrf.app

def help():
    print '''
    Manage one or more an OpenSRF applications

    Options:
        -a <action>
            start   -- Start a service
            stop    -- stop a service
            restart -- restart a service

        -s <service>
            The service name

        -f <config file>
            The OpenSRF config file

        -c <config context>
            The OpenSRF config file context

        -p <PID dir>
            The location of application PID files.  Default is /tmp

        -d 
            If set, run in daemon (background) mode
    '''
    sys.exit(0)


# Parse the command line options
ops, args = getopt.getopt(sys.argv[1:], 'a:s:f:c:p:d')
options = dict(ops)

if '-a' not in options or '-s' not in options or '-f' not in options:
    help()

action = options['-a']
service = options['-s']
config_file = options['-f']
config_ctx = options.get('-c', 'opensrf')
pid_dir = options.get('-p', '/tmp')
as_daemon = '-d' in options
pidfile = "%s/osrf_py_%s.pid" % (pid_dir, service)


if action == 'start':

    # connect to the OpenSRF network
    osrf.system.System.net_connect(
        config_file = config_file, config_context = config_ctx)

    # XXX load the settings configs...
    osrf.app.Application.load(service, 'osrf.apps.example') # XXX example only for now
    osrf.app.Application.register_sysmethods()
    osrf.app.Application.application.global_init()

    controller = osrf.server.Controller(service)
    controller.max_requests = 10
    controller.max_children = 6
    controller.min_children = 3

    if as_daemon:
        osrf.system.System.daemonize()
        file = open(pidfile, 'w')
        file.write(str(os.getpid()))
        file.close()

    controller.run()

elif action == 'stop':
    file = open(pidfile)
    pid = file.read()
    file.close()
    os.kill(int(pid), signal.SIGTERM)
    os.remove(pidfile)


