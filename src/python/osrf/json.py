import simplejson, types 
from osrf.net_obj import NetworkObject, parse_net_object
from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY
import osrf.log

try:
    # if available, use the faster cjson module for encoding/decoding JSON
    import cjson
    _use_cjson = True
except ImportError:
    _use_cjson = False

class NetworkEncoder(simplejson.JSONEncoder):
    ''' Encoder used by simplejson '''
    def default(self, obj):

        if isinstance(obj, NetworkObject):
            reg = obj.get_registry()
            data = obj.get_data()

            # re-encode the object as an array if necessary
            if reg.protocol == 'array':
                objarray = []
                for key in reg.keys:
                    objarray.append(data.get(key)) 
                data = objarray

            return { 
                OSRF_JSON_CLASS_KEY: reg.hint,
                OSRF_JSON_PAYLOAD_KEY: self.default(data)
            }   
        return obj


def encode_object(obj):
    ''' Generic opensrf object encoder, used by cjson '''

    if isinstance(obj, dict):
        newobj = {}
        for k,v in obj.iteritems():
            newobj[k] = encode_object(v)
        return newobj

    else:
        if isinstance(obj, list):
            return [encode_object(v) for v in obj]

        else:
            if isinstance(obj, NetworkObject):
                reg = obj.get_registry()
                data = obj.get_data()
                if reg.protocol == 'array':
                    objarray = []
                    for key in reg.keys:
                        objarray.append(data.get(key)) 
                    data = objarray

                return {
                    OSRF_JSON_CLASS_KEY: reg.hint,
                    OSRF_JSON_PAYLOAD_KEY: encode_object(data)
                }

    return obj
        


def to_json(obj):
    """Turns a python object into a wrapped JSON object"""
    if _use_cjson:
        return cjson.encode(encode_object(obj))
    return simplejson.dumps(obj, cls=NetworkEncoder)


def to_object(json):
    """Turns a JSON string into python objects"""
    if _use_cjson:
        return parse_net_object(cjson.decode(json))
    return parse_net_object(simplejson.loads(json))

def parse_json_raw(json):
    """Parses JSON the old fashioned way."""
    if _use_cjson:
        return cjson.decode(json)
    return simplejson.loads(json)

def to_json_raw(obj):
    """Stringifies an object as JSON with no additional logic."""
    if _use_cjson:
        return cjson.encode(json)
    return simplejson.dumps(obj)

def __tabs(depth):
    space = ''
    for i in range(depth):
        space += '   '
    return space

def debug_net_object(obj, depth=1):
    """Returns a debug string for a given object.

    If it's an NetworkObject and has registered keys, key/value pairs
    are returned.  Otherwise formatted JSON is returned"""

    debug_str = ''
    if isinstance(obj, NetworkObject):
        reg = obj.get_registry()
        keys = list(reg.keys) # clone it, so sorting won't break the original
        keys.sort()

        for k in keys:

            key = str(k)
            while len(key) < 24:
                key += '.' # pad the names to make the values line up somewhat
            val = getattr(obj, k)()

            subobj = val and not (isinstance(val, unicode) or isinstance(val, str) or \
                isinstance(val, int) or isinstance(val, float) or isinstance(val, long))

            debug_str += __tabs(depth) + key + ' = '

            if subobj:
                debug_str += '\n'
                val = debug_net_object(val, depth+1)

            debug_str += str(val)

            if not subobj: debug_str += '\n'

    else:
        osrf.log.log_internal("Pretty-printing NetworkObject")
        debug_str = pprint(to_json(obj))
    return debug_str

def pprint(json):
    """JSON pretty-printer"""
    r = ''
    t = 0
    instring = False
    inescape = False
    done = False
    eatws = False

    for c in json:

        if eatws and not _use_cjson: # simpljson adds a pesky space after array and object items
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
