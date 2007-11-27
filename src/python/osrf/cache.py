import memcache
from osrf.json import osrfObjectToJSON, osrfJSONToObject

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
            self.client = memcache.Client(server, debug=0)
        else:
            if not _client:
                raise CacheException("not connected to any memcache servers.  try CacheClient.connect(servers)")
            self.client = _client

    def put(self, key, val, timeout=None):
        global defaultTimeout
        if timeout is None:
            timeout = defaultTimeout
        self.client.set(key, osrfObjectToJSON(val), timeout)

    def get(self, key):
        return osrfJSONToObject(self.client.get(key) or "null")

    @staticmethod
    def connect(svrs):
        global _client
        _client = memcache.Client(svrs, debug=0)


