"""
Unit tests for the osrf.net_obj module
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import osrf.json, osrf.net_obj, unittest

class CheckNetworkEncoder(unittest.TestCase):
    """Tests the NetworkEncoder JSON encoding extension"""

    def setUp(self):
        osrf.net_obj.register_hint('osrfMessage', ['threadTrace', 'locale', 'type', 'payload'], 'hash')
        self.testo = TestObject()
        self.ne = osrf.json.NetworkEncoder()

    def test_connect(self):
        test_json = self.ne.default(
            osrf.net_obj.NetworkObject.osrfMessage({
                    'threadTrace' : 0,
                    'type' : "CONNECT"
                } 
            )
        )
        self.assertEqual(test_json, {'__p': 
            {'threadTrace': 0, 'type': 'CONNECT'},
            '__c': 'osrfMessage'}
        )

    def test_connect_array(self):
        test_json = self.ne.default(
            osrf.net_obj.NetworkObject.osrfMessage({
                    'threadTrace' : 0,
                    'type' : "CONNECT",
                    'protocol' : "array"
                } 
            )
        )
        self.assertEqual(test_json, {'__p':
            {'threadTrace': 0, 'protocol': 'array', 'type': 'CONNECT'},
            '__c': 'osrfMessage'}
        )

    def test_connect_to_xml(self):
        test_json = self.ne.default(
            osrf.net_obj.NetworkObject.osrfMessage({
                    'threadTrace' : 0,
                    'type' : "CONNECT"
                } 
            )
        )
        self.assertEqual(
            osrf.net_obj.to_xml(test_json), 
            "<object><element key='__p'><object><element key='threadTrace'>"
            "<number>0</number></element><element key='type'>"
            "<string>CONNECT</string></element></object></element>"
            "<element key='__c'><string>osrfMessage</string></element></object>"
        )


if __name__ == '__main__':
    unittest.main()

