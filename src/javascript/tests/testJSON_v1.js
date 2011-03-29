dojo.provide('opensrf.tests.testJSON_v1');

dojo.require('DojoSRF');

doh.register("JSONTests", [
    function test_version() {
        doh.assertTrue(JSON_version() == 'wrapper');
    },
    function test_js2JSON() {
        // Solo nulls and booleans are stringified XXX
        doh.assertTrue(js2JSON(null) === "null");
        doh.assertTrue(js2JSON(true) === "true");
        doh.assertTrue(js2JSON(false) === "false");
        doh.assertTrue(js2JSON(0) === 0);
        doh.assertTrue(js2JSON(1.5) === 1.5);
        doh.assertTrue(js2JSON(.7) === .7);
        doh.assertTrue(js2JSON("") == '""');
        doh.assertTrue(js2JSON("foo") == '"foo"');
        // Escape sequences
        doh.assertTrue(js2JSON("foo\n\t\n") == '"foo\\n\\t\\n"');
        doh.assertTrue(js2JSON({"foo":"bar"}) == '{"foo":"bar"}');
        doh.assertTrue(js2JSON({"foo":true}) == '{"foo":true}');
        doh.assertTrue(js2JSON({"foo":0}) == '{"foo":0}');
        doh.assertTrue(js2JSON([0,"foo",null,"true",true]) === '[0,"foo",null,"true",true]');
        // Order of object attributes is not guaranteed
        doh.assertTrue(js2JSON({"foo":{"one":null,"two":2}}) == '{"foo":{"two":2,"one":null}}');
    },
    function test_js2JSONRaw() {
        // Solo nulls and booleans are stringified XXX
        doh.assertTrue(js2JSONRaw(null) === "null");
        doh.assertTrue(js2JSONRaw(true) === "true");
        doh.assertTrue(js2JSONRaw(false) === "false");
        doh.assertTrue(js2JSONRaw(0) === 0);
        doh.assertTrue(js2JSONRaw(1.5) === 1.5);
        doh.assertTrue(js2JSONRaw(.7) === .7);
        doh.assertTrue(js2JSONRaw("") == '""');
        doh.assertTrue(js2JSONRaw("foo") == '"foo"');
        // Escape sequences
        doh.assertTrue(js2JSONRaw("foo\n\t\n") == '"foo\\n\\t\\n"');
        doh.assertTrue(js2JSONRaw({"foo":"bar"}) == '{"foo":"bar"}');
        doh.assertTrue(js2JSONRaw({"foo":true}) == '{"foo":true}');
        doh.assertTrue(js2JSONRaw({"foo":0}) == '{"foo":0}');
        doh.assertTrue(js2JSONRaw([0,"foo",null,"true",true]) === '[0,"foo",null,"true",true]');
        // Order of object attributes is not guaranteed
        doh.assertTrue(js2JSONRaw({"foo":{"one":null,"two":2}}) == '{"foo":{"two":2,"one":null}}');
    },
    function test_JSON2js() {
        // Standalone quoted nulls and booleans are converted to primitives
        doh.assertTrue(JSON2js(null) === null);
        doh.assertTrue(JSON2js("null") === null);
        doh.assertTrue(JSON2js(true) === true);
        doh.assertTrue(JSON2js("true") === true);
        doh.assertTrue(JSON2js(false) === false);
        doh.assertTrue(JSON2js("false") === false);
        // Zero is zero and only zero
        doh.assertTrue(JSON2js(0) === 0);
        // Empty string
        doh.assertTrue(JSON2js('""') === "");
        // String
        doh.assertTrue(JSON2js('"foo"') == "foo");
        // Array; access an index
        doh.assertTrue(JSON2js('[0,1,2,3,4,5]')[1] == 1);
        // Object; access a key
        doh.assertTrue(JSON2js('{"foo":"bar"}').foo == "bar");
        doh.assertTrue(JSON2js('{"foo":{"two":2,"one":null}}').foo.one === null);
        doh.assertTrue(JSON2js('{"foo":{"two":2,"one":"null"}}').foo.one === "null");
    }
]);
