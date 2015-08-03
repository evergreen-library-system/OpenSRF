/* -----------------------------------------------------------------------
 * Copyright (C) 2008  Georgia Public Library Service
 * Bill Erickson <erickson@esilibrary.com>
 *  
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Portions of this file are Copyright (c) Jon Nylander
 *
 * jsTimezoneDetect is released under the MIT License
 *  - http://www.opensource.org/licenses/mit-license.php
 *
 * For usage and examples, visit: http://pellepim.bitbucket.org/jstz/
 * ----------------------------------------------------------------------- */
(function(e){var t=function(){"use strict";var e="s",n=function(e){var t=-e.getTimezoneOffset();return t!==null?t:0},r=function(e,t,n){var r=new Date;return e!==undefined&&r.setFullYear(e),r.setDate(n),r.setMonth(t),r},i=function(e){return n(r(e,0,2))},s=function(e){return n(r(e,5,2))},o=function(e){var t=e.getMonth()>7?s(e.getFullYear()):i(e.getFullYear()),r=n(e);return t-r!==0},u=function(){var t=i(),n=s(),r=i()-s();return r<0?t+",1":r>0?n+",1,"+e:t+",0"},a=function(){var e=u();return new t.TimeZone(t.olson.timezones[e])},f=function(e){var t=new Date(2010,6,15,1,0,0,0),n={"America/Denver":new Date(2011,2,13,3,0,0,0),"America/Mazatlan":new Date(2011,3,3,3,0,0,0),"America/Chicago":new Date(2011,2,13,3,0,0,0),"America/Mexico_City":new Date(2011,3,3,3,0,0,0),"America/Asuncion":new Date(2012,9,7,3,0,0,0),"America/Santiago":new Date(2012,9,3,3,0,0,0),"America/Campo_Grande":new Date(2012,9,21,5,0,0,0),"America/Montevideo":new Date(2011,9,2,3,0,0,0),"America/Sao_Paulo":new Date(2011,9,16,5,0,0,0),"America/Los_Angeles":new Date(2011,2,13,8,0,0,0),"America/Santa_Isabel":new Date(2011,3,5,8,0,0,0),"America/Havana":new Date(2012,2,10,2,0,0,0),"America/New_York":new Date(2012,2,10,7,0,0,0),"Asia/Beirut":new Date(2011,2,27,1,0,0,0),"Europe/Helsinki":new Date(2011,2,27,4,0,0,0),"Europe/Istanbul":new Date(2011,2,28,5,0,0,0),"Asia/Damascus":new Date(2011,3,1,2,0,0,0),"Asia/Jerusalem":new Date(2011,3,1,6,0,0,0),"Asia/Gaza":new Date(2009,2,28,0,30,0,0),"Africa/Cairo":new Date(2009,3,25,0,30,0,0),"Pacific/Auckland":new Date(2011,8,26,7,0,0,0),"Pacific/Fiji":new Date(2010,11,29,23,0,0,0),"America/Halifax":new Date(2011,2,13,6,0,0,0),"America/Goose_Bay":new Date(2011,2,13,2,1,0,0),"America/Miquelon":new Date(2011,2,13,5,0,0,0),"America/Godthab":new Date(2011,2,27,1,0,0,0),"Europe/Moscow":t,"Asia/Yekaterinburg":t,"Asia/Omsk":t,"Asia/Krasnoyarsk":t,"Asia/Irkutsk":t,"Asia/Yakutsk":t,"Asia/Vladivostok":t,"Asia/Kamchatka":t,"Europe/Minsk":t,"Australia/Perth":new Date(2008,10,1,1,0,0,0)};return n[e]};return{determine:a,date_is_dst:o,dst_start_for:f}}();t.TimeZone=function(e){"use strict";var n={"America/Denver":["America/Denver","America/Mazatlan"],"America/Chicago":["America/Chicago","America/Mexico_City"],"America/Santiago":["America/Santiago","America/Asuncion","America/Campo_Grande"],"America/Montevideo":["America/Montevideo","America/Sao_Paulo"],"Asia/Beirut":["Asia/Beirut","Europe/Helsinki","Europe/Istanbul","Asia/Damascus","Asia/Jerusalem","Asia/Gaza"],"Pacific/Auckland":["Pacific/Auckland","Pacific/Fiji"],"America/Los_Angeles":["America/Los_Angeles","America/Santa_Isabel"],"America/New_York":["America/Havana","America/New_York"],"America/Halifax":["America/Goose_Bay","America/Halifax"],"America/Godthab":["America/Miquelon","America/Godthab"],"Asia/Dubai":["Europe/Moscow"],"Asia/Dhaka":["Asia/Yekaterinburg"],"Asia/Jakarta":["Asia/Omsk"],"Asia/Shanghai":["Asia/Krasnoyarsk","Australia/Perth"],"Asia/Tokyo":["Asia/Irkutsk"],"Australia/Brisbane":["Asia/Yakutsk"],"Pacific/Noumea":["Asia/Vladivostok"],"Pacific/Tarawa":["Asia/Kamchatka"],"Africa/Johannesburg":["Asia/Gaza","Africa/Cairo"],"Asia/Baghdad":["Europe/Minsk"]},r=e,i=function(){var e=n[r],i=e.length,s=0,o=e[0];for(;s<i;s+=1){o=e[s];if(t.date_is_dst(t.dst_start_for(o))){r=o;return}}},s=function(){return typeof n[r]!="undefined"};return s()&&i(),{name:function(){return r}}},t.olson={},t.olson.timezones={"-720,0":"Etc/GMT+12","-660,0":"Pacific/Pago_Pago","-600,1":"America/Adak","-600,0":"Pacific/Honolulu","-570,0":"Pacific/Marquesas","-540,0":"Pacific/Gambier","-540,1":"America/Anchorage","-480,1":"America/Los_Angeles","-480,0":"Pacific/Pitcairn","-420,0":"America/Phoenix","-420,1":"America/Denver","-360,0":"America/Guatemala","-360,1":"America/Chicago","-360,1,s":"Pacific/Easter","-300,0":"America/Bogota","-300,1":"America/New_York","-270,0":"America/Caracas","-240,1":"America/Halifax","-240,0":"America/Santo_Domingo","-240,1,s":"America/Santiago","-210,1":"America/St_Johns","-180,1":"America/Godthab","-180,0":"America/Argentina/Buenos_Aires","-180,1,s":"America/Montevideo","-120,0":"Etc/GMT+2","-120,1":"Etc/GMT+2","-60,1":"Atlantic/Azores","-60,0":"Atlantic/Cape_Verde","0,0":"Etc/UTC","0,1":"Europe/London","60,1":"Europe/Berlin","60,0":"Africa/Lagos","60,1,s":"Africa/Windhoek","120,1":"Asia/Beirut","120,0":"Africa/Johannesburg","180,0":"Asia/Baghdad","180,1":"Europe/Moscow","210,1":"Asia/Tehran","240,0":"Asia/Dubai","240,1":"Asia/Baku","270,0":"Asia/Kabul","300,1":"Asia/Yekaterinburg","300,0":"Asia/Karachi","330,0":"Asia/Kolkata","345,0":"Asia/Kathmandu","360,0":"Asia/Dhaka","360,1":"Asia/Omsk","390,0":"Asia/Rangoon","420,1":"Asia/Krasnoyarsk","420,0":"Asia/Jakarta","480,0":"Asia/Shanghai","480,1":"Asia/Irkutsk","525,0":"Australia/Eucla","525,1,s":"Australia/Eucla","540,1":"Asia/Yakutsk","540,0":"Asia/Tokyo","570,0":"Australia/Darwin","570,1,s":"Australia/Adelaide","600,0":"Australia/Brisbane","600,1":"Asia/Vladivostok","600,1,s":"Australia/Sydney","630,1,s":"Australia/Lord_Howe","660,1":"Asia/Kamchatka","660,0":"Pacific/Noumea","690,0":"Pacific/Norfolk","720,1,s":"Pacific/Auckland","720,0":"Pacific/Tarawa","765,1,s":"Pacific/Chatham","780,0":"Pacific/Tongatapu","780,1,s":"Pacific/Apia","840,0":"Pacific/Kiritimati"},typeof exports!="undefined"?exports.jstz=t:e.jstz=t})(this);

/* session states */
var OSRF_APP_SESSION_CONNECTED = 0;
var OSRF_APP_SESSION_CONNECTING = 1;
var OSRF_APP_SESSION_DISCONNECTED = 2;

/* types of transport layers */
var OSRF_TRANSPORT_TYPE_XHR = 1;
var OSRF_TRANSPORT_TYPE_XMPP = 2;
var OSRF_TRANSPORT_TYPE_WS = 3;
var OSRF_TRANSPORT_TYPE_WS_SHARED = 4;

/* message types */
var OSRF_MESSAGE_TYPE_REQUEST = 'REQUEST';
var OSRF_MESSAGE_TYPE_STATUS = 'STATUS';
var OSRF_MESSAGE_TYPE_RESULT = 'RESULT';
var OSRF_MESSAGE_TYPE_CONNECT = 'CONNECT';
var OSRF_MESSAGE_TYPE_DISCONNECT = 'DISCONNECT';

/* message statuses */
var OSRF_STATUS_CONTINUE = 100;
var OSRF_STATUS_OK = 200;
var OSRF_STATUS_ACCEPTED = 202;
var OSRF_STATUS_COMPLETE = 205;
var OSRF_STATUS_REDIRECTED = 307;
var OSRF_STATUS_BADREQUEST = 400;
var OSRF_STATUS_UNAUTHORIZED = 401;
var OSRF_STATUS_FORBIDDEN = 403;
var OSRF_STATUS_NOTFOUND = 404;
var OSRF_STATUS_NOTALLOWED = 405;
var OSRF_STATUS_TIMEOUT = 408;
var OSRF_STATUS_EXPFAILED = 417;
var OSRF_STATUS_INTERNALSERVERERROR = 500;
var OSRF_STATUS_NOTIMPLEMENTED = 501;
var OSRF_STATUS_VERSIONNOTSUPPORTED = 505;

// TODO: get path from ./configure prefix
var SHARED_WORKER_LIB = '/js/dojo/opensrf/opensrf_ws_shared.js'; 

/* The following classes map directly to network-serializable opensrf objects */

function osrfMessage(hash) {
    this.hash = hash;
    if(!this.hash.locale)
        this.hash.locale = OpenSRF.locale || 'en-US';
    if(!this.hash.tz)
        this.hash.tz = jstz.determine().name()
    this._encodehash = true;
}
osrfMessage.prototype.threadTrace = function(d) { 
    if(arguments.length == 1) 
        this.hash.threadTrace = d; 
    return this.hash.threadTrace; 
};
osrfMessage.prototype.type = function(d) { 
    if(arguments.length == 1) 
        this.hash.type = d; 
    return this.hash.type; 
};
osrfMessage.prototype.payload = function(d) { 
    if(arguments.length == 1) 
        this.hash.payload = d; 
    return this.hash.payload; 
};
osrfMessage.prototype.locale = function(d) { 
    if(arguments.length == 1) 
        this.hash.locale = d; 
    return this.hash.locale; 
};
osrfMessage.prototype.tz = function(d) { 
    if(arguments.length == 1) 
        this.hash.tz = d; 
    return this.hash.tz; 
};
osrfMessage.prototype.api_level = function(d) { 
    if(arguments.length == 1) 
        this.hash.api_level = d; 
    return this.hash.api_level; 
};
osrfMessage.prototype.serialize = function() {
    return {
        "__c":"osrfMessage",
        "__p": {
            'threadTrace' : this.hash.threadTrace,
            'type' : this.hash.type,
            'payload' : (this.hash.payload) ? this.hash.payload.serialize() : 'null',
            'locale' : this.hash.locale,
            'tz' : this.hash.tz,
            'api_level' : this.hash.api_level
        }
    };
};

function osrfMethod(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfMethod.prototype.method = function(d) {
    if(arguments.length == 1) 
        this.hash.method = d; 
    return this.hash.method; 
};
osrfMethod.prototype.params = function(d) {
    if(arguments.length == 1) 
        this.hash.params = d; 
    return this.hash.params; 
};
osrfMethod.prototype.serialize = function() {
    return {
        "__c":"osrfMethod",
        "__p": {
            'method' : this.hash.method,
            'params' : this.hash.params
        }
    };
};

function osrfMethodException(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfMethodException.prototype.status = function(d) {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
};
osrfMethodException.prototype.statusCode = function(d) {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
};
function osrfConnectStatus(hash) { 
    this.hash = hash;
    this._encodehash = true;
}
osrfConnectStatus.prototype.status = function(d) {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
};
osrfConnectStatus.prototype.statusCode = function(d) {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
};
function osrfResult(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfResult.prototype.status = function(d) {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
};
osrfResult.prototype.statusCode = function(d) {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
};
osrfResult.prototype.content = function(d) {
    if(arguments.length == 1) 
        this.hash.content = d; 
    return this.hash.content; 
};
function osrfServerError(hash) { 
    this.hash = hash;
    this._encodehash = true;
}
osrfServerError.prototype.status = function(d) {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
};
osrfServerError.prototype.statusCode = function(d) {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
};
function osrfContinueStatus(hash) { 
    this.hash = hash;
    this._encodehash = true;
}
osrfContinueStatus.prototype.status = function(d) {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
};
osrfContinueStatus.prototype.statusCode = function(d) {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
};

OpenSRF = {};
OpenSRF.tz = jstz.determine().name();
OpenSRF.locale = null;
OpenSRF.api_level = 1;

/* makes cls a subclass of pcls */
OpenSRF.set_subclass = function(cls, pcls) {
    var str = cls+'.prototype = new '+pcls+'();';
    str += cls+'.prototype.constructor = '+cls+';';
    str += cls+'.baseClass = '+pcls+'.prototype.constructor;';
    str += cls+'.prototype["super"] = '+pcls+'.prototype;';
    eval(str);
};


/* general session superclass */
OpenSRF.Session = function() {
    this.remote_id = null;
    this.state = OSRF_APP_SESSION_DISCONNECTED;
};

OpenSRF.Session.transport = OSRF_TRANSPORT_TYPE_XHR;
OpenSRF.Session.cache = {};

OpenSRF.Session.find_session = function(thread_trace) {
    return OpenSRF.Session.cache[thread_trace];
};
OpenSRF.Session.prototype.cleanup = function() {
    delete OpenSRF.Session.cache[this.thread];
};

OpenSRF.Session.prototype.send = function(osrf_msg, args) {
    args = (args) ? args : {};
    switch(OpenSRF.Session.transport) {
        case OSRF_TRANSPORT_TYPE_WS:
            return this.send_ws(osrf_msg);
        case OSRF_TRANSPORT_TYPE_WS_SHARED:
            return this.send_ws_shared(osrf_msg);
        case OSRF_TRANSPORT_TYPE_XHR:
            return this.send_xhr(osrf_msg, args);
        case OSRF_TRANSPORT_TYPE_XMPP:
            return this.send_xmpp(osrf_msg, args);
    }
};

OpenSRF.Session.prototype.send_xhr = function(osrf_msg, args) {
    args.thread = this.thread;
    args.rcpt = this.remote_id;
    args.rcpt_service = this.service;
    new OpenSRF.XHRequest(osrf_msg, args).send();
};

OpenSRF.websocketConnected = function() {
    return OpenSRF.sharedWebsocketConnected || (
        OpenSRF.websocketConnection && 
        OpenSRF.websocketConnection.connected()
    );
}

OpenSRF.Session.prototype.send_ws = function(osrf_msg) {

    // XXX there appears to be a bug in Chromium where loading the
    // same page multiple times (without a refresh or cache clear)
    // causes the SharedWorker to fail to instantiate on 
    // every other page load.  Disabling SharedWorker's entirely
    // for now.
    if (false /* ^-- */ && typeof SharedWorker == 'function' 

        /*
         * https://bugzilla.mozilla.org/show_bug.cgi?id=504553#c73
         * Firefox does not yet support WebSockets in worker threads
         */
        && !navigator.userAgent.match(/Firefox/)
    ) {
        // vanilla websockets requested, but this browser supports
        // shared workers, so use those instead.
        return this.send_ws_shared(osrf_msg);
    }

    // otherwise, use a per-tab connection

    if (!OpenSRF.websocketConnection) {
        this.setup_single_ws();
    }

    var json = js2JSON({
        service : this.service,
        thread : this.thread,
        osrf_msg : [osrf_msg.serialize()]
    });

    OpenSRF.websocketConnection.send(json);
};

OpenSRF.Session.prototype.setup_single_ws = function() {
    OpenSRF.websocketConnection = new OpenSRF.WebSocket();

    OpenSRF.websocketConnection.onmessage = function(msg) {
        try {
            var msg = JSON2js(msg);
        } catch(E) {
            console.error(
                "Error parsing JSON in shared WS response: " + msg);
            throw E;
        }
        OpenSRF.Stack.push(                                                        
            new OpenSRF.NetMessage(                                                
               null, null, msg.thread, null, msg.osrf_msg)                        
        ); 

        return;
    }
}

OpenSRF.Session.setup_shared_ws = function() {
    OpenSRF.Session.transport = OSRF_TRANSPORT_TYPE_WS_SHARED;

    OpenSRF.sharedWSWorker = new SharedWorker(SHARED_WORKER_LIB);

    OpenSRF.sharedWSWorker.port.addEventListener('message', function(e) {                          
        var data = e.data;

        if (data.action == 'message') {
            // pass all inbound message up the opensrf stack

            OpenSRF.sharedWebsocketConnected = true;
            var msg;
            try {
                msg = JSON2js(data.message);
            } catch(E) {
                console.error(
                    "Error parsing JSON in shared WS response: " + msg);
                throw E;
            }
            OpenSRF.Stack.push(                                                        
                new OpenSRF.NetMessage(                                                
                   null, null, msg.thread, null, msg.osrf_msg)                        
            ); 

            return;
        }


        if (data.action == 'event') {
            if (data.type.match(/onclose|onerror/)) {
                OpenSRF.sharedWebsocketConnected = false;
                if (OpenSRF.onWebSocketClosed)
                    OpenSRF.onWebSocketClosed();
                if (data.type.match(/onerror/)) 
                    throw new Error(data.message);
            }
        }
    });

    OpenSRF.sharedWSWorker.port.start();   
}

OpenSRF.Session.prototype.send_ws_shared = function(message) {

    if (!OpenSRF.sharedWSWorker) 
        OpenSRF.Session.setup_shared_ws();

    var json = js2JSON({
        service : this.service,
        thread : this.thread,
        osrf_msg : [message.serialize()]
    });

    OpenSRF.sharedWSWorker.port.postMessage({
        action : 'message', 
        // pass the thread additionally as a stand-alone value so the
        // worker can more efficiently inspect it.
        thread : this.thread,
        message : json
    });
}


OpenSRF.Session.prototype.send_xmpp = function(osrf_msg, args) {
    alert('xmpp transport not implemented');
};


/* client sessions make requests */
OpenSRF.ClientSession = function(service) {
    this.service = service;
    this.remote_id = null;
    this.locale = OpenSRF.locale || 'en-US';
    this.tz = OpenSRF.tz;
    this.last_id = 0;
    this.requests = [];
    this.onconnect = null;
    this.thread = Math.random() + '' + new Date().getTime();
    OpenSRF.Session.cache[this.thread] = this;
};
OpenSRF.set_subclass('OpenSRF.ClientSession', 'OpenSRF.Session');


OpenSRF.ClientSession.prototype.connect = function(args) {
    args = (args) ? args : {};
    this.remote_id = null;

    if (this.state == OSRF_APP_SESSION_CONNECTED) {
        if (args.onconnect) args.onconnect();
        return true;
    }

    if(args.onconnect) {
        this.onconnect = args.onconnect;

    } else {
        /* if no handler is provided, make this a synchronous call */
        this.timeout = (args.timeout) ? args.timeout : 5;
    }

    message = new osrfMessage({
        'threadTrace' : this.last_id++, 
        'type' : OSRF_MESSAGE_TYPE_CONNECT
    });

    this.send(message, {'timeout' : this.timeout});

    if(this.onconnect || this.state == OSRF_APP_SESSION_CONNECTED)
        return true;

    return false;
};

OpenSRF.ClientSession.prototype.disconnect = function(args) {

    if (this.state == OSRF_APP_SESSION_CONNECTED) {
        this.send(
            new osrfMessage({
                'threadTrace' : this.last_id++,
                'type' : OSRF_MESSAGE_TYPE_DISCONNECT
            })
        );
    }

    this.remote_id = null;
    this.state = OSRF_APP_SESSION_DISCONNECTED;
};


OpenSRF.ClientSession.prototype.request = function(args) {
    
    if(this.state != OSRF_APP_SESSION_CONNECTED)
        this.remote_id = null;
        
    if(typeof args == 'string') { 
        params = [];
        for(var i = 1; i < arguments.length; i++)
            params.push(arguments[i]);

        args = {
            method : args, 
            params : params
        };
    } else {
        if(typeof args == 'undefined')
            args = {};
    }

    var req = new OpenSRF.Request(this, this.last_id++, args);
    this.requests.push(req);
    return req;
};

OpenSRF.ClientSession.prototype.find_request = function(reqid) {
    for(var i = 0; i < this.requests.length; i++) {
        var req = this.requests[i];
        if(req.reqid == reqid)
            return req;
    }
    return null;
};

OpenSRF.Request = function(session, reqid, args) {
    this.session = session;
    this.reqid = reqid;

    /* callbacks */
    this.onresponse = args.onresponse;
    this.oncomplete = args.oncomplete;
    this.onerror = args.onerror;
    this.onmethoderror = args.onmethoderror;
    this.ontransporterror = args.ontransporterror;

    this.method = args.method;
    this.params = args.params;
    this.timeout = args.timeout;
    this.api_level = args.api_level || OpenSRF.api_level;
    this.response_queue = [];
    this.complete = false;
};

OpenSRF.Request.prototype.peek_last = function(timeout) {
    if(this.response_queue.length > 0) {
        var x = this.response_queue.pop();
        this.response_queue.push(x);
        return x;
    }
    return null;
};

OpenSRF.Request.prototype.peek = function(timeout) {
    if(this.response_queue.length > 0)
        return this.response_queue[0];
    return null;
};

OpenSRF.Request.prototype.recv = function(timeout) {
    if(this.response_queue.length > 0)
        return this.response_queue.shift();
    return null;
};

OpenSRF.Request.prototype.send = function() {
    method = new osrfMethod({'method':this.method, 'params':this.params});
    message = new osrfMessage({
        'threadTrace' : this.reqid, 
        'type' : OSRF_MESSAGE_TYPE_REQUEST, 
        'payload' : method, 
        'locale' : this.session.locale,
        'tz' : this.session.tz,
        'api_level' : this.api_level
    });

    this.session.send(message, {
        'timeout' : this.timeout,
        'onresponse' : this.onresponse,
        'oncomplete' : this.oncomplete,
        'onerror' : this.onerror,
        'onmethoderror' : this.onmethoderror,
        'ontransporterror' : this.ontransporterror
    });
};

OpenSRF.NetMessage = function(to, from, thread, body, osrf_msg) {
    this.to = to;
    this.from = from;
    this.thread = thread;
    this.body = body;
    this.osrf_msg = osrf_msg;
};

OpenSRF.Stack = function() {
};

// global inbound message queue
OpenSRF.Stack.queue = [];

// XXX testing
function log(msg) {
    try {
        dump(msg + '\n'); // xulrunner
    } catch(E) {
        console.log(msg);
    }
}

// ses may be passed to us by the network handler
OpenSRF.Stack.push = function(net_msg, callbacks) {
    var ses = OpenSRF.Session.find_session(net_msg.thread); 
    if (!ses) return;
    ses.remote_id = net_msg.from;

    // NetMessage's from websocket connections are parsed before they get here
    osrf_msgs = net_msg.osrf_msg;

    if (!osrf_msgs) {

        try {
            osrf_msgs = JSON2js(net_msg.body);

            // TODO: pretty sure we don't need this..
            if (OpenSRF.Session.transport == OSRF_TRANSPORT_TYPE_WS) {
                // WebSocketRequests wrap the content
                osrf_msgs = osrf_msgs.osrf_msg;
            }

        } catch(E) {
            log('Error parsing OpenSRF message body as JSON: ' + net_msg.body + '\n' + E);

            /** UGH
              * For unknown reasons, the Content-Type header will occasionally
              * be included in the XHR.responseText for multipart/mixed messages.
              * When this happens, strip the header and newlines from the message
              * body and re-parse.
              */
            net_msg.body = net_msg.body.replace(/^.*\n\n/, '');
            log('Cleaning up and retrying...');

            try {
                osrf_msgs = JSON2js(net_msg.body);
            } catch(E2) {
                log('Unable to clean up message, giving up: ' + net_msg.body);
                return;
            }
        }
    }

    // push the latest responses onto the end of the inbound message queue
    for(var i = 0; i < osrf_msgs.length; i++)
        OpenSRF.Stack.queue.push({msg : osrf_msgs[i], ses : ses});

    // continue processing responses, oldest to newest
    while(OpenSRF.Stack.queue.length) {
        var data = OpenSRF.Stack.queue.shift();
        OpenSRF.Stack.handle_message(data.ses, data.msg);
    }
};

OpenSRF.Stack.handle_message = function(ses, osrf_msg) {
    
    var req = ses.find_request(osrf_msg.threadTrace());

    if(osrf_msg.type() == OSRF_MESSAGE_TYPE_STATUS) {

        var payload = osrf_msg.payload();
        var status = payload.statusCode();
        var status_text = payload.status();

        if(status == OSRF_STATUS_COMPLETE) {
            if(req) {
                req.complete = true;
                if(req.oncomplete && !req.oncomplete_called) {
                    req.oncomplete_called = true;
                    return req.oncomplete(req);
                }
            }
        }

        if(status == OSRF_STATUS_OK) {
            ses.state = OSRF_APP_SESSION_CONNECTED;

            /* call the connect callback */
            if(ses.onconnect && !ses.onconnect_called) {
                ses.onconnect_called = true;
                return ses.onconnect();
            }
        }

        // capture all 400's and 500's as method errors
        if ((status+'').match(/^4/) || (status+'').match(/^5/)) {
            if(req && req.onmethoderror) 
                return req.onmethoderror(req, status, status_text);
        }
    }

    if(osrf_msg.type() == OSRF_MESSAGE_TYPE_RESULT) {
        if(req) {
            req.response_queue.push(osrf_msg.payload());
            if(req.onresponse) {
                return req.onresponse(req);
            }
        }
    }
};


