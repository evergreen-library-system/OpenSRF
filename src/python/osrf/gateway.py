from xml.dom import minidom
from xml.sax import handler, make_parser, saxutils
from osrf.json import to_object
from osrf.net_obj import NetworkObject, new_object_from_hint
import osrf.log
import urllib, urllib2, sys, re

defaultHost = None

class GatewayRequest:
    def __init__(self, service, method, params=[]):
        self.service = service
        self.method = method
        self.params = params
        self.path = 'gateway'
        self.bytes_read = 0 # for now this, this is really characters read

    def setPath(self, path):
        self.path = path

    def send(self):
        params = self.buildPOSTParams()
        request = urllib2.Request(self.buildURL(), data=params)
        response = None
        try:
            response =urllib2.urlopen(request)
        except urllib2.HTTPError, e:
            # log this?
            sys.stderr.write('%s => %s?%s\n' % (unicode(e), self.buildURL(), params))
            raise e
            
        return self.handleResponse(response)

    def buildPOSTParams(self):

        params = urllib.urlencode({   
            'service': self.service,
            'method': self.method,
            'format': self.getFormat(),
            'input_format': self.getInputFormat()
        })

        for p in self.params:
            params += '&param=%s' % urllib.quote(self.encodeParam(p), "'/")
        return params

    def setDefaultHost(host):
        global defaultHost
        defaultHost = host
    setDefaultHost = staticmethod(setDefaultHost)

    def buildURL(self):
        return 'http://%s/%s' % (defaultHost, self.path)

class JSONGatewayRequest(GatewayRequest):
    def __init__(self, service, method, *params):
        GatewayRequest.__init__(self, service, method, list(params))

    def getFormat(self):
        return 'json'

    def getInputFormat(self):
        return self.getFormat()

    def handleResponse(self, response):

        data = response.read()
        self.bytes_read = len(str(response.headers)) + len(data)
        obj = to_object(data)

        if obj['status'] != 200:
            sys.stderr.write('JSON gateway returned status %d:\n%s\n' % (obj['status'], s))
            return None

        # the gateway wraps responses in an array to handle streaming data
        # if there is only one item in the array, it (probably) wasn't a streaming request
        p = obj['payload']
        if len(p) > 1: return p
        return p[0]

    def encodeParam(self, param):
        return osrf.json.to_json(param)

class XMLGatewayRequest(GatewayRequest):

    def __init__(self, service, method, *params):
        GatewayRequest.__init__(self, service, method, list(params))

    def getFormat(self):
        return 'xml'

    def getInputFormat(self):
        return self.getFormat()

    def handleResponse(self, response):
        handler = XMLGatewayParser()
        parser = make_parser()
        parser.setContentHandler(handler)
        try:
            parser.parse(response)
        except Exception, e:
            osrf.log.log_error('Error parsing gateway XML: %s' % unicode(e))
            return None

        return handler.getResult()

    def encodeParam(self, param):
        return osrf.net_obj.to_xml(param);

class XMLGatewayParser(handler.ContentHandler):

    def __init__(self):
        self.result = None
        self.objStack = []
        self.keyStack = []
        self.posStack = [] # for tracking array-based hinted object indices

        # true if we are parsing an element that may have character data
        self.charsPending = 0 

    def getResult(self):
        return self.result

    def __getAttr(self, attrs, name):
        for (k, v) in attrs.items():
            if k == name:
                return v
        return None

    def startElement(self, name, attrs):
        
        if self.charsPending:
            # we just read a 'string' or 'number' element that resulted
            # in no text data.  Appaned a None object
            self.appendChild(None)

        if name == 'null':
            self.appendChild(None)
            return

        if name == 'string' or name == 'number':
            self.charsPending = True
            return

        if name == 'element': # this is an object item wrapper
            self.keyStack.append(self.__getAttr(attrs, 'key'))
            return

        hint = self.__getAttr(attrs, 'class_hint')
        if hint:
            obj = new_object_from_hint(hint)
            self.appendChild(obj)
            self.objStack.append(obj)
            if name == 'array':
                self.posStack.append(0)
            return

        if name == 'array':
            obj = []
            self.appendChild(obj)
            self.objStack.append(obj)
            return

        if name == 'object':
            obj = {}
            self.appendChild(obj)
            self.objStack.append(obj)
            return

        if name == 'boolean':
            self.appendChild((self.__getAttr(attrs, 'value') == 'true'))
            return


    def appendChild(self, child):

        if self.result == None:
            self.result = child

        if not self.objStack: return;

        parent = self.objStack[len(self.objStack)-1]

        if isinstance(parent, list):
            parent.append(child)
        else:
            if isinstance(parent, dict):
                parent[self.keyStack.pop()] = child
            else:
                if isinstance(parent, NetworkObject):
                    key = None
                    if parent.get_registry().protocol == 'array':
                        keys = parent.get_registry().keys
                        i = self.posStack.pop()
                        key = keys[i]
                        if i+1 < len(keys):
                            self.posStack.append(i+1)
                    else:
                        key = self.keyStack.pop()

                    parent.set_field(key, child)

    def endElement(self, name):
        if name == 'array' or name == 'object':
            self.objStack.pop()

    def characters(self, chars):
        self.charsPending = False
        self.appendChild(urllib.unquote_plus(chars))



    

