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


from pyxmpp.jabber.client import JabberClient
from pyxmpp.message import Message
from pyxmpp.jid import JID
from socket import gethostname
import osrf.log
import os, time, threading

THREAD_SESSIONS = {}

# - log jabber activity (for future reference)
#import logging
#logger=logging.getLogger()
#logger.addHandler(logging.StreamHandler())
#logger.addHandler(logging.FileHandler('j.log'))
#logger.setLevel(logging.DEBUG)

def set_network_handle(handle):
    """ Sets the thread-specific network handle"""
    THREAD_SESSIONS[threading.currentThread().getName()] = handle

def get_network_handle():
    """ Returns the thread-specific network connection handle."""
    return THREAD_SESSIONS.get(threading.currentThread().getName())


class NetworkMessage(object):
    """Network message

    attributes:

    sender - message sender
    recipient - message recipient
    body - the body of the message
    thread - the message thread
    locale - locale of the message
    """

    def __init__(self, message=None, **args):
        if message:
            self.body = message.get_body()
            self.thread = message.get_thread()
            self.recipient = message.get_to()
            if message.xmlnode.hasProp('router_from') and \
                message.xmlnode.prop('router_from') != '':
                self.sender = message.xmlnode.prop('router_from')
            else:
                self.sender = message.get_from().as_utf8()
        else:
            if args.has_key('sender'):
                self.sender = args['sender']
            if args.has_key('recipient'):
                self.recipient = args['recipient']
            if args.has_key('body'):
                self.body = args['body']
            if args.has_key('thread'):
                self.thread = args['thread']

class Network(JabberClient):
    def __init__(self, **args):
        self.isconnected = False

        # Create a unique jabber resource
        resource = 'python'
        if args.has_key('resource'):
            resource = args['resource']
        resource += '_' + gethostname() + ':' + str(os.getpid()) + '_' + \
            threading.currentThread().getName().lower()
        self.jid = JID(args['username'], args['host'], resource)

        osrf.log.logDebug("initializing network with JID %s and host=%s, "
            "port=%s, username=%s" % (self.jid.as_utf8(), args['host'], \
            args['port'], args['username']))

        #initialize the superclass
        JabberClient.__init__(self, self.jid, args['password'], args['host'])
        self.queue = []

        self.receive_callback = None

    def connect(self):
        JabberClient.connect(self)
        while not self.isconnected:
            stream = self.get_stream()
            act = stream.loop_iter(10)
            if not act:
                self.idle()

    def set_receive_callback(self, func):
        """The callback provided is called when a message is received.
        
            The only argument to the function is the received message. """
        self.receive_callback = func

    def session_started(self):
        osrf.log.log_info("Successfully connected to the opensrf network")
        self.authenticated()
        self.stream.set_message_handler("normal", self.message_received)
        self.isconnected = True

    def send(self, message):
        """Sends the provided network message."""
        osrf.log.log_internal("jabber sending to %s: %s" % \
            (message.recipient, message.body))
        msg = Message(None, None, message.recipient, None, None, None, \
            message.body, message.thread)
        self.stream.send(msg)
    
    def message_received(self, stanza):
        """Handler for received messages."""
        if stanza.get_type()=="headline":
            return True
        # check for errors
        osrf.log.log_internal("jabber received message from %s : %s" 
            % (stanza.get_from().as_utf8(), stanza.get_body()))
        self.queue.append(NetworkMessage(stanza))
        return True

    def recv(self, timeout=120):
        """Attempts to receive a message from the network.

        timeout - max number of seconds to wait for a message.  
        If a message is received in 'timeout' seconds, the message is passed to 
        the receive_callback is called and True is returned.  Otherwise, false is
        returned.
        """

        if len(self.queue) == 0:
            while timeout >= 0 and len(self.queue) == 0:
                starttime = time.time()
                act = self.get_stream().loop_iter(timeout)
                endtime = time.time() - starttime
                timeout -= endtime
                osrf.log.log_internal("exiting stream loop after %s seconds. "
                    "act=%s, queue size=%d" % (str(endtime), act, len(self.queue)))
                if not act:
                    self.idle()

        # if we've acquired a message, handle it
        msg = None
        if len(self.queue) > 0:
            msg = self.queue.pop(0)
            if self.receive_callback:
                self.receive_callback(msg)

        return msg



