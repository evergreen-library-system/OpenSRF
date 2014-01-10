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

var WEBSOCKET_URL_PATH = '/osrf-websocket-translator';
var WEBSOCKET_PORT = 7680;
var WEBSOCKET_PORT_SSL = 7682;

// set of shared ports (i.e. browser tabs)
var connected_ports = {};
var port_identifier = 0;

// maps osrf message threads to a port index in connected_ports
var thread_port_map = {}; 

// our shared websocket
var websocket;

function send_msg_to_port(ident, msg) {
    console.debug('sending msg to port ' + ident + ' : ' + msg.action);
    try {
        connected_ports[ident].postMessage(msg);
    } catch(E) {
        // some browsers (Opera) throw an exception when messaging
        // a disconnected port.
        console.log('unable to send msg to port ' + ident);
        delete connected_ports[ident];
    }
}

// send a message to all listeners
function broadcast(msg) {
    for (var ident in connected_ports)
      send_msg_to_port(ident, msg);
}


// opens the websocket connection
// port_ident refers to the requesting port
function open_websocket(port_ident) {
    var port = connected_ports[port_ident];

    if (websocket) {
        switch (websocket.readyState) {

            case websocket.CONNECTING:
                // nothing to do.  The port will get notified on open
                return;

            case websocket.OPEN:
                // websocket is already open, let the connecting.
                // other ports have been notified already, so
                // no broadcast is required.
                send_msg_to_port(port_ident, {action : 'socket_connected'});
                return;

            default:
                // websocket is no longer connected.  We need a new socket.
                websocket = null;
        }
    }

    // TODO:
    // assume non-SSL for now.  SSL silently dies if the cert is
    // invalid and has not been added as an exception.
    var path = 'ws://' + location.host + ':' + 
        WEBSOCKET_PORT + WEBSOCKET_URL_PATH

    console.log('connecting websocket to ' + path);

    websocket = new WebSocket(path);

    websocket.onopen = function() {
        // tell all ports the websocket is open and ready
        console.log('websocket.onopen()');
        broadcast({action : 'socket_connected'});
    }

    websocket.onmessage = function(evt) {
        var message = evt.data;

        // this is a hack to avoid having to run JSON2js multiple 
        // times on the same message.  Hopefully match() is faster.
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
         * thread_port_map as we go, because that gets messy.  Instead,
         * after the map has reached a certain size, clear it.  If any
         * pending messages are afield that depend on the map, they 
         * will be broadcast to all ports on arrival (see above).  Only the 
         * port expecting a message with the given thread will honor the 
         * message, all other ports will drop it silently.  We could just 
         * do that for every messsage, but this is more efficient.
         */
        if (Object.keys(thread_port_map).length > 1000) {
            thread_port_map = {};
        }
    }

    websocket.onerror = function(evt) {
        var err = "WebSocket Error " + evt + ' : ' + evt.data;
        // propagate to all ports so it can be logged, etc. 
        broadcast({action : 'error', message : err});
        throw new Error(err);
    }

    websocket.onclose = function() {
        console.log('closing websocket');
    }
}

// called when a new port (tab) is opened
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
            websocket.send(data.message);
            return;
        } 

        if (messsage.action == 'close') {
            // TODO: add me to body onunload in calling pages.
            delete connected_ports[port_ident];
            console.log('closed port ' + port_ident + 
                '; ' + Object.keys(connected_ports).length + ' remaining');
            return;
        }

    }, false);

    port.start();

    console.log('added port ' + port_ident + 
      '; ' + Object.keys(connected_ports).length + ' total');

    open_websocket(port_ident);
}

