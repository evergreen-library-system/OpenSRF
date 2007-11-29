import memcache
from osrf.json import osrfObjectToJSON, osrfJSONToObject
from osrf.log import *

'''
Abstracted OpenSRF caching interface.
Requires memcache: ftp://ftp.tummy.com/pub/python-memcached/
'''

_client = None
defaultTimeout = 0

class CacheException(Exception):
    def __init__(self, info):
        self.info = info
    def __str__(self):
        return "%s: %s" % (self.__class__.__name__, self.info)

class CacheClient(object):
    def __init__(self, servers=None):
        ''' If no servers are provided, this instance will use 
            the global memcache connection.
            servers takes the form ['server:port', 'server2:port2', ...]
            '''
        global _client
        if servers:
            self.client = memcache.Client(server, debug=1)
        else:
            if not _client:
                raise CacheException("not connected to any memcache servers.  try CacheClient.connect(servers)")
            self.client = _client

    def put(self, key, val, timeout=None):
        global defaultTimeout
        if timeout is None:
            timeout = defaultTimeout
        s = osrfObjectToJSON(val)
        osrfLogInternal("cache: %s => %s" % (str(key), s))
        return self.client.set(str(key), s, timeout)

    def get(self, key):
        o = self.client.get(str(key))
        osrfLogInternal("cache: fetching %s => %s" % (str(key), o))
        return osrfJSONToObject(o or "null")

    def delete(self, key):
        osrfLogInternal("cache: deleting %s" % str(key))
        self.client.delete(str(key))

    @staticmethod
    def connect(svrs):
        global _client
        osrfLogDebug("cache: connecting to servers %s" % str(svrs))
        _client = memcache.Client(svrs, debug=1)


