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

var WS_PATH = '/osrf-websocket';

/**
 * onopen is required. no data can be sent 
 * until the async connection dance completes.
 */
OpenSRF.WSRequest = function(session, args, onopen) {
    this.session = session;
    this.args = args;

    var proto = location.protocol == 'https' ? 'wss' : 'ws';

    var path = proto + '://' + location.host + 
        WS_PATH + '?service=' + this.session.service;

    try {
        this.ws = new WebSocket(path);
    } catch(e) {
        throw new Error("WebSocket() not supported in this browser " + e);
    }

    var self = this;

    this.ws.onopen = function(evt) {
        onopen(self);
    }

    this.ws.onmessage = function(evt) {
        self.core_handler(evt.data);
    }

    this.ws.onerror = function(evt) {
        self.transport_error_handler(evt.data);
    }

    this.ws.onclose = function(evt) {
    }
};

OpenSRF.WSRequest.prototype.send = function(message) {
    //console.log('sending: ' + js2JSON([message.serialize()]));
    this.last_message = message;
    this.ws.send(js2JSON([message.serialize()]));
    return this;
};

OpenSRF.WSRequest.prototype.close = function() {
    try { this.ws.close(); } catch(e) {}
}

OpenSRF.WSRequest.prototype.core_handler = function(json) {
    //console.log('received: ' + json);

    OpenSRF.Stack.push(
        new OpenSRF.NetMessage(null, null, '', json),
        {
            onresponse : this.args.onresponse,
            oncomplete : this.args.oncomplete,
            onerror : this.args.onerror,
            onmethoderror : this.method_error_handler()
        },
        this.args.session
    );
};


OpenSRF.WSRequest.prototype.method_error_handler = function() {
    var self = this;
    return function(req, status, status_text) {
        if(self.args.onmethoderror) 
            self.args.onmethoderror(req, status, status_text);

        if(self.args.onerror)  {
            self.args.onerror(
                self.last_message, self.session.service, '');
        }
    };
};

OpenSRF.WSRequest.prototype.transport_error_handler = function(msg) {
    if(this.args.ontransporterror) {
        this.args.ontransporterror(msg);
    }
    if(this.args.onerror) {
        this.args.onerror(msg, this.session.service, '');
    }
};


