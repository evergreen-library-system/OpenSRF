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

var OSRF_HTTP_HEADER_TO = 'X-OpenSRF-to';
var OSRF_HTTP_HEADER_XID = 'X-OpenSRF-thread';
var OSRF_HTTP_HEADER_FROM = 'X-OpenSRF-from';
var OSRF_HTTP_HEADER_THREAD = 'X-OpenSRF-thread';
var OSRF_HTTP_HEADER_TIMEOUT = 'X-OpenSRF-timeout';
var OSRF_HTTP_HEADER_SERVICE = 'X-OpenSRF-service';
var OSRF_HTTP_HEADER_MULTIPART = 'X-OpenSRF-multipart';
var OSRF_HTTP_TRANSLATOR = '/osrf-http-translator'; /* XXX config */
var OSRF_POST_CONTENT_TYPE = 'application/x-www-form-urlencoded';


OpenSRF.XHRequest = function(osrf_msg, args) {
    this.message = osrf_msg;
    this.args = args;
    this.xreq = new XMLHttpRequest(); /* XXX browser check */
}

OpenSRF.XHRequest.prototype.send = function() {
    var xhr_req = this;
    var xreq = this.xreq

    xreq.multipart = true; /* XXX browser check */
    xreq.onload = function(evt) {xhr_req.core_handler(evt.target);}
    xreq.open('POST', OSRF_HTTP_TRANSLATOR, true);

    xreq.setRequestHeader('Content-Type', OSRF_POST_CONTENT_TYPE);
    xreq.setRequestHeader(OSRF_HTTP_HEADER_MULTIPART, 'true');
    xreq.setRequestHeader(OSRF_HTTP_HEADER_THREAD, this.args.thread);

    if(this.args.rcpt)
        xreq.setRequestHeader(OSRF_HTTP_HEADER_TO, this.args.rcpt);
    else
        xreq.setRequestHeader(OSRF_HTTP_HEADER_SERVICE, this.args.rcpt_service);

    var post = 'osrf-msg=' + encodeURIComponent(js2JSON([this.message.serialize()]));
    xreq.send(post);

    return this;
}

OpenSRF.XHRequest.prototype.core_handler = function() {
    sender = this.xreq.getResponseHeader(OSRF_HTTP_HEADER_FROM);
    thread = this.xreq.getResponseHeader(OSRF_HTTP_HEADER_THREAD);
    json = this.xreq.responseText;
    stat = this.xreq.status;

    var xhr = this;
    var req = OpenSRF.Stack.push(
        new OpenSRF.NetMessage(null, sender, thread, json),
        function(ses, req) {
            if(req) {
                if(req.response_queue.length > 0 && xhr.args.onresponse) 
                    return xhr.args.onresponse(req);
                if(req.complete && xhr.args.oncomplete)
                    return xhr.args.oncomplete(req);
            }
        }
    );
}

