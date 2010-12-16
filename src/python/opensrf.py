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
import osrf.system, osrf.server, osrf.app, osrf.set, osrf.json

def do_help():
    print '''
    Manage OpenSRF application processes

    Options:
        -a <action>
            start   -- Start a service
            stop    -- stop a service
            restart -- restart a service
            start_all -- Start all services
            stop_all -- Stop all services
            restart_all -- Restart all services

        -s <service>
            The service name

        -f <config file>
            The OpenSRF config file

        -c <config context>
            The OpenSRF config file context

        -p <PID dir>
            The location of application PID files.  Default is /tmp

        -d 
            If set, run in daemon (background) mode.  This creates a PID 
            file for managing the process.

        -l
            If set, run in 'localhost' mode

        -h
            Prints help message
    '''
    sys.exit(0)


# Parse the command line options
ops, args = None, None
try:
    ops, args = getopt.getopt(sys.argv[1:], 'a:s:f:c:p:dhl')
except getopt.GetoptError, e:
    print '* %s' % str(e)
    do_help()

options = dict(ops)

if '-a' not in options or '-f' not in options:
    do_help()

action = options['-a']
config_file = options['-f']
pid_dir = options['-p']

service = options.get('-s')
config_ctx = options.get('-c', 'config.opensrf')
as_localhost = '-l' in options
as_daemon = '-d' in options

domain = None
settings = None
services = {}


def get_pid_file(service):
    return "%s/%s.pid" % (pid_dir, service)

def do_init():
    global domain
    global settings

    # connect to the OpenSRF network
    osrf.system.System.net_connect(
        config_file = config_file, config_context = config_ctx)

    if as_localhost:
        domain = 'localhost'
    else:
        domain = osrf.conf.get('domain')

    osrf.set.load(domain)

    settings = osrf.set.get('apps')

    for key in settings.keys():
        svc = settings[key]
        if isinstance(svc, dict) and svc['language'] == 'python':
            services[key] = svc


def do_start(service):

    pidfile = get_pid_file(service)

    if service not in services:
        print "* service %s is not a 'python' application" % service
        return

    if os.path.exists(pidfile):
        print "* service %s already running" % service
        return

    print "* starting %s" % service

    if as_daemon:

        if osrf.system.System.daemonize(False):
            return # parent process returns

        # write PID file
        file = open(pidfile, 'w')
        file.write(str(os.getpid()))
        file.close()

    settings = services[service];

    osrf.app.Application.load(service, settings['implementation'])
    osrf.app.Application.register_sysmethods()
    osrf.app.Application.application.global_init()

    controller = osrf.server.Controller(service)
    controller.max_requests = settings['unix_config']['max_requests']
    controller.max_children = settings['unix_config']['max_children']
    controller.min_children = settings['unix_config']['min_children']
    controller.keepalive = settings['keepalive']

    controller.run()
    os._exit(0)

def do_start_all():
    print "* starting all services for %s " % domain
    for service in services.keys():
        do_start(service)

def do_stop_all():
    print "* stopping all services for %s " % domain
    for service in services.keys():
        do_stop(service)

def do_stop(service):
    pidfile = get_pid_file(service)

    if not os.path.exists(pidfile):
        print "* %s is not running" % service
        return

    print "* stopping %s" % service

    file = open(pidfile)
    pid = file.read()
    file.close()
    try:
        os.kill(int(pid), signal.SIGTERM)
    except:
        pass
    os.remove(pidfile)

# -----------------------------------------------------

do_init()

if action == 'start':
    do_start(service)

elif action == 'stop':
    do_stop(service)

elif action == 'restart':
    do_stop(service)
    do_start(service)

elif action == 'start_all':
    do_start_all()

elif action == 'stop_all':
    do_stop_all()

elif action == 'restart_all':
    do_stop_all()
    do_start_all()

elif action == 'help':
    do_help()