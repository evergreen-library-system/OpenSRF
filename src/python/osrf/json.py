import simplejson, types 
from osrf.net_obj import NetworkObject, parse_net_object
from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY

class NetworkEncoder(simplejson.JSONEncoder):
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


def to_json(obj):
    """Turns a python object into a wrapped JSON object"""
    return simplejson.dumps(obj, cls=NetworkEncoder)


def to_object(json):
    """Turns a JSON string into python objects"""
    obj = simplejson.loads(json)
    return parse_net_object(obj)

def parse_json_raw(json):
    """Parses JSON the old fashioned way."""
    return simplejson.loads(json)

def to_json_raw(obj):
    """Stringifies an object as JSON with no additional logic."""
    return simplejson.dumps(obj)

def __tabs(depth):
    space = ''
    while range(depth):
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

            key = k
            while len(key) < 24: key += '.' # pad the names to make the values line up somewhat
            val = getattr(obj, k)()

            subobj = val and not (isinstance(val, unicode) or \
                isinstance(val, int) or isinstance(val, float) or isinstance(val, long))

            debug_str += __tabs(depth) + key + ' = '

            if subobj:
                debug_str += '\n'
                val = debug_net_object(val, depth+1)

            debug_str += str(val)

            if not subobj: debug_str += '\n'

    else:
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

        if eatws: # simpljson adds a pesky space after array and object items
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
