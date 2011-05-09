"""
Unit tests for the osrf.json module
"""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import osrf.json, osrf.net_obj, unittest

class TestObject(object):
    """Test object with basic JSON structures"""
    def __init__(self):
        self.int = 1
        self.string = "two"
        self.array = [1,2,3,4]
        self.dict = {'foo': 'bar', 'key': 'value'}
        self.true = True
        self.false = False
        self.null = None

class CheckObjectToJSON(unittest.TestCase):
    """Tests the osrf.json.to_json() method that converts Python objects into JSON"""
    def setUp(self):
        self.testo = TestObject()

    def test_int(self):
        test_json = osrf.json.to_json(self.testo.int)
        self.assertEqual(test_json, '1')

    def test_string(self):
        test_json = osrf.json.to_json(self.testo.string)
        self.assertEqual(test_json, '"two"')

    def test_array(self):
        test_json = osrf.json.to_json(self.testo.array)
        self.assertEqual(test_json, '[1, 2, 3, 4]')

    def test_dict(self):
        test_json = osrf.json.to_json(self.testo.dict)
        self.assertEqual(test_json, '{"foo": "bar", "key": "value"}')

    def test_true(self):
        test_json = osrf.json.to_json(self.testo.true)
        self.assertEqual(test_json, 'true')

    def test_false(self):
        test_json = osrf.json.to_json(self.testo.false)
        self.assertEqual(test_json, 'false')

    def test_null(self):
        test_json = osrf.json.to_json(self.testo.null)
        self.assertEqual(test_json, 'null')

class CheckJSONToObject(unittest.TestCase):
    """Tests that the osrf.json.to_object() method converts JSON into Python objects"""

    def setUp(self):
        self.testo = TestObject()

    def test_int(self):
        test_json = osrf.json.to_object('1')
        self.assertEqual(test_json, self.testo.int)

    def test_string(self):
        test_json = osrf.json.to_object('"two"')
        self.assertEqual(test_json, self.testo.string)

    def test_array(self):
        test_json = osrf.json.to_object('[1, 2, 3, 4]')
        self.assertEqual(test_json, self.testo.array)

    def test_dict(self):
        test_json = osrf.json.to_object('{"foo": "bar", "key": "value"}')
        self.assertEqual(test_json, self.testo.dict)

    def test_true(self):
        test_json = osrf.json.to_object('true')
        self.assertEqual(test_json, self.testo.true)

    def test_false(self):
        test_json = osrf.json.to_object('false')
        self.assertEqual(test_json, self.testo.false)

    def test_null(self):
        test_json = osrf.json.to_object('null')
        self.assertEqual(test_json, self.testo.null)

if __name__ == '__main__':
    unittest.main()
