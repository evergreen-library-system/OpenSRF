import os, time, md5, random
from mod_python import apache, util

import osrf.cache
from osrf.system import osrfConnect
from osrf.json import osrfJSONToObject
from osrf.conf import osrfConfigValue
from osrf.set import osrfSettingsValue
from osrf.const import *
from osrf.net import *


''' 
Proof of concept OpenSRF-HTTP multipart streaming gateway.

Example Apache mod_python config:

<Location /osrf-http-translator>
   SetHandler mod_python
   PythonPath "['/path/to/translator-dir'] + sys.path"
   PythonHandler osrf.http_translator
   PythonOption OSRF_CONFIG /path/to/opensrf_core.xml
   PythonOption OSRF_CONFIG_CONTEXT gateway
   # testing only
   PythonAutoReload On
</Location>
'''


OSRF_HTTP_HEADER_TO = 'X-OpenSRF-to'
OSRF_HTTP_HEADER_XID = 'X-OpenSRF-thread'
OSRF_HTTP_HEADER_FROM = 'X-OpenSRF-from'
OSRF_HTTP_HEADER_THREAD = 'X-OpenSRF-thread'
OSRF_HTTP_HEADER_TIMEOUT = 'X-OpenSRF-timeout'
OSRF_HTTP_HEADER_SERVICE = 'X-OpenSRF-service'
OSRF_HTTP_HEADER_MULTIPART = 'X-OpenSRF-multipart'

MULTIPART_CONTENT_TYPE = 'multipart/x-mixed-replace;boundary="%s"'
JSON_CONTENT_TYPE = 'text/plain';
CACHE_TIME = 300

ROUTER_NAME = None
OSRF_DOMAIN = None

# If true, all data sent to the client is also written to stderr (apache error log)
DEBUG_WRITE = False

def _dbg(s):
    ''' testing only '''
    sys.stderr.write("%s\n\n" % str(s))
    sys.stderr.flush()


initComplete = False
def childInit(req):
    ''' At time of writing, mod_python doesn't support a childInit handler,
        so this function is called once per process to initialize 
        the opensrf connection '''

    global initComplete, ROUTER_NAME, OSRF_DOMAIN
    if initComplete: 
        return

    ops = req.get_options()
    conf = ops['OSRF_CONFIG']
    ctxt = ops.get('OSRF_CONFIG_CONTEXT') or 'opensrf'
    osrfConnect(conf, ctxt)

    ROUTER_NAME = osrfConfigValue('router_name')
    OSRF_DOMAIN = osrfConfigValue('domains.domain')
    initComplete = True

    servers = osrfSettingsValue('cache.global.servers.server')
    if not isinstance(servers, list):
        servers = [servers]
    osrf.cache.CacheClient.connect(servers)


def handler(req):
    ''' Create the translator and tell it to process the request. '''
    childInit(req)
    return HTTPTranslator(req).process()

class HTTPTranslator(object):
    def __init__(self, apreq):

        self.apreq = apreq
        if apreq.header_only: 
            return

        try:
            post = util.parse_qsl(apreq.read(int(apreq.headers_in['Content-length'])))
            self.body = [d for d in post if d[0] == 'osrf-msg'][0][1]
        except: 
            self.body = None
            return

        self.messages = []
        self.complete = False
        self.handle = osrfGetNetworkHandle()
        self.handle.setRecvCallback(None)

        self.to = apreq.headers_in.get(OSRF_HTTP_HEADER_TO)
        self.service = apreq.headers_in.get(OSRF_HTTP_HEADER_SERVICE)
        self.thread = apreq.headers_in.get(OSRF_HTTP_HEADER_THREAD) or "%s%s" % (os.getpid(), time.time())
        self.timeout = apreq.headers_in.get(OSRF_HTTP_HEADER_TIMEOUT) or 1200
        self.multipart = str(apreq.headers_in.get(OSRF_HTTP_HEADER_MULTIPART)).lower() == 'true'

        # generate a random multipart delimiter
        m = md5.new()
        m.update("%f%d%d" % (time.time(), os.getpid(), random.randint(100,10000000)))
        self.delim = m.hexdigest()
        self.remoteHost = self.apreq.get_remote_host(apache.REMOTE_NOLOOKUP)
        self.cache = osrf.cache.CacheClient()


    def process(self):

        if self.apreq.header_only: 
            return apache.OK
        if not self.body:
            return apache.HTTP_BAD_REQUEST
        if not self.setToAddr():
            return apache.HTTP_BAD_REQUEST

        while self.handle.recv(0):
            pass # drop stale messages

        netMsg = osrfNetworkMessage(to=self.to, thread=self.thread, body=self.body)
        self.handle.send(netMsg)

        firstWrite = True
        while not self.complete:

            netMsg = self.handle.recv(self.timeout)
            if not netMsg: 
                return apache.GATEWAY_TIME_OUT

            if not self.checkStatus(netMsg):
                continue 

            if firstWrite:
                self.initHeaders(netMsg)
                firstWrite = False

            if self.multipart:
                self.respondChunk(netMsg)
            else:
                self.messages.append(netMsg.body)

                if self.complete:

                    # condense the sets of arrays into a single array of messages
                    json = self.messages.pop(0)
                    while len(self.messages) > 0:
                        m = self.messages.pop(0)
                        json = "%s,%s" % (json[0:len(json)-1], m[1:])
                        
                    self.write("%s" % json)


        return apache.OK

    def setToAddr(self):
        ''' Determines the TO address.  Returns false if 
            the address is missing or ambiguous. '''
        if self.service:
            if self.to:
                # specifying both a SERVICE and a TO is not allowed
                return False
            self.to = "%s@%s/%s" % (ROUTER_NAME, OSRF_DOMAIN, self.service)
            return True
        else:
            if self.to:
                # If the client specifies a specific TO address, verify it's the same
                # address that was cached with the previous request.  
                obj = self.cache.get(self.thread)
                if obj and obj['ip'] == self.remoteHost and obj['jid'] == self.to:
                    return True
        return False

        
    def initHeaders(self, netMsg):
        self.apreq.headers_out[OSRF_HTTP_HEADER_FROM] = netMsg.sender
        if self.multipart:
            self.apreq.content_type = MULTIPART_CONTENT_TYPE % self.delim
            self.write("--%s\n" % self.delim)
        else:
            self.apreq.content_type = JSON_CONTENT_TYPE
        self.cache.put(self.thread, {'ip':self.remoteHost, 'jid': netMsg.sender}, CACHE_TIME)



    def checkStatus(self, netMsg): 
        ''' Checks the status of the server response. 
            If we received a timeout message, we drop it.
            if it's any other non-continue status, we mark this session as
            complete and carry on.
            @return False if there is no data to return to the caller 
            (dropped message, eg. timeout), True otherwise '''

        osrfMsgs = osrfJSONToObject(netMsg.body)
        lastMsg = osrfMsgs.pop()

        if lastMsg.type() == OSRF_MESSAGE_TYPE_STATUS:
            code = int(lastMsg.payload().statusCode())

            if code == OSRF_STATUS_TIMEOUT:
                # remove any existing thread cache for this session and drop the message
                self.cache.delete(netMsg.thread)
                return False 

            if code != OSRF_STATUS_CONTINUE:
                self.complete = True

        return True


    def respondChunk(self, resp):
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
            

