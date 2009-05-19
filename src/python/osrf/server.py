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

import os, sys, threading, logging, fcntl, socket, errno, signal, time
import osrf.log, osrf.conf, osrf.net, osrf.system, osrf.stack, osrf.app, osrf.const


# used to define the size of the PID/size leader in 
# status and data messages passed to and from children
SIZE_PAD = 12

class Controller(object):
    ''' 
        OpenSRF forking request server.  
    '''

    def __init__(self, service):
        self.service = service # service name
        self.application = None # the application we're serving
        self.max_requests = 0 # max child requests
        self.max_children = 0 # max num of child processes
        self.min_childen = 0 # min num of child processes
        self.num_children = 0 # current num children
        self.child_idx = 0 # current index into the children array
        self.children = [] # list of children
        self.osrf_handle = None # xmpp handle
        self.routers = [] # list of registered routers
        self.keepalive = 0 # how long to wait for subsequent, stateful requests

        # Global status socketpair.  All children relay their 
        # availability info to the parent through this socketpair. 
        self.read_status, self.write_status = socket.socketpair()

    def load_app(self):
        settings = osrf.set.get('activeapps.%s' % self.service)
        

    def cleanup(self):
        ''' Closes management sockets, kills children, reaps children, exits '''

        osrf.log.log_info("Shutting down...")
        self.cleanup_routers()

        self.read_status.shutdown(socket.SHUT_RDWR)
        self.write_status.shutdown(socket.SHUT_RDWR)
        self.read_status.close()
        self.write_status.close()

        for child in self.children:
            child.read_data.shutdown(socket.SHUT_RDWR)
            child.write_data.shutdown(socket.SHUT_RDWR)
            child.read_data.close()
            child.write_data.close()

        os.kill(0, signal.SIGKILL)
        self.reap_children(True)
        os._exit(0)


    def handle_signals(self):
        ''' Installs SIGINT and SIGTERM handlers '''
        def handler(signum, frame):
            self.cleanup()
        signal.signal(signal.SIGINT, handler)
        signal.signal(signal.SIGTERM, handler)


    def run(self):

        osrf.net.get_network_handle().disconnect()
        osrf.net.clear_network_handle()
        self.spawn_children()
        self.handle_signals()

        time.sleep(.5) # give children a chance to connect before we start taking data
        self.osrf_handle = osrf.system.System.net_connect(resource = '%s_listener' % self.service)

        # clear the recv callback so inbound messages do not filter through the opensrf stack
        self.osrf_handle.receive_callback = None

        # connect to our listening routers
        self.register_routers()

        try:
            osrf.log.log_debug("entering main server loop...")
            while True: # main server loop

                self.reap_children()
                self.check_status()
                data = self.osrf_handle.recv(-1).to_xml()

                if self.try_avail_child(data):
                    continue

                if self.try_new_child(data):
                    continue

                self.wait_for_child()

        except KeyboardInterrupt:
            self.cleanup()
        #except Exception, e: 
            #osrf.log.log_error("server exiting with exception: %s" % e.message)
            #self.cleanup()
                

    def try_avail_child(self, data):
        ''' Trys to send current request data to an available child process '''
        ctr = 0
        while ctr < self.num_children:

            if self.child_idx >= self.num_children:
                self.child_idx = 0
            child = self.children[self.child_idx]

            if child.available:
                osrf.log.log_internal("sending data to available child")
                self.write_child(child, data)
                return True

            ctr += 1
            self.child_idx += 1
        return False

    def try_new_child(self, data):
        ''' Tries to spawn a new child to send request data to '''
        if self.num_children < self.max_children:
            osrf.log.log_internal("spawning new child to handle data")
            child = self.spawn_child()
            self.write_child(child, data)
            return True
        return False

    def try_wait_child(self, data):
        ''' Waits for a child to become available '''
        osrf.log.log_warn("No children available, waiting...")
        child = self.check_status(True)
        self.write_child(child, data)


    def write_child(self, child, data):
        ''' Sends data to the child process '''
        child.available = False
        child.write_data.sendall(str(len(data)).rjust(SIZE_PAD) + data)
        self.child_idx += 1


    def check_status(self, block=False):
        ''' Checks to see if any children have indicated they are done with 
            their current request.  If block is true, this will wait 
            indefinitely for a child to be free. '''

        pid = None
        child = None
        if block:
            pid = self.read_status.recv(SIZE_PAD)
        else:
            try:
                self.read_status.setblocking(0)
                pid = self.read_status.recv(SIZE_PAD)
            except socket.error, e:
                if e.args[0] != errno.EAGAIN:
                    raise e
            self.read_status.setblocking(1)
                
        if pid:
            pid = int(pid)
            child = [c for c in self.children if c.pid == pid][0]
            child.available = True

        return child
        

    def reap_children(self, done=False):
        ''' Uses waitpid() to reap the children.  If necessary, new children are spawned '''
        options = 0
        if not done: 
            options = os.WNOHANG 

        while True:
            try:
                (pid, status) = os.waitpid(0, options)
                if pid == 0:
                    if not done:
                        self.spawn_children()
                    return
                osrf.log.log_debug("reaping child %d" % pid)
                self.num_children -= 1
                self.children = [c for c in self.children if c.pid != pid]
            except OSError:
                return
        
    def spawn_children(self):
        ''' Launches up to min_children child processes '''
        while self.num_children < self.min_children:
            self.spawn_child()

    def spawn_child(self):
        ''' Spawns a new child process '''

        child = Child(self)
        child.read_data, child.write_data = socket.socketpair()
        child.pid = os.fork()

        if child.pid:
            self.num_children += 1
            self.children.append(child)
            osrf.log.log_debug("spawned child %d : %d total" % (child.pid, self.num_children))
            return child
        else:
            child.pid = os.getpid()
            child.init()
            child.run()
            osrf.net.get_network_handle().disconnect()
            osrf.log.log_internal("child exiting...")
            os._exit(0)

    def register_routers(self):
        ''' Registers this application instance with all configured routers '''
        routers = osrf.conf.get('routers.router')

        if not isinstance(routers, list):
            routers = [routers]

        for router in routers:
            if isinstance(router, dict):
                if not 'services' in router or \
                        self.service in router['services']['service']:
                    target = "%s@%s/router" % (router['name'], router['domain'])
                    self.register_router(target)
            else:
                router_name = osrf.conf.get('router_name')
                target = "%s@%s/router" % (router_name, router)
                self.register_router(target)


    def register_router(self, target):
        ''' Registers with a single router '''
        osrf.log.log_info("registering with router %s" % target)
        self.routers.append(target)

        reg_msg = osrf.net.NetworkMessage(
            recipient = target,
            body = 'registering...',
            router_command = 'register',
            router_class = self.service
        )

        self.osrf_handle.send(reg_msg)

    def cleanup_routers(self):
        ''' Un-registers with all connected routers '''
        for target in self.routers:
            osrf.log.log_info("un-registering with router %s" % target)
            unreg_msg = osrf.net.NetworkMessage(
                recipient = target,
                body = 'un-registering...',
                router_command = 'unregister',
                router_class = self.service
            )
            self.osrf_handle.send(unreg_msg)
        

class Child(object):
    ''' Models a single child process '''

    def __init__(self, controller):
        self.controller = controller # our Controller object
        self.num_requests = 0 # how many requests we've served so far
        self.read_data = None # the child reads data from the controller on this socket
        self.write_data = None # the controller sends data to the child on this socket 
        self.available = True # true if this child is not currently serving a request
        self.pid = 0 # my process id


    def run(self):
        ''' Loops, processing data, until max_requests is reached '''
        while True:
            try:
                size = int(self.read_data.recv(SIZE_PAD) or 0)
                data = self.read_data.recv(size)
                osrf.log.log_internal("recv'd data " + data)
                session = osrf.stack.push(osrf.net.NetworkMessage.from_xml(data))
                self.keepalive_loop(session)
                self.num_requests += 1
                if self.num_requests == self.controller.max_requests:
                    break
                self.send_status()
            except KeyboardInterrupt:
                pass
        # run the exit handler
        osrf.app.Application.application.child_exit()

    def keepalive_loop(self, session):
        keepalive = self.controller.keepalive

        while session.state == osrf.const.OSRF_APP_SESSION_CONNECTED:

            start = time.time()
            session.wait(keepalive)
            end = time.time()

            if session.state == osrf.const.OSRF_APP_SESSION_DISCONNECTED:
                osrf.log.log_internal("client sent disconnect, exiting keepalive")
                break

            if (end - start) >= keepalive: # exceeded keepalive timeout

                osrf.log.log_info("No request was received in %d seconds, exiting stateful session" % int(keepalive));

                session.send_status(
                    session.thread, 
                    osrf.net_obj.NetworkObject.osrfConnectStatus({   
                        'status' : 'Disconnected on timeout',
                        'statusCode': osrf.const.OSRF_STATUS_TIMEOUT
                    })
                )

                break

        return

    def send_status(self):
        ''' Informs the controller that we are done processing this request '''
        fcntl.lockf(self.controller.write_status.fileno(), fcntl.LOCK_EX)
        try:
            self.controller.write_status.sendall(str(self.pid).rjust(SIZE_PAD))
        finally:
            fcntl.lockf(self.controller.write_status.fileno(), fcntl.LOCK_UN)

    def init(self):
        ''' Connects the opensrf xmpp handle '''
        osrf.net.clear_network_handle()
        osrf.system.System.net_connect(resource = '%s_drone' % self.controller.service)
        osrf.app.Application.application.child_init()

