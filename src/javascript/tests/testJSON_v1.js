dojo.provide('opensrf.tests.testJSON_v1');

dojo.require('DojoSRF');

doh.register("JSONTests", [
    function test_version() {
        doh.assertTrue(JSON_version() == 'wrapper');
    },
    function test_decodeJS() {
        doh.assertTrue(decodeJS(0) === 0);
        doh.assertTrue(decodeJS(null) === null);
        doh.assertTrue(decodeJS("") === "");
    },
    function test_encodeJS() {
        doh.assertTrue(encodeJS(0) === 0);
        doh.assertTrue(encodeJS(null) === null);
        doh.assertTrue(encodeJS("") === "");
    },
    function test_js2JSON_strict() {
        // Solo nulls and booleans are stringified XXX
        doh.assertTrue(js2JSON(null) === "null");
        doh.assertTrue(js2JSON(true) === "true");
        doh.assertTrue(js2JSON(false) === "false");
    },
    function test_js2JSON_numbers() {
        doh.assertTrue(js2JSON(0) === 0);
        doh.assertTrue(js2JSON(1.5) === 1.5);
        doh.assertTrue(js2JSON(.7) === .7);
    },
    function test_js2JSON_strings() {
        doh.assertTrue(js2JSON("") == '""');
        doh.assertTrue(js2JSON("foo") == '"foo"');
        // Escape sequences
        doh.assertTrue(js2JSON("foo\n\t\n") == '"foo\\n\\t\\n"');
    },
    function test_js2JSON_arrays() {
        doh.assertTrue(js2JSON([0,"foo",null,"true",true]) === '[0,"foo",null,"true",true]');
    },
    function test_js2JSON_objects() {
        doh.assertTrue(js2JSON({"foo":"bar"}) == '{"foo":"bar"}');
        doh.assertTrue(js2JSON({"foo":true}) == '{"foo":true}');
        doh.assertTrue(js2JSON({"foo":0}) == '{"foo":0}');
    },
    function test_js2JSON_objects_ordered() {
        // Order of object attributes is not guaranteed
        doh.assertTrue(js2JSON({"foo":{"one":[null,"two",2]}}) == '{"foo":{"one":[null,"two",2]}}');
    },
    function test_JSON2js_strict() {
        // Standalone quoted nulls and booleans are converted to primitives
        doh.assertTrue(JSON2js(null) === null);
        doh.assertTrue(JSON2js("null") === null);
        doh.assertTrue(JSON2js(true) === true);
        doh.assertTrue(JSON2js("true") === true);
        doh.assertTrue(JSON2js(false) === false);
        doh.assertTrue(JSON2js("false") === false);
    },
    function test_JSON2js_numbers() {
        // Zero is zero and only zero
        doh.assertTrue(JSON2js(0) === 0);
        doh.assertTrue(JSON2js(1.5) === 1.5);
        doh.assertTrue(JSON2js(.5) === .5);
    },
    function test_JSON2js_strings() {
        // Empty string
        doh.assertTrue(JSON2js('""') === "");
        // String
        doh.assertTrue(JSON2js('"foo"') == "foo");
    },
    function test_JSON2js_arrays() {
        // Array; access an index
        doh.assertTrue(JSON2js('[0,1,2,3,4,5]')[1] == 1);
    },
    function test_JSON2js_objects() {
        // Object; access a key
        doh.assertTrue(JSON2js('{"foo":"bar"}').foo == "bar");
        doh.assertTrue(JSON2js('{"foo":{"two":2,"one":null}}').foo.one === null);
        doh.assertTrue(JSON2js('{"foo":{"two":2,"one":"null"}}').foo.one === "null");
    }
]);
