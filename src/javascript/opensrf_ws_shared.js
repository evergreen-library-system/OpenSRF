/* -----------------------------------------------------------------------
 * Copyright (C) 2014  Equinox Software, Inc.
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


/**
 * Shared WebSocket communication layer.  Each browser tab registers with
 * this code all inbound / outbound messages are delivered through a
 * single websocket connection managed within.
 *
 * Messages take the form : {action : my_action, message : my_message}
 * actions for tab-generated messages may be "message" or "close".
 * actions for messages generated within may be "message" or "error"
 */

var WEBSOCKET_URL_PATH = '/osrf-websocket-translator';
var WEBSOCKET_PORT_SSL = 7682;
var WEBSOCKET_MAX_THREAD_PORT_CACHE_SIZE = 1000;

/**
 * Collection of shared ports (browser tabs)
 */
var connected_ports = {};

/**
 * Each port gets a local identifier so we have an easy way to refer to 
 * it later.
 */
var port_identifier = 0;

// maps osrf message threads to a port index in connected_ports.
// this is how we know which browser tab to deliver messages to.
var thread_port_map = {}; 

/**
 * Browser-global, shared websocket connection.
 */
var websocket;

/** 
 * Pending messages awaiting a successful websocket connection
 *
 * instead of asking the caller to pass messages after a connection
 * is made, queue the messages for the caller and deliver them
 * after the connection is established.
 */
var pending_ws_messages = [];

/** 
 * Deliver the message blob to the specified port (tab)
 */
function send_msg_to_port(ident, msg) {
    console.debug('sending msg to port ' + ident + ' : ' + msg.action);
    try {
        connected_ports[ident].postMessage(msg);
    } catch(E) {
        // some browsers (Opera) throw an exception when messaging
        // a disconnected port.
        console.debug('unable to send msg to port ' + ident);
        delete connected_ports[ident];
    }
}

/**
 * Send a message blob to all ports (tabs)
 */
function broadcast(msg) {
    for (var ident in connected_ports)
      send_msg_to_port(ident, msg);
}


/**
 * Opens the websocket connection.
 *
 * If our global socket is already open, use it.  Otherwise, queue the 
 * message for delivery after the socket is open.
 */
function send_to_websocket(message) {

    if (websocket && websocket.readyState == websocket.OPEN) {
        // websocket connection is viable.  send our message now.
        websocket.send(message);
        return;
    }

    // no viable connection. queue our outbound messages for future delivery.
    pending_ws_messages.push(message);

    if (websocket && websocket.readyState == websocket.CONNECTING) {
        // we are already in the middle of a setup call.  
        // our queued message will be delivered after setup completes.
        return;
    }

    // we have no websocket or an invalid websocket.  build a new one.

    // TODO:
    // assume non-SSL for now.  SSL silently dies if the cert is
    // invalid and has not been added as an exception.  need to
    // explain / document / avoid this better.
    var path = 'wss://' + location.host + ':' + 
        WEBSOCKET_PORT_SSL + WEBSOCKET_URL_PATH;

    console.debug('connecting websocket to ' + path);

    try {
        websocket = new WebSocket(path);
    } catch(E) {
        console.log('Error creating WebSocket for path ' + path + ' : ' + E);
        throw new Error(E);
    }

    websocket.onopen = function() {
        console.debug('websocket.onopen()');
        // deliver any queued messages
        var msg;
        while ( (msg = pending_ws_messages.shift()) )
            websocket.send(msg);

    }

    websocket.onmessage = function(evt) {
        var message = evt.data;

        // this is sort of a hack to avoid having to run JSON2js
        // multiple times on the same message.  Hopefully match() is
        // faster.  Note: We can't use JSON_v1 within a shared worker
        // for marshalling messages, because it has no knowledge of
        // application-level class hints in this environment.
        var thread;
        var match = message.match(/"thread":"(.*?)"/);
        if (!match || !(thread = match[1])) {
            throw new Error("Websocket message malformed; no thread: " + message);
        }

        console.debug('websocket received message for thread ' + thread);

        var port_msg = {action: 'message', message : message};
        var port_ident = thread_port_map[thread];

        if (port_ident) {
            send_msg_to_port(port_ident, port_msg);
        } else {
            // don't know who it's for, broadcast and let the ports
            // sort it out for themselves.
            broadcast(port_msg);
        }

        /* poor man's memory management.  We are not cleaning up our
         * thread_port_map as we go, because that would require parsing
         * and analyzing every message to look for opensrf statuses.  
         * parsing messages adds overhead (see also above comments about
         * JSON_v1.js).  So, instead, after the map has reached a certain 
         * size, clear it.  If any pending messages are afield that depend 
         * on the map, they will be broadcast to all ports on arrival 
         * (see above).  Only the port expecting a message with the given 
         * thread will honor the message, all other ports will drop it 
         * silently.  We could just broadcastfor every messsage, but this 
         * is presumably more efficient.
         *
         * If for some reason this fails to work as expected, we could add
         * a new tab->ws message type for marking a thread as complete.
         * My hunch is this will be faster, since it will require a lot
         * fewer cross-tab messages overall.
         */
        if (Object.keys(thread_port_map).length > 
                WEBSOCKET_MAX_THREAD_PORT_CACHE_SIZE) {
            console.debug('resetting thread_port_map');
            thread_port_map = {};
        }
    }

    /**
     * Websocket error handler.  This type of error indicates a probelem
     * with the connection.  I.e. it's not port-specific. 
     * Broadcast to all ports.
     */
    websocket.onerror = function(evt) {
        var err = "WebSocket Error " + evt;
        console.error(err);
        broadcast({action : 'event', type : 'onerror', message : err});
        websocket.close(); // connection is no good; reset.
    }

    /**
     * Called when the websocket connection is closed.
     *
     * Once a websocket is closed, it will be re-opened the next time
     * a message delivery attempt is made.  Clean up and prepare to reconnect.
     */
    websocket.onclose = function() {
        console.debug('closing websocket');
        websocket = null;
        thread_port_map = {};
        broadcast({action : 'event', type : 'onclose'});
    }
}

/**
 * New port (tab) opened handler
 *
 * Apply the port identifier and message handlers.
 */
onconnect = function(e) {
    var port = e.ports[0];

    // we have no way of identifying ports within the message handler,
    // so we apply an identifier to each and toss that into a closer.
    var port_ident = port_identifier++;
    connected_ports[port_ident] = port;

    // message handler
    port.addEventListener('message', function(e) {
        var data = e.data;

        if (data.action == 'message') {
            thread_port_map[data.thread] = port_ident;
            send_to_websocket(data.message);
            return;
        } 

        if (messsage.action == 'close') {
            // TODO: all browser tabs need an onunload handler which sends
            // a action=close message, so that the port may be removed from
            // the conected_ports collection.
            delete connected_ports[port_ident];
            console.debug('closed port ' + port_ident + 
                '; ' + Object.keys(connected_ports).length + ' remaining');
            return;
        }

    }, false);

    port.start();

    console.debug('added port ' + port_ident + 
      '; ' + Object.keys(connected_ports).length + ' total');
}

