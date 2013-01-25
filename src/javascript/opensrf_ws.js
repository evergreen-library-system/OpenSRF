/* -----------------------------------------------------------------------
 * Copyright (C) 2012  Equinox Software, Inc.
 * Bill Erickson <berick@esilibrary.com>
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

// opensrf defaults
var WEBSOCKET_URL_PATH = '/osrf-websocket-translator';
var WEBSOCKET_PORT = 7680;
var WEBSOCKET_PORT_SSL = 7682;


// Create the websocket and connect to the server
// args.onopen is required
// if args.default is true, use the default connection
OpenSRF.WebSocketConnection = function(args, handlers) {
    args = args || {};
    this.handlers = handlers;

    var secure = (args.ssl || location.protocol == 'https');
    var path = args.path || WEBSOCKET_URL_PATH;
    var port = args.port || (secure ? WEBSOCKET_PORT_SSL : WEBSOCKET_PORT);
    var host = args.host || location.host;
    var proto = (secure) ? 'wss' : 'ws';
    this.path = proto + '://' + host + ':' + port + path;

    this.setupSocket();
    OpenSRF.WebSocketConnection.pool[args.name] = this;
};

// global pool of connection objects; name => connection map
OpenSRF.WebSocketConnection.pool = {};

OpenSRF.WebSocketConnection.defaultConnection = function() {
    return OpenSRF.WebSocketConnection.pool['default'];
}

/**
 * create a new WebSocket.  useful for new connections or 
 * applying a new socket to an existing connection (whose 
 * socket was disconnected)
 */
OpenSRF.WebSocketConnection.prototype.setupSocket = function() {

    try {
        this.socket = new WebSocket(this.path);
    } catch(e) {
        throw new Error("WebSocket() not supported in this browser: " + e);
    }

    this.socket.onopen = this.handlers.onopen;
    this.socket.onmessage = this.handlers.onmessage;
    this.socket.onerror = this.handlers.onerror;
    this.socket.onclose = this.handlers.onclose;
};

/** default onmessage handler: push the message up the opensrf stack */
OpenSRF.WebSocketConnection.default_onmessage = function(evt) {
    //console.log('receiving: ' + evt.data);
    var msg = JSON2js(evt.data);
    OpenSRF.Stack.push(
        new OpenSRF.NetMessage(
            null, null, msg.thread, null, msg.osrf_msg)
    );
};

/** default error handler */
OpenSRF.WebSocketConnection.default_onerror = function(evt) {
    throw new Error("WebSocket Error " + evt + ' : ' + evt.data);
};


/** shut it down */
OpenSRF.WebSocketConnection.prototype.destroy = function() {
    this.socket.close();
    delete OpenSRF.WebSocketConnection.pool[this.name];
};

/**
 * Creates the request object, but does not connect or send anything
 * until the first call to send().
 */
OpenSRF.WebSocketRequest = function(session, onopen, connectionArgs) {
    this.session = session;
    this.onopen = onopen;
    this.setupConnection(connectionArgs || {});
}

OpenSRF.WebSocketRequest.prototype.setupConnection = function(args) {
    var self = this;

    var cname = args.name || 'default';
    this.wsc = OpenSRF.WebSocketConnection.pool[cname];

    if (this.wsc) { // we have a WebSocketConnection.  

        switch (this.wsc.socket.readyState) {

            case this.wsc.socket.CONNECTING:
                // replace the original onopen handler with a new combined handler
                var orig_open = this.wsc.socket.onopen;
                this.wsc.socket.onopen = function() {
                    orig_open();
                    self.onopen(self);
                };
                break;

            case this.wsc.socket.OPEN:
                // user is expecting an onopen event.  socket is 
                // already open, so we have to manufacture one.
                this.onopen(this);
                break;

            default:
                console.log('WebSocket is no longer connecting; reconnecting');
                this.wsc.setupSocket();
        }

    } else { // no connection found

        if (cname == 'default' || args.useDefaultHandlers) { // create the default handle 

            this.wsc = new OpenSRF.WebSocketConnection(
                {name : cname}, {
                    onopen : function(evt) {if (self.onopen) self.onopen(self)},
                    onmessage : OpenSRF.WebSocketConnection.default_onmessage,
                    onerror : OpenSRF.WebSocketRequest.default_onerror,
                    onclose : OpenSRF.WebSocketRequest.default_onclose
                } 
            );

        } else {
            throw new Error("No such WebSocketConnection '" + cname + "'");
        }
    }
}


OpenSRF.WebSocketRequest.prototype.send = function(message) {
    var wrapper = {
        service : this.session.service,
        thread : this.session.thread,
        osrf_msg : [message.serialize()]
    };

    var json = js2JSON(wrapper);
    //console.log('sending: ' + json);

    // drop it on the wire
    this.wsc.socket.send(json);
    return this;
};




