var JSON_CLASS_KEY    = '__c';
var JSON_DATA_KEY    = '__p';



function JSON_version() { return 'wrapper'; }

function JSON2js(text) {
    return decodeJS(JSON.parse(text));
}

JSON2js.fallbackObjectifier = null;

/* iterates over object, arrays, or fieldmapper objects */
function jsIterate( arg, callback ) {
    if( arg && typeof arg == 'object' ) {
        if( arg.constructor == Array ) {
            for( var i = 0; i < arg.length; i++ ) 
                callback(arg, i);

        }  else if( arg.constructor == Object ) {
                for( var i in arg ) 
                    callback(arg, i);

        } else if( arg._isfieldmapper && arg.a ) {
            for( var i = 0; i < arg.a.length; i++ ) 
                callback(arg.a, i);
        }
    }
}


/* removes the class/paylod wrapper objects */
function decodeJS(arg) {

    if(arg == null) return null;

    if(    arg && typeof arg == 'object' &&
            arg.constructor == Object &&
            arg[JSON_CLASS_KEY] ) {

        try {
            arg = eval('new ' + arg[JSON_CLASS_KEY] + '(arg[JSON_DATA_KEY])');    
        } catch(E) {
            if (JSON2js.fallbackObjectifier)
                arg = JSON2js.fallbackObjectifier(arg, JSON_CLASS_KEY, JSON_DATA_KEY );
        }

    }

    if(arg._encodehash) {
        jsIterate( arg.hash, 
            function(o, i) {
                o[i] = decodeJS(o[i]);
            }
        );
    } else {
        jsIterate( arg, 
            function(o, i) {
                o[i] = decodeJS(o[i]);
            }
        );
    }

    return arg;
}


function jsClone(obj) {
    if( obj == null ) return null;
    if( typeof obj != 'object' ) return obj;

    var newobj;
    if (obj.constructor == Array) {
        newobj = [];
        for( var i = 0; i < obj.length; i++ ) 
            newobj[i] = jsClone(obj[i]);

    } else if( obj.constructor == Object ) {
        newobj = {};
        for( var i in obj )
            newobj[i] = jsClone(obj[i]);

    } else if( obj._isfieldmapper && obj.a ) {
        eval('newobj = new '+obj.classname + '();');
        for( var i = 0; i < obj.a.length; i++ ) 
            newobj.a[i] = jsClone(obj.a[i]);
    }

    return newobj;
}
    

/* adds the class/payload wrapper objects */
function encodeJS(arg) {
    if( arg == null ) return null;    
    if( typeof arg != 'object' ) return arg;

    if( arg._isfieldmapper ) {
      var newarr = [];
      if(!arg.a) arg.a = [];
      for( var i = 0; i < arg.a.length; i++ ) 
          newarr[i] = encodeJS(arg.a[i]);

      var a = {};
      a[JSON_CLASS_KEY] = arg.classname;
      a[JSON_DATA_KEY] = newarr;
      return a;
    }

    var newobj;

    if(arg.length != undefined) {
        newobj = [];
        for( var i = 0; i < arg.length; i++ ) 
            newobj.push(encodeJS(arg[i]));
        return newobj;
    } 
   
    newobj = {};
    for( var i in arg )
        newobj[i] = encodeJS(arg[i]);
    return newobj;
}

/* turns a javascript object into a JSON string */
function js2JSON(arg) {
    return JSON.stringify(encodeJS(arg));
}
