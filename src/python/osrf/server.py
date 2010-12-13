# -----------------------------------------------------------------------
# Copyright (C) 2008-2010  Equinox Software, Inc.
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

import os, sys, threading, fcntl, socket, errno, signal, time
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
        self.max_requests = 0 # max child requests
        self.max_children = 0 # max num of child processes
        self.min_children = 0 # min num of child processes
        self.num_children = 0 # current num children
        self.osrf_handle = None # xmpp handle
        self.routers = [] # list of registered routers
        self.keepalive = 0 # how long to wait for subsequent, stateful requests
        self.active_list = [] # list of active children
        self.idle_list = [] # list of idle children
        self.pid_map = {} # map of pid -> child object for faster access

        # Global status socketpair.  All children relay their 
        # availability info to the parent through this socketpair. 
        self.read_status, self.write_status = socket.socketpair()
        self.read_status.setblocking(0)

    def load_app(self):
        settings = osrf.set.get('activeapps.%s' % self.service)
        

    def cleanup(self):
        ''' Closes management sockets, kills children, reaps children, exits '''

        osrf.log.log_info("server: shutting down...")
        self.cleanup_routers()

        self.read_status.shutdown(socket.SHUT_RDWR)
        self.write_status.shutdown(socket.SHUT_RDWR)
        self.read_status.close()
        self.write_status.close()

        for child in self.idle_list + self.active_list:
            child.read_data.shutdown(socket.SHUT_RDWR)
            child.write_data.shutdown(socket.SHUT_RDWR)
            child.read_data.close()
            child.write_data.close()
            os.kill(child.pid, signal.SIGKILL)

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
        self.osrf_handle = osrf.system.System.net_connect(
            resource = '%s_listener' % self.service,
            service = self.service
        )

        # clear the recv callback so inbound messages do not filter through the opensrf stack
        self.osrf_handle.receive_callback = None

        # connect to our listening routers
        self.register_routers()

        try:
            osrf.log.log_internal("server: entering main server loop...")

            while True: # main server loop

                self.reap_children()
                self.check_status()
                data = self.osrf_handle.recv(-1).to_xml()
                child = None

                if len(self.idle_list) > 0:
                    child = self.idle_list.pop() 
                    self.active_list.append(child)
                    osrf.log.log_internal("server: sending data to available child %d" % child.pid)

                elif self.num_children < self.max_children:
                    child = self.spawn_child(True)
                    osrf.log.log_internal("server: sending data to new child %d" % child.pid)

                else:
                    osrf.log.log_warn("server: no children available, waiting...")
                    child = self.check_status(True)

                self.write_child(child, data)

        except KeyboardInterrupt:
            osrf.log.log_info("server: exiting with keyboard interrupt")

        except Exception, e: 
            osrf.log.log_error("server: exiting with exception: %s" % e.message)

        finally:
            self.cleanup()
                

    def write_child(self, child, data):
        ''' Sends data to the child process '''

        try:
            child.write_data.sendall(data)

        except Exception, e:
            osrf.log.log_error("server: error sending data to child %d: %s" % (child.pid, str(e)))
            self.cleanup_child(child.pid, True)
            return False

        return True


    def check_status(self, wait=False):
        ''' Checks to see if any children have indicated they are done with 
            their current request.  If wait is true, wait indefinitely 
            for a child to be free. '''

        ret_child = None

        if wait:
            self.read_status.setblocking(1)

        while True:
            pid = None

            try:
                pid = self.read_status.recv(SIZE_PAD)

            except socket.error, e:
                if e.args[0] == errno.EAGAIN:
                    break # no data left to read in nonblocking mode

                osrf.log.log_error("server: child status check failed: %s" % str(e))
                if not wait or ret_child:
                    break

            finally:
                if wait and ret_child:
                    # we've received a status update from at least 
                    # 1 child.  No need to block anymore.
                    self.read_status.setblocking(0)

            if pid:
                child = self.pid_map[int(pid)]
                osrf.log.log_internal("server: child process %d reporting for duty" % child.pid)
                if wait and ret_child is None:
                    # caller is waiting for a free child, leave it in the active list
                    ret_child = child
                else:
                    self.active_list.remove(child)
                    self.idle_list.append(child)

        return ret_child
        

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

                osrf.log.log_internal("server: cleaning up child %d" % pid)
                self.num_children -= 1
                self.cleanup_child(pid)

            except OSError:
                return

    def cleanup_child(self, pid, kill=False):

        if kill:
            os.kill(pid, signal.SIGKILL)

        # locate the child in the active or idle list and remove it
        # Note: typically, a dead child will be in the active list, since 
        # exiting children do not send a cleanup status to the controller

        try:
            self.active_list.pop(self.active_list.index(self.pid_map[pid]))
        except:
            try:
                self.idle_list.pop(self.active_list.index(self.pid_map[pid]))
            except:
                pass

        del self.pid_map[pid]

            
        
    def spawn_children(self):
        ''' Launches up to min_children child processes '''
        while self.num_children < self.min_children:
            self.spawn_child()

    def spawn_child(self, active=False):
        ''' Spawns a new child process '''

        child = Child(self)
        child.read_data, child.write_data = socket.socketpair()
        child.pid = os.fork()

        if child.pid: # parent process
            self.num_children += 1
            self.pid_map[child.pid] = child
            if active:
                self.active_list.append(child)
            else:
                self.idle_list.append(child)
            osrf.log.log_internal("server: %s spawned child %d : %d total" % (self.service, child.pid, self.num_children))
            return child
        else:
            child.pid = os.getpid()
            child.init()
            child.run()
            osrf.net.get_network_handle().disconnect()
            osrf.log.log_internal("server: child exiting...")
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

        osrf.log.log_info("server: registering with router %s" % target)
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
            osrf.log.log_info("server: un-registering with router %s" % target)
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
        self.pid = 0 # my process id

    def run(self):
        ''' Loops, processing data, until max_requests is reached '''

        while True:

            try:

                self.read_data.setblocking(1)
                data = ''

                while True: # read all the data from the socket

                    buf = None
                    try:
                        buf = self.read_data.recv(2048)
                    except socket.error, e:
                        if e.args[0] == errno.EAGAIN:
                            break
                        osrf.log.log_error("server: child data read failed: %s" % str(e))
                        osrf.app.Application.application.child_exit()
                        return

                    if buf is None or buf == '':
                        break

                    data += buf
                    self.read_data.setblocking(0)

                osrf.log.log_internal("server: child received message: " + data)

                osrf.net.get_network_handle().flush_inbound_data()
                session = osrf.stack.push(osrf.net.NetworkMessage.from_xml(data))
                self.keepalive_loop(session)

                self.num_requests += 1

                osrf.log.log_internal("server: child done processing message")

                if self.num_requests == self.controller.max_requests:
                    break

                # tell the parent we're done w/ this request session
                self.send_status()

            except KeyboardInterrupt:
                pass

        # run the exit handler
        osrf.app.Application.application.child_exit()

    def keepalive_loop(self, session):
        keepalive = self.controller.keepalive

        while session.state == osrf.const.OSRF_APP_SESSION_CONNECTED:

            status = session.wait(keepalive)

            if session.state == osrf.const.OSRF_APP_SESSION_DISCONNECTED:
                osrf.log.log_internal("server: client sent disconnect, exiting keepalive")
                break

            if status is None: # no msg received before keepalive timeout expired

                osrf.log.log_info(
                    "server: no request was received in %d seconds from %s, exiting stateful session" % (
                    session.remote_id, int(keepalive)));

                session.send_status(
                    session.thread, 
                    osrf.net_obj.NetworkObject.osrfConnectStatus({   
                        'status' : 'Disconnected on timeout',
                        'statusCode': osrf.const.OSRF_STATUS_TIMEOUT
                    })
                )

                break

        session.cleanup()
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
        osrf.system.System.net_connect(
            resource = '%s_drone' % self.controller.service, 
            service = self.controller.service
        )
        osrf.app.Application.application.child_init()

