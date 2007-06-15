from osrf.const import OSRF_JSON_PAYLOAD_KEY, OSRF_JSON_CLASS_KEY
from xml.sax import saxutils


class osrfNetworkObject(object):
    ''' Base class for all network serializable objects '''
    pass

def osrfNewObjectFromHint(hint):
    try:
        obj = None
        exec('obj = osrfNetworkObject.%s()' % hint)
        return obj
    except AttributeError:
        return osrfNetworkObject.__unknown()
#newFromHint = staticmethod(newFromHint)


''' Global object registry '''
objectRegistry = {}

class osrfNetworkRegistry(object):
    ''' Network-serializable objects must be registered.  The class
        hint maps to a set (ordered in the case of array-base objects)
        of field names (keys).  
        '''

    def __init__(self, hint, keys, wireProtocol):
        global objectRegistry
        self.hint = hint
        self.keys = keys
        self.wireProtocol = wireProtocol
        objectRegistry[hint] = self
    
    def getRegistry(hint):
        global objectRegistry
        return objectRegistry.get(hint)
    getRegistry = staticmethod(getRegistry)


def __makeNetworkAccessor(cls, key):
    '''  Creates and accessor/mutator method for the given class.  

        'key' is the name the method will have and represents
        the field on the object whose data we are accessing
        ''' 
    def accessor(self, *args):
        if len(args) != 0:
            self.__data[key] = args[0]
        return self.__data.get(key)
    setattr(cls, key, accessor)



def __makeGetRegistry(cls, registry):
    ''' Wraps the registry for this class inside an accessor method '''
    def get(self):
        return registry
    setattr(cls, 'getRegistry', get)

def __makeGetData(cls):
    ''' Wraps the stored data in an accessor method '''
    def get(self):
        return self.__data
    setattr(cls, 'getData', get)

def __makeSetField(cls):
    ''' Creates a generic mutator for fields by fieldname '''
    def set(self, field, value):
        self.__data[field] = value
    setattr(cls, 'setField', set)
        

def __osrfNetworkObjectInit(self, data={}):
    ''' __init__ method for osrNetworkObjects.
        If this is an array, we pull data out of the data array
        (if there is any) and translate that into a hash internally
        '''

    self.__data = data
    if len(data) > 0:
        reg = self.getRegistry()
        if reg.wireProtocol == 'array':
            self.__data = {}
            for i in range(len(reg.keys)):
                try:
                    self.__data[reg.keys[i]] = data[i]
                except:
                    self.__data[reg.keys[i]] = None


def osrfNetworkRegisterHint(hint, keys, type='hash'):
    ''' Registers a new network-serializable object class.

        'hint' is the class hint
        'keys' is the list of field names on the object
            If this is an array-based object, the field names
            must be sorted to reflect the encoding order of the fields
        'type' is the wire-protocol of the object.  hash or array.
        '''

    # register the class with the global registry
    registry = osrfNetworkRegistry(hint, keys, type)

    # create the new class locally with the given hint name
    exec('class %s(osrfNetworkObject):\n\tpass' % hint)

    # give the new registered class a local handle
    cls = None
    exec('cls = %s' % hint)

    # assign an accessor/mutator for each field on the object
    for k in keys:
        __makeNetworkAccessor(cls, k)

    # assign our custom init function
    setattr(cls, '__init__', __osrfNetworkObjectInit)
    __makeGetRegistry(cls, registry)
    __makeGetData(cls)
    __makeSetField(cls)


    # attach our new class to the osrfNetworkObject 
    # class so others can access it
    setattr(osrfNetworkObject, hint , cls)




# create a unknown object to handle unregistred types
osrfNetworkRegisterHint('__unknown', [], 'hash')

# -------------------------------------------------------------------
# Define the custom object parsing behavior 
# -------------------------------------------------------------------
def parseNetObject(obj):
    hint = None
    islist = False
    try:
        hint = obj[OSRF_JSON_CLASS_KEY]
        obj = obj[OSRF_JSON_PAYLOAD_KEY]
    except: pass
    if isinstance(obj,list):
        islist = True
        for i in range(len(obj)):
            obj[i] = parseNetObject(obj[i])
    else: 
        if isinstance(obj,dict):
            for k,v in obj.iteritems():
                obj[k] = parseNetObject(v)

    if hint: # Now, "bless" the object into an osrfNetworkObject
        estr = 'obj = osrfNetworkObject.%s(obj)' % hint
        try:
            exec(estr)
        except AttributeError:
            # this object has not been registered, shove it into the default container
            obj = osrfNetworkObject.__unknown(obj)

    return obj;



def osrfObjectToXML(obj):
    """ Returns the XML representation of an internal object."""
    chars = []
    __osrfObjectToXML(obj, chars)
    return ''.join(chars)

def __osrfObjectToXML(obj, chars):
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

    classHint = None

    if isinstance(obj, osrfNetworkObject): 

        registry = obj.getRegistry()
        data = obj.getData()
        hint = saxutils.escape(registry.hint)

        if registry.wireProtocol == 'array':
            chars.append("<array class_hint='%s'>" % hint)
            for k in registry.keys:
                __osrfObjectToXML(data.get(k), chars)
            chars.append('</array>')

        else:
            if registry.wireProtocol == 'hash':
                chars.append("<object class_hint='%s'>" % hint)
                for k,v in data.items():
                    chars.append("<element key='%s'>" % saxutils.escape(k))
                    __osrfObjectToXML(v, chars)
                    chars.append('</element>')
                chars.append('</object>')
                

    if isinstance(obj, list):
        chars.append('<array>')
        for i in obj:
            __osrfObjectToXML(i, chars)
        chars.append('</array>')
        return

    if isinstance(obj, dict):
        chars.append('<object>')
        for k,v in obj.items():
            chars.append("<element key='%s'>" % saxutils.escape(k))
            __osrfObjectToXML(v, chars)
            chars.append('</element>')
        chars.append('</object>')
        return

    if isinstance(obj, bool):
        val = 'false'
        if obj: val = 'true'
        chars.append("<boolean value='%s'/>" % val)
        return

