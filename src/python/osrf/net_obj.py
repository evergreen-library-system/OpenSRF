from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY
import re
from xml.sax import saxutils


# -----------------------------------------------------------
# Define the global network-class registry
# -----------------------------------------------------------


class NetworkRegistry(object):
    ''' Network-serializable objects must be registered.  The class
        hint maps to a set (ordered in the case of array-base objects)
        of field names (keys).  
        '''

    # Global object registry 
    registry = {}

    def __init__(self, hint, keys, protocol):
        self.hint = hint
        self.keys = keys
        self.protocol = protocol
        NetworkRegistry.registry[hint] = self
    
    @staticmethod
    def get_registry(hint):
        return NetworkRegistry.registry.get(hint)

# -----------------------------------------------------------
# Define the base class for all network-serializable objects
# -----------------------------------------------------------

class NetworkObject(object):
    ''' Base class for all network serializable objects '''

    # link to our registry object for this registered class
    registry = None

    def __init__(self, data=None):
        ''' If this is an array, we pull data out of the data array
            (if there is any) and translate that into a hash internally '''

        self._data = data
        if not data: self._data = {}
        if isinstance(data, list):
            self.import_array_data(list)

    def import_array_data(self, data):
        ''' If an array-based object is created with an array
            of data, cycle through and load the data '''

        self._data = {}
        if len(data) == 0:
            return

        reg = self.get_registry()
        if reg.protocol == 'array':
            for entry in range(len(reg.keys)):
                if len(data) > entry:
                    break
                self.set_field(reg.keys[entry], data[entry])

    def get_data(self):
        ''' Returns the full dataset for this object as a dict '''
        return self._data

    def set_field(self, field, value):
        self._data[field] = value

    def get_field(self, field):
        return self._data.get(field)

    def get_registry(self):
        ''' Returns the registry object for this registered class '''
        return self.__class__.registry

def new_object_from_hint(hint):
    ''' Given a hint, this will create a new object of that 
        type and return it.  If this hint is not registered,
        an object of type NetworkObject.__unknown is returned'''
    try:
        obj = None
        exec('obj = NetworkObject.%s()' % hint)
        return obj
    except AttributeError:
        return NetworkObject.__unknown()

def __make_network_accessor(cls, key):
    ''' Creates and accessor/mutator method for the given class.  
        'key' is the name the method will have and represents
        the field on the object whose data we are accessing ''' 
    def accessor(self, *args):
        if len(args) != 0:
            self.set_field(key, args[0])
        return self.get_field(key)
    setattr(cls, key, accessor)


def register_hint(hint, keys, type='hash'):
    ''' Registers a new network-serializable object class.

        'hint' is the class hint
        'keys' is the list of field names on the object
            If this is an array-based object, the field names
            must be sorted to reflect the encoding order of the fields
        'type' is the wire-protocol of the object.  hash or array.
        '''

    # register the class with the global registry
    registry = NetworkRegistry(hint, keys, type)

    # create the new class locally with the given hint name
    exec('class %s(NetworkObject):\n\tpass' % hint)

    # give the new registered class a local handle
    cls = None
    exec('cls = %s' % hint)

    # assign an accessor/mutator for each field on the object
    for k in keys:
        __make_network_accessor(cls, k)

    # attach our new class to the NetworkObject 
    # class so others can access it
    setattr(NetworkObject, hint , cls)
    cls.registry = registry




# create a unknown object to handle unregistred types
register_hint('__unknown', [], 'hash')

# -------------------------------------------------------------------
# Define the custom object parsing behavior 
# -------------------------------------------------------------------
def parse_net_object(obj):
    
    try:
        hint = obj[OSRF_JSON_CLASS_KEY]
        sub_object = obj[OSRF_JSON_PAYLOAD_KEY]
        reg = NetworkRegistry.get_registry(hint)

        obj = {}

        if reg.protocol == 'array':
            for entry in range(len(reg.keys)):
                if len(sub_object) > entry:
                    obj[reg.keys[entry]] = parse_net_object(sub_object[entry])
                else:
                    obj[reg.keys[entry]] = None
        else:
            for key in reg.keys:
                obj[key] = parse_net_object(sub_object.get(key))

        estr = 'obj = NetworkObject.%s(obj)' % hint
        try:
            exec(estr)
        except:
            # this object has not been registered, shove it into the default container
            obj = NetworkObject.__unknown(obj)

        return obj

    except:
        pass

    # the current object does not have a class hint
    if isinstance(obj, list):
        for entry in range(len(obj)):
            obj[entry] = parse_net_object(obj[entry])

    else:
        if isinstance(obj, dict):
            for key, value in obj.iteritems():
                obj[key] = parse_net_object(value)

    return obj


def to_xml(obj):
    """ Returns the XML representation of an internal object."""
    chars = []
    __to_xml(obj, chars)
    return ''.join(chars)

def __to_xml(obj, chars):
    """ Turns an internal object into OpenSRF XML """

    if obj is None:
        chars.append('<null/>')
        return

    if isinstance(obj, unicode) or isinstance(obj, str):
        chars.append('<string>%s</string>' % saxutils.escape(obj))
        return

    if isinstance(obj, int)  or isinstance(obj, long):
        chars.append('<number>%d</number>' % obj)
        return

    if isinstance(obj, float):
        chars.append('<number>%f</number>' % obj)
        return

    if isinstance(obj, NetworkObject): 

        registry = obj.get_registry()
        data = obj.get_data()
        hint = saxutils.escape(registry.hint)

        if registry.protocol == 'array':
            chars.append("<array class_hint='%s'>" % hint)
            for key in registry.keys:
                __to_xml(data.get(key), chars)
            chars.append('</array>')

        else:
            if registry.protocol == 'hash':
                chars.append("<object class_hint='%s'>" % hint)
                for key, value in data.items():
                    chars.append("<element key='%s'>" % saxutils.escape(key))
                    __to_xml(value, chars)
                    chars.append('</element>')
                chars.append('</object>')
                

    if isinstance(obj, list):
        chars.append('<array>')
        for entry in obj:
            __to_xml(entry, chars)
        chars.append('</array>')
        return

    if isinstance(obj, dict):
        chars.append('<object>')
        for key, value in obj.items():
            chars.append("<element key='%s'>" % saxutils.escape(key))
            __to_xml(value, chars)
            chars.append('</element>')
        chars.append('</object>')
        return

    if isinstance(obj, bool):
        val = 'false'
        if obj:
            val = 'true'
        chars.append("<boolean value='%s'/>" % val)
        return

def find_object_path(obj, path, idx=None):
    """Searches an object along the given path for a value to return.

    Path separators can be '/' or '.', '/' is tried first."""

    parts = []

    if re.search('/', path):
        parts = path.split('/')
    else:
        parts = path.split('.')

    for part in parts:
        try:
            val = obj[part]
        except:
            return None
        if isinstance(val, str): 
            return val
        if isinstance(val, list):
            if idx != None:
                return val[idx]
            return val
        if isinstance(val, dict):
            obj = val
        else:
            return val

    return obj
