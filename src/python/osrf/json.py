import simplejson
from osrf.net_obj import NetworkObject, parse_net_object
from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY
import osrf.log

try:
    # if available, use the faster cjson module for encoding/decoding JSON
    import cjson
    _use_cjson = True
except ImportError:
    _use_cjson = False

_use_cjson = False

class NetworkEncoder(simplejson.JSONEncoder):
    ''' Encoder used by simplejson '''

    def default(self, obj):
        '''
        Extend the default method offered by simplejson.JSONEncoder

        Wraps the Python object into with OpenSRF class / payload keys
        '''

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
        for key, val in obj.iteritems():
            newobj[key] = encode_object(val)
        return newobj

    elif isinstance(obj, list):
        return [encode_object(v) for v in obj]

    elif isinstance(obj, NetworkObject):
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
        return cjson.encode(obj)
    return simplejson.dumps(obj)

def __tabs(depth):
    '''
    Returns a string of spaces-not-tabs for the desired indentation level

    >>> print '"' + __tabs(0) + '"'
    ""
    >>> print '"' + __tabs(4) + '"'
    "                "
    '''
    space = '    ' * depth
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

            if not subobj:
                debug_str += '\n'

    else:
        osrf.log.log_internal("Pretty-printing NetworkObject")
        debug_str = pprint(to_json(obj))
    return debug_str

def pprint(json):
    """JSON pretty-printer"""
    result = ''
    tab = 0
    instring = False
    inescape = False
    done = False
    eatws = False

    for char in json:

        if eatws and not _use_cjson: # simplejson adds a pesky space after array and object items
            if char == ' ': 
                continue

        eatws = False
        done = False
        if (char == '{' or char == '[') and not instring:
            tab += 1
            result += char + '\n' + __tabs(tab)
            done = True

        if (char == '}' or char == ']') and not instring:
            tab -= 1
            result += '\n' + __tabs(tab) + char
            done = True

        if char == ',' and not instring:
            result += char + '\n' + __tabs(tab)
            done = True
            eatws = True

        if char == ':' and not instring:
            eatws = True

        if char == '"' and not inescape:
            instring = not instring

        if inescape: 
            inescape = False

        if char == '\\':
            inescape = True

        if not done:
            result += char

    return result

if __name__ == "__main__":
    import doctest
    doctest.testmod()
