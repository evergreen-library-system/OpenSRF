from xml.dom import minidom
from xml.sax import handler, make_parser, saxutils
from json import *
from net_obj import *
import urllib, urllib2, sys

defaultHost = None

class GatewayRequest:
    def __init__(self, service, method, params=[]):
        self.service = service
        self.method = method
        self.params = params

    def send(self):
        params = self.buildPOSTParams()
        request = urllib2.Request(self.buildURL(), data=params)
        response = None
        try:
            response =urllib2.urlopen(request)
        except urllib2.HTTPError, e:
            # log this?
            sys.stderr.write('HTTPError: code=%d : %s' % (e.code, str(e)))
            raise e
            
        return self.handleResponse(response)

    def buildPOSTParams(self):

        params = urllib.urlencode({   
            'service': self.service,
            'method': self.method,
            'format': self.getFormat()
        })

        for p in self.params:
            param = {'param': osrfObjectToJSON(p)}
            params += '&%s' % urllib.urlencode(param)

        return params

    def setDefaultHost(host):
        global defaultHost
        defaultHost = host
    setDefaultHost = staticmethod(setDefaultHost)

    def buildURL(self):
        return 'http://%s/gateway' % defaultHost


class XMLGatewayRequest(GatewayRequest):

    def __init__(self, service, method, *params):
        GatewayRequest.__init__(self, service, method, list(params))

    def getFormat(self):
        return 'xml'

    def handleResponse(self, response):
        handler = XMLGatewayParser()
        parser = make_parser()
        parser.setContentHandler(handler)
        parser.parse(response)
        return handler.getResult()

class XMLGatewayParser(handler.ContentHandler):

    def __init__(self):
        self.result = None
        self.objStack = []
        self.keyStack = []

    def getResult(self):
        return self.result

    def __getAttr(self, attrs, name):
        for (k, v) in attrs.items():
            if k == name:
                return v
        return None

    def startElement(self, name, attrs):

        # XXX add support for serializable objects!

        if name == 'element': # this is an object item wrapper
            self.keyStack.append(self.__getAttr(attrs, 'key'))
            return

        if name == 'object':
            obj = {}
            self.appendChild(obj)
            self.objStack.append(obj)
            return

        if name == 'array':
            obj = []
            self.appendChild(obj)
            self.objStack.append(obj)
            return

        if name == 'null':
            self.appendChild(None)
            return

        if name == 'boolean':
            self.appendChild((self.__getAttr(attrs, 'value') == 'true'))
            return


    def appendChild(self, child):

        if self.result == None:
            self.result = child

        if not self.objStack: return;

        parent = self.objStack[len(self.objStack)-1]

        if( isinstance(parent, list) ):
            parent.append(child)
        else:
            parent[self.keyStack.pop()] = child

    def endElement(self, name):
        if name == 'array' or name == 'object':
            self.objStack.pop()

    def characters(self, chars):
        #self.appendChild(''.join(chars[start:leng+start]))
        self.appendChild(urllib.unquote_plus(chars))



    

