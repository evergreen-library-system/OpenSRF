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

/* session states */
var OSRF_APP_SESSION_CONNECTED = 0;
var OSRF_APP_SESSION_CONNECTING = 1;
var OSRF_APP_SESSION_DISCONNECTED = 2;

/* types of transport layers */
var OSRF_TRANSPORT_TYPE_XHR = 1;
var OSRF_TRANSPORT_TYPE_XMPP = 2;

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

var OpenSRF = {};

/* makes cls a subclass of pcls */
OpenSRF.set_subclass = function(cls, pcls) {
    var str = cls+'.prototype = new '+pcls+'();';
    str += cls+'.prototype.constructor = '+cls+';';
    str += cls+'.baseClass = '+pcls+'.prototype.constructor;';
    str += cls+'.prototype.super = '+pcls+'.prototype;';
    eval(str);
}


/* general session superclass */
OpenSRF.Session = function() {
    this.remote_id = null;
}

OpenSRF.Session.transport = OSRF_TRANSPORT_TYPE_XHR; /* default to XHR */
OpenSRF.Session.cache = {};
OpenSRF.Session.find_session = function(thread_trace) {
    return OpenSRF.Session.cache[thread_trace];
}
OpenSRF.Session.prototype.cleanup = function() {
    delete OpenSRF.Session.cache[this.thread];
}

OpenSRF.Session.prototype.send = function(osrf_msg, args) {
    switch(OpenSRF.Session.transport) {
        case OSRF_TRANSPORT_TYPE_XHR:
            return this.send_xhr(osrf_msg, args);
        case OSRF_TRANSPORT_TYPE_XMPP:
            return this.send_xmpp(osrf_msg, args);
    }
}

OpenSRF.Session.prototype.send_xhr = function(osrf_msg, args) {
    args.thread = this.thread;
    args.rcpt = this.remote_id;
    args.rcpt_service = this.service;
    new OpenSRF.XHRequest(osrf_msg, args).send();
}

OpenSRF.Session.prototype.send_xmpp = function(osrf_msg, args) {
    alert('xmpp transport not yet implemented');
}


/* client sessions make requests */
OpenSRF.ClientSession = function(service) {
    this.service = service
    this.remote_id = null;
    this.locale = 'en-US';
    this.last_id = 0;
    this.thread = Math.random() + '' + new Date().getTime();
    this.do_connect = false;
    this.requests = [];
    OpenSRF.Session.cache[this.thread] = this;
}
OpenSRF.set_subclass('OpenSRF.ClientSession', 'OpenSRF.Session');


OpenSRF.ClientSession.prototype.request = function(args) {

    if(typeof args == 'string') { 
        params = [];
        for(var i = 1; i < arguments.length; i++)
            params.push(arguments[i]);

        args = {
            method : args, 
            params : params
        };
    }

    var req = new OpenSRF.Request(this, this.last_id++, args);
    this.requests.push(req);
    return req;
}

OpenSRF.ClientSession.prototype.find_request = function(reqid) {
    for(var i = 0; i < this.requests.length; i++) {
        var req = this.requests[i];
        if(req.reqid == reqid)
            return req;
    }
    return null;
}

OpenSRF.Request = function(session, reqid, args) {
    this.session = session;
    this.reqid = reqid;
    this.onresponse = args.onresponse;
    this.onerror = args.onerror;
    this.oncomplete = args.oncomplete;
    this.method = args.method;
    this.params = args.params;
    this.timeout = args.timeout;
    this.response_queue = [];
    this.complete = false;
}

OpenSRF.Request.prototype.recv = function(timeout) {
    if(this.response_queue.length > 0)
        return this.response_queue.shift();
}

OpenSRF.Request.prototype.send = function() {
    method = new osrfMethod({'method':this.method, 'params':this.params});
    message = new osrfMessage({
        'threadTrace' : this.reqid, 
        'type' : OSRF_MESSAGE_TYPE_REQUEST, 
        'payload' : method, 
        'locale' : this.session.locale
    });

    this.session.send(message, {
        'timeout' : this.timeout,
        'onresponse' : this.onresponse,
        'oncomplete' : this.oncomplete,
        'onerror' : this.onerror
    });
}

OpenSRF.NetMessage = function(to, from, thread, body) {
    this.to = to;
    this.from = from;
    this.thread = thread;
    this.body = body;
}

OpenSRF.Stack = function() {
}

OpenSRF.Stack.push = function(net_msg, stack_callback) {
    var ses = OpenSRF.Session.find_session(net_msg.thread); 
    if(!ses) return;
    ses.remote_id = net_msg.sender;
    osrf_msgs = JSON2js(net_msg.body);
    for(var i = 0; i < osrf_msgs.length; i++) 
        OpenSRF.Stack.handle_message(ses, osrf_msgs[i], stack_callback);        
}

OpenSRF.Stack.handle_message = function(ses, osrf_msg, stack_callback) {
    
    var req = null;

    if(osrf_msg.type() == OSRF_MESSAGE_TYPE_STATUS) {
        var payload = osrf_msg.payload();
        var status = payload.statusCode();
        var status_text = payload.status();

        if(status == OSRF_STATUS_NOTFOUND) {
            alert('status = ' + status_text);
        }
    }

    if(osrf_msg.type() == OSRF_MESSAGE_TYPE_RESULT) {
        req = ses.find_request(osrf_msg.threadTrace());
        if(req) {
            req.response_queue.push(osrf_msg.payload());
        }
    }

    if(stack_callback)
        stack_callback(ses, req);
}

/* The following classes map directly to network-serializable opensrf objects */

function osrfMessage(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfMessage.prototype.threadTrace = function(d) { 
    if(arguments.length == 1) 
        this.hash.threadTrace = d; 
    return this.hash.threadTrace; 
}
osrfMessage.prototype.type = function(d) { 
    if(arguments.length == 1) 
        this.hash.type = d; 
    return this.hash.type; 
}
osrfMessage.prototype.payload = function(d) { 
    if(arguments.length == 1) 
        this.hash.payload = d; 
    return this.hash.payload; 
}
osrfMessage.prototype.locale = function(d) { 
    if(arguments.length == 1) 
        this.hash.locale = d; 
    return this.hash.locale; 
}
osrfMessage.prototype.serialize = function() {
    return {
        "__c":"osrfMessage",
        "__p": {
            'threadTrace' : this.hash.threadTrace,
            'type' : this.hash.type,
            'payload' : this.hash.payload.serialize(),
            'locale' : this.hash.locale
        }
    };
}

function osrfMethod(hash) {
    this.hash = hash;
    this._encodehash = true;
} 
osrfMethod.prototype.method = function() {
    if(arguments.length == 1) 
        this.hash.method = d; 
    return this.hash.method; 
}
osrfMethod.prototype.params = function() {
    if(arguments.length == 1) 
        this.hash.params = d; 
    return this.hash.params; 
}
osrfMethod.prototype.serialize = function() {
    return {
        "__c":"osrfMethod",
        "__p": {
            'method' : this.hash.method,
            'params' : this.hash.params
        }
    };
}

function osrfMethodException(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfMethodException.prototype.status = function() {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
}
osrfMethodException.prototype.statusCode = function() {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
}
function osrfConnectStatus(hash) { 
    this.hash = hash;
    this._encodehash = true;
}
osrfConnectStatus.prototype.status = function() {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
}
osrfConnectStatus.prototype.statusCode = function() {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
}
function osrfResult(hash) {
    this.hash = hash;
    this._encodehash = true;
}
osrfResult.prototype.status = function() {
    if(arguments.length == 1) 
        this.hash.status = d; 
    return this.hash.status; 
}
osrfResult.prototype.statusCode = function() {
    if(arguments.length == 1) 
        this.hash.statusCode = d; 
    return this.hash.statusCode; 
}
osrfResult.prototype.content = function() {
    if(arguments.length == 1) 
        this.hash.content = d; 
    return this.hash.content; 
}



