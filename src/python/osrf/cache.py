import memcache
from osrf.json import to_json, to_object
import osrf.log

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
            self.client = memcache.Client(servers, debug=1)
        else:
            if not _client:
                raise CacheException(
                    "not connected to any memcache servers."
                    "try CacheClient.connect(servers)"
                )
            self.client = _client

    def put(self, key, val, timeout=None):
        global defaultTimeout
        if timeout is None:
            timeout = defaultTimeout
        timeout = int(timeout)
        json = to_json(val)
        osrf.log.log_internal("cache: %s => %s" % (str(key), json))
        return self.client.set(str(key), json, timeout)

    def get(self, key):
        obj = self.client.get(str(key))
        osrf.log.log_internal("cache: fetching %s => %s" % (str(key), obj))
        return to_object(obj or "null")

    def delete(self, key):
        osrf.log.log_internal("cache: deleting %s" % str(key))
        self.client.delete(str(key))

    @staticmethod
    def connect(svrs):
        global _client
        osrf.log.log_debug("cache: connecting to servers %s" % str(svrs))
        _client = memcache.Client(svrs, debug=1)

    @staticmethod
    def get_client():
        global _client
        return _client


