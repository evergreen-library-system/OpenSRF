import sys, os, time, md5, random
from mod_python import apache, util
import osrf.system, osrf.cache, osrf.json, osrf.conf, osrf.net, osrf.log
from osrf.const import OSRF_MESSAGE_TYPE_DISCONNECT, OSRF_MESSAGE_TYPE_CONNECT, \
    OSRF_STATUS_CONTINUE, OSRF_STATUS_TIMEOUT, OSRF_MESSAGE_TYPE_STATUS


''' 
Proof of concept OpenSRF-HTTP multipart streaming gateway.

Example Apache mod_python config:

<Location /osrf-http-translator>
   SetHandler mod_python
   PythonPath "['/path/to/osrf-python'] + sys.path"
   PythonHandler osrf.http_translator
   PythonOption OSRF_CONFIG /path/to/opensrf_core.xml
   PythonOption OSRF_CONFIG_CONTEXT config.gateway
   PythonOption OSRF_CACHE_SERVERS 127.0.0.1:11211
   # testing only
   PythonAutoReload On
</Location>
'''

OSRF_HTTP_HEADER_TO = 'X-OpenSRF-to'
OSRF_HTTP_HEADER_XID = 'X-OpenSRF-xid'
OSRF_HTTP_HEADER_FROM = 'X-OpenSRF-from'
OSRF_HTTP_HEADER_THREAD = 'X-OpenSRF-thread'
OSRF_HTTP_HEADER_TIMEOUT = 'X-OpenSRF-timeout'
OSRF_HTTP_HEADER_SERVICE = 'X-OpenSRF-service'
OSRF_HTTP_HEADER_MULTIPART = 'X-OpenSRF-multipart'
MULTIPART_CONTENT_TYPE = 'multipart/x-mixed-replace;boundary="%s"'
JSON_CONTENT_TYPE = 'text/plain'
CACHE_TIME = 300

ROUTER_NAME = None
OSRF_DOMAIN = None

# If DEBUG_WRITE = True, all data sent to the client is also written
# to stderr (apache error log)
DEBUG_WRITE = False

def _dbg(msg):
    ''' testing only '''
    sys.stderr.write("%s\n\n" % str(msg))
    sys.stderr.flush()


INIT_COMPLETE = False
def child_init(req):
    ''' At time of writing, mod_python doesn't support a child_init handler,
        so this function is called once per process to initialize 
        the opensrf connection '''

    global INIT_COMPLETE, ROUTER_NAME, OSRF_DOMAIN
    if INIT_COMPLETE: 
        return

    # Apache complains with: UnboundLocalError: local variable 'osrf' referenced before assignment
    # if the following import line is removed, even though its also at the top of the file...
    import osrf.system 

    ops = req.get_options()
    conf = ops['OSRF_CONFIG']
    ctxt = ops.get('OSRF_CONFIG_CONTEXT') or 'opensrf'
    osrf.system.System.net_connect(config_file=conf, config_context=ctxt)

    ROUTER_NAME = osrf.conf.get('router_name')
    OSRF_DOMAIN = osrf.conf.get('domain')
    INIT_COMPLETE = True

    servers = ops.get('OSRF_CACHE_SERVERS')
    if servers:
        servers = servers.split(',')
    else:
        # no cache servers configured, see if we can talk to the settings server
        import osrf.set
        servers = osrf.set.get('cache.global.servers.server')
        if not isinstance(servers, list):
            servers = [servers]

    osrf.cache.CacheClient.connect(servers)


def handler(req):
    ''' Create the translator and tell it to process the request. '''
    child_init(req)
    translator = HTTPTranslator(req)
    status = translator.process()
    osrf.log.log_debug("translator call resulted in status %d" % int(status))
    if translator.local_xid:
        osrf.log.clear_xid()
    return status

class HTTPTranslator(object):
    def __init__(self, apreq):

        self.apreq = apreq

        if OSRF_HTTP_HEADER_XID in apreq.headers_in:
            osrf.log.log_debug('read XID from client %s' % apreq.headers_in.get(OSRF_HTTP_HEADER_XID))
            osrf.log.set_xid(apreq.headers_in.get(OSRF_HTTP_HEADER_XID))
            self.local_xid = False
        else:
            osrf.log.make_xid()
            osrf.log.log_debug('created new XID %s' % osrf.log.get_xid())
            self.local_xid = True

        if apreq.header_only: 
            return

        for k,v in apreq.headers_in.iteritems():
            osrf.log.log_internal('HEADER: %s = %s' % (k, v))

        try:
            #post = util.parse_qsl(apreq.read(int(apreq.headers_in['Content-length'])))
            post = util.parse_qsl(apreq.read())
            osrf.log.log_debug('post = ' + str(post))
            self.body = [d for d in post if d[0] == 'osrf-msg'][0][1]
            osrf.log.log_debug(self.body)
        except Exception, e: 
            osrf.log.log_warn("error parsing osrf message: %s" % unicode(e))
            self.body = None
            return

        self.messages = []
        self.complete = False
        self.handle = osrf.net.get_network_handle()
        self.handle.set_receive_callback(None)

        self.recipient = apreq.headers_in.get(OSRF_HTTP_HEADER_TO)
        self.service = apreq.headers_in.get(OSRF_HTTP_HEADER_SERVICE)
        self.thread = apreq.headers_in.get(OSRF_HTTP_HEADER_THREAD) or \
            "%s%s" % (os.getpid(), time.time())
        self.timeout = apreq.headers_in.get(OSRF_HTTP_HEADER_TIMEOUT) or 1200
        self.multipart = str(
            apreq.headers_in.get(OSRF_HTTP_HEADER_MULTIPART)).lower() == 'true'
        self.connect_only = False
        self.disconnect_only = False

        # generate a random multipart delimiter
        mpart = md5.new()
        mpart.update("%f%d%d" % (time.time(), os.getpid(), \
            random.randint(100, 10000000)))
        self.delim = mpart.hexdigest()
        self.remote_host = self.apreq.get_remote_host(apache.REMOTE_NOLOOKUP)
        self.cache = osrf.cache.CacheClient()



    def process(self):

        if self.apreq.header_only: 
            return apache.OK
        if not self.body:
            return apache.HTTP_BAD_REQUEST
        if not self.set_to_addr():
            return apache.HTTP_BAD_REQUEST
        if not self.parse_request():
            return apache.HTTP_BAD_REQUEST

        while self.handle.recv(0):
            pass # drop stale messages


        net_msg = osrf.net.NetworkMessage(
            recipient=self.recipient, thread=self.thread, body=self.body)
        self.handle.send(net_msg)

        if self.disconnect_only:
            osrf.log.log_debug("exiting early on DISCONNECT")
            return apache.OK

        first_write = True
        while not self.complete:

            net_msg = None
            try:
                net_msg = self.handle.recv(self.timeout)
            except osrf.net.XMPPNoRecipient:
                return apache.HTTP_NOT_FOUND 

            if not net_msg: 
                return apache.GATEWAY_TIME_OUT

            if not self.check_status(net_msg):
                continue 

            if first_write:
                self.init_headers(net_msg)
                first_write = False

            if self.multipart:
                self.respond_chunk(net_msg)
                if self.connect_only:
                    break
            else:
                self.messages.append(net_msg.body)

                # condense the sets of arrays into a single array of messages
                if self.complete or self.connect_only:
                    json = self.messages.pop(0)
                    while len(self.messages) > 0:
                        msg = self.messages.pop(0)
                        json = "%s,%s" % (json[0:len(json)-1], msg[1:])
                        
                    self.write("%s" % json)


        return apache.OK

    def parse_request(self):
        '''
        If this is solely a DISCONNECT message, we set self.disconnect_only
        to true
        @return True if the body parses correctly, False otherwise
        '''
        osrf_msgs = osrf.json.to_object(self.body)
        if not osrf_msgs:
            return False
        
        if len(osrf_msgs) == 1:
            if osrf_msgs[0].type() == OSRF_MESSAGE_TYPE_CONNECT:
                self.connect_only = True
            elif osrf_msgs[0].type() == OSRF_MESSAGE_TYPE_DISCONNECT:
                self.disconnect_only = True

        return True


    def set_to_addr(self):
        ''' Determines the TO address.  Returns false if 
            the address is missing or ambiguous. 
            Also returns false if an explicit TO is specified and the
            thread/IP/TO combination is not found in the session cache
            '''
        if self.service:
            if self.recipient:
                osrf.log.log_warn("specifying both SERVICE and TO is not allowed")
                return False
            self.recipient = "%s@%s/%s" % \
                (ROUTER_NAME, OSRF_DOMAIN, self.service)
            osrf.log.log_debug("set service to %s" % self.recipient)
            return True
        else:
            if self.recipient:
                # If the client specifies a specific TO address, verify it's
                # the same address that was cached with the previous request.  
                obj = self.cache.get(self.thread)
                if obj and obj['ip'] == self.remote_host and \
                    obj['jid'] == self.recipient:
                    return True
        osrf.log.log_warn("client [%s] attempted to send directly "
            "[%s] without a session" % (self.remote_host, self.recipient))
        return False

        
    def init_headers(self, net_msg):
        osrf.log.log_debug("initializing request headers")
        self.apreq.headers_out[OSRF_HTTP_HEADER_FROM] = net_msg.sender
        self.apreq.headers_out[OSRF_HTTP_HEADER_THREAD] = self.thread
        if self.multipart:
            self.apreq.content_type = MULTIPART_CONTENT_TYPE % self.delim
            self.write("--%s\n" % self.delim)
        else:
            self.apreq.content_type = JSON_CONTENT_TYPE
        self.cache.put(self.thread, \
            {'ip':self.remote_host, 'jid': net_msg.sender}, CACHE_TIME)

        osrf.log.log_debug("caching session [%s] for host [%s] and server "
            " drone [%s]" % (self.thread, self.remote_host, net_msg.sender))



    def check_status(self, net_msg): 
        ''' Checks the status of the server response. 
            If we received a timeout message, we drop it.
            if it's any other non-continue status, we mark this session as
            complete and carry on.
            @return False if there is no data to return to the caller 
            (dropped message, eg. timeout), True otherwise '''

        osrf.log.log_debug('checking status...')
        osrf_msgs = osrf.json.to_object(net_msg.body)
        last_msg = osrf_msgs.pop()

        if last_msg.type() == OSRF_MESSAGE_TYPE_STATUS:
            code = int(last_msg.payload().statusCode())
            osrf.log.log_debug("got a status message with code %d" % code)

            if code == OSRF_STATUS_TIMEOUT:
                osrf.log.log_debug("removing cached session [%s] and "
                    "dropping TIMEOUT message" % net_msg.thread)
                self.cache.delete(net_msg.thread)
                return False 

            if code != OSRF_STATUS_CONTINUE:
                self.complete = True

        return True


    def respond_chunk(self, resp):
        ''' Writes a single multipart-delimited chunk of data '''

        self.write("Content-type: %s\n\n" % JSON_CONTENT_TYPE)
        self.write("%s\n\n" % resp.body)
        if self.complete:
            self.write("--%s--\n" % self.delim)
        else:
            self.write("--%s\n" % self.delim)
        self.apreq.flush()

    def write(self, msg):
        ''' Writes data to the client stream. '''

        if DEBUG_WRITE:
            sys.stderr.write(msg)
            sys.stderr.flush()
        self.apreq.write(msg)
            

