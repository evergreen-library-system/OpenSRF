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

import osrf.json
import osrf.conf
import osrf.log
import osrf.net
import osrf.net_obj
from osrf.const import OSRF_APP_SESSION_CONNECTED, \
    OSRF_APP_SESSION_CONNECTING, OSRF_APP_SESSION_DISCONNECTED, \
    OSRF_MESSAGE_TYPE_CONNECT, OSRF_MESSAGE_TYPE_DISCONNECT, \
    OSRF_MESSAGE_TYPE_REQUEST
import osrf.ex
import random, os, time, threading


# -----------------------------------------------------------------------
# Go ahead and register the common network objects
# -----------------------------------------------------------------------
osrf.net_obj.NetworkRegisterHint('osrfMessage', ['threadTrace', 'type', 'payload'], 'hash')
osrf.net_obj.NetworkRegisterHint('osrfMethod', ['method', 'params'], 'hash')
osrf.net_obj.NetworkRegisterHint('osrfResult', ['status', 'statusCode', 'content'], 'hash')
osrf.net_obj.NetworkRegisterHint('osrfConnectStatus', ['status', 'statusCode'], 'hash')
osrf.net_obj.NetworkRegisterHint('osrfMethodException', ['status', 'statusCode'], 'hash')


class Session(object):
    """Abstract session superclass."""

    ''' Global cache of in-service sessions '''
    session_cache = {}

    def __init__(self):
        # by default, we're connected to no one
        self.state = OSRF_APP_SESSION_DISCONNECTED
        self.remote_id = None

    def find_session(threadTrace):
        return Session.session_cache.get(threadTrace)
    find_session = staticmethod(find_session)

    def wait(self, timeout=120):
        """Wait up to <timeout> seconds for data to arrive on the network"""
        osrf.log.osrfLogInternal("Session.wait(%d)" % timeout)
        handle = osrf.net.get_network_handle()
        handle.recv(timeout)

    def send(self, omessage):
        """Sends an OpenSRF message"""
        net_msg = osrf.net.NetworkMessage(
            recipient      = self.remote_id,
            body    = osrf.json.to_json([omessage]),
            thread = self.thread )

        handle = osrf.net.get_network_handle()
        handle.send(net_msg)

    def cleanup(self):
        """Removes the session from the global session cache."""
        del Session.session_cache[self.thread]

class ClientSession(Session):
    """Client session object.  Use this to make server requests."""

    def __init__(self, service):
        
        # call superclass constructor
        Session.__init__(self)

        # the remote service we want to make requests of
        self.service = service

        # find the remote service handle <router>@<domain>/<service>
        domain = osrf.conf.get('domains.domain', 0)
        router = osrf.conf.get('router_name')
        self.remote_id = "%s@%s/%s" % (router, domain, service)
        self.orig_remote_id = self.remote_id

        # generate a random message thread
        self.thread = "%s%s%s%s" % (os.getpid(), 
            str(random.randint(100,100000)), str(time.time()),threading.currentThread().getName().lower())

        # how many requests this session has taken part in
        self.next_id = 0 

        # cache of request objects 
        self.requests = {}

        # cache this session in the global session cache
        Session.session_cache[self.thread] = self

    def reset_request_timeout(self, rid):
        req = self.find_request(rid)
        if req:
            req.reset_timeout = True
            

    def request2(self, method, arr):
        """Creates a new request and sends the request to the server using a python array as the params."""
        return self.__request(method, arr)

    def request(self, method, *args):
        """Creates a new request and sends the request to the server using a variable argument list as params"""
        arr = list(args)
        return self.__request(method, arr)

    def __request(self, method, arr):
        """Builds the request object and sends it."""
        if self.state != OSRF_APP_SESSION_CONNECTED:
            self.reset_remote_id()

        osrf.log.logDebug("Sending request %s -> %s " % (self.service, method))
        req = Request(self, self.next_id, method, arr)
        self.requests[str(self.next_id)] = req
        self.next_id += 1
        req.send()
        return req


    def connect(self, timeout=10):
        """Connects to a remote service"""

        if self.state == OSRF_APP_SESSION_CONNECTED:
            return True
        self.state == OSRF_APP_SESSION_CONNECTING

        # construct and send a CONNECT message
        self.send(
            osrf.net_obj.NetworkObject.osrfMessage( 
                {   'threadTrace' : 0,
                    'type' : OSRF_MESSAGE_TYPE_CONNECT
                } 
            )
        )

        while timeout >= 0 and not self.state == OSRF_APP_SESSION_CONNECTED:
            start = time.time()
            self.wait(timeout)
            timeout -= time.time() - start
        
        if self.state != OSRF_APP_SESSION_CONNECTED:
            raise osrf.ex.OSRFServiceException("Unable to connect to " + self.service)
        
        return True

    def disconnect(self):
        """Disconnects from a remote service"""

        if self.state == OSRF_APP_SESSION_DISCONNECTED:
            return True

        self.send(
            osrf.net_obj.NetworkObject.osrfMessage( 
                {   'threadTrace' : 0,
                    'type' : OSRF_MESSAGE_TYPE_DISCONNECT
                } 
            )
        )

        self.state = OSRF_APP_SESSION_DISCONNECTED

    
    def set_remote_id(self, remoteid):
        self.remote_id = remoteid
        osrf.log.osrfLogInternal("Setting request remote ID to %s" % self.remote_id)

    def reset_remote_id(self):
        """Recovers the original remote id"""
        self.remote_id = self.orig_remote_id
        osrf.log.osrfLogInternal("Resetting remote ID to %s" % self.remote_id)

    def push_response_queue(self, message):
        """Pushes the message payload onto the response queue 
            for the request associated with the message's ID."""
        osrf.log.logDebug("pushing %s" % message.payload())
        try:
            self.find_request(message.threadTrace()).pushResponse(message.payload())
        except Exception, e: 
            osrf.log.osrfLogWarn("pushing respond to non-existent request %s : %s" % (message.threadTrace(), e))

    def find_request(self, rid):
        """Returns the original request matching this message's threadTrace."""
        try:
            return self.requests[str(rid)]
        except KeyError:
            osrf.log.logDebug('find_request(): non-existent request %s' % str(rid))
            return None



class Request(object):
    """Represents a single OpenSRF request.
        A request is made and any resulting respones are 
        collected for the client."""

    def __init__(self, session, rid, method=None, params=[]):

        self.session = session # my session handle
        self.rid     = rid # my unique request ID
        self.method = method # method name
        self.params = params # my method params
        self.queue  = [] # response queue
        self.reset_timeout = False # resets the recv timeout?
        self.complete = False # has the server told us this request is done?
        self.send_time = 0 # local time the request was put on the wire
        self.complete_time =  0 # time the server told us the request was completed
        self.first_response_time = 0 # time it took for our first reponse to be received

    def send(self):
        """Sends a request message"""

        # construct the method object message with params and method name
        method = osrf.net_obj.NetworkObject.osrfMethod( {
            'method' : self.method,
            'params' : self.params
        } )

        # construct the osrf message with our method message embedded
        message = osrf.net_obj.NetworkObject.osrfMessage( {
            'threadTrace' : self.rid,
            'type' : OSRF_MESSAGE_TYPE_REQUEST,
            'payload' : method
        } )

        self.send_time = time.time()
        self.session.send(message)

    def recv(self, timeout=120):
        """Waits up to <timeout> seconds for a response to this request.
        
            If a message is received in time, the response message is returned.
            Returns None otherwise."""

        self.session.wait(0)

        orig_timeout = timeout
        while not self.complete and timeout >= 0 and len(self.queue) == 0:
            s = time.time()
            self.session.wait(timeout)
            timeout -= time.time() - s
            if self.reset_timeout:
                self.reset_timeout = False
                timeout = orig_timeout

        now = time.time()

        # -----------------------------------------------------------------
        # log some statistics 
        if len(self.queue) > 0:
            if not self.first_response_time:
                self.first_response_time = now
                osrf.log.logDebug("time elapsed before first response: %f" \
                    % (self.first_response_time - self.send_time))

        if self.complete:
            if not self.complete_time:
                self.complete_time = now
                osrf.log.logDebug("time elapsed before complete: %f" \
                    % (self.complete_time - self.send_time))
        # -----------------------------------------------------------------


        if len(self.queue) > 0:
            # we have a reponse, return it
            return self.queue.pop(0)

        return None

    def pushResponse(self, content):
        """Pushes a method response onto this requests response queue."""
        self.queue.append(content)

    def cleanup(self):
        """Cleans up request data from the cache. 

            Do this when you are done with a request to prevent "leaked" cache memory."""
        del self.session.requests[str(self.rid)]

    def set_complete(self):
        """Sets me as complete.  This means the server has sent a 'request complete' message"""
        self.complete = True


class ServerSession(Session):
    """Implements a server-side session"""
    pass


def AtomicRequest(service, method, *args):
    ses = ClientSession(service)
    req = ses.request2(method, list(args))
    resp = req.recv()
    data = resp.content()
    req.cleanup()
    ses.cleanup()
    return data



