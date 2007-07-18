import simplejson, types 
from osrf.net_obj import *
from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY

class osrfJSONNetworkEncoder(simplejson.JSONEncoder):
    def default(self, obj):

        if isinstance(obj, osrfNetworkObject):
            reg = obj.getRegistry()
            data = obj.getData()

            # re-encode the object as an array if necessary
            if reg.wireProtocol == 'array':
                d = []
                for k in reg.keys:
                    d.append(data[k]) 
                data = d

            return { 
                OSRF_JSON_CLASS_KEY: reg.hint,
                OSRF_JSON_PAYLOAD_KEY: self.default(data)
            }   
        return obj


def osrfObjectToJSON(obj):
    """Turns a python object into a wrapped JSON object"""
    return simplejson.dumps(obj, cls=osrfJSONNetworkEncoder)


def osrfJSONToObject(json):
    """Turns a JSON string into python objects"""
    obj = simplejson.loads(json)
    return parseNetObject(obj)

def osrfParseJSONRaw(json):
    """Parses JSON the old fashioned way."""
    return simplejson.loads(json)

def osrfToJSONRaw(obj):
    """Stringifies an object as JSON with no additional logic."""
    return simplejson.dumps(obj)

def __tabs(t):
    r=''
    for i in range(t): r += '   '
    return r

def osrfDebugNetworkObject(obj, t=1):
    """Returns a debug string for a given object.

    If it's an osrfNetworkObject and has registered keys, key/value p
    pairs are returned.  Otherwise formatted JSON is returned"""

    s = ''
    if isinstance(obj, osrfNetworkObject):
        reg = obj.getRegistry()
        keys = list(reg.keys) # clone it, so sorting won't break the original
        keys.sort()

        for k in keys:

            key = k
            while len(key) < 24: key += '.' # pad the names to make the values line up somewhat
            val = getattr(obj, k)()

            subobj = val and not (isinstance(val,unicode) or \
                isinstance(val, int) or isinstance(val, float) or isinstance(val, long))

            s += __tabs(t) + key + ' = '

            if subobj:
                s += '\n'
                val = osrfDebugNetworkObject(val, t+1)

            s += str(val)

            if not subobj: s += '\n'

    else:
        s = osrfFormatJSON(osrfObjectToJSON(obj))
    return s

def osrfFormatJSON(json):
    """JSON pretty-printer"""
    r = ''
    t = 0
    instring = False
    inescape = False
    done = False
    eatws = False

    for c in json:

        if eatws: # simpljson adds a pesky after array and object items
            if c == ' ': 
                continue

        eatws = False
        done = False
        if (c == '{' or c == '[') and not instring:
            t += 1
            r += c + '\n' + __tabs(t)
            done = True

        if (c == '}' or c == ']') and not instring:
            t -= 1
            r += '\n' + __tabs(t) + c
            done = True

        if c == ',' and not instring:
            r += c + '\n' + __tabs(t)
            done = True
            eatws = True

        if c == ':' and not instring:
            eatws = True

        if c == '"' and not inescape:
            instring = not instring

        if inescape: 
            inescape = False

        if c == '\\':
            inescape = True

        if not done:
            r += c

    return r






