# -----------------------------------------------------------------------
# Copyright (C) 2007  Georgia Public Library Service
# Bill Erickson <billserickson@gmail.com>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# -----------------------------------------------------------------------

import osrf.json
import osrf.log
import osrf.ex
import osrf.ses
from osrf.const import OSRF_APP_SESSION_CONNECTED, \
    OSRF_APP_SESSION_DISCONNECTED, OSRF_MESSAGE_TYPE_RESULT, \
    OSRF_MESSAGE_TYPE_STATUS, OSRF_STATUS_COMPLETE, OSRF_STATUS_CONTINUE, \
    OSRF_STATUS_NOTFOUND, OSRF_STATUS_OK, OSRF_STATUS_TIMEOUT
import time


def push(net_msg):
    ses = osrf.ses.Session.find_session(net_msg.thread)

    if not ses:
        # This is an incoming request from a client, create a new server session
        osrf.log.log_error("server-side sessions don't exist yet")

    ses.set_remote_id(net_msg.sender)

    omessages = osrf.json.to_object(net_msg.body)

    osrf.log.log_internal("push(): received %d messages" \
        % len(omessages))

    # Pass each bundled opensrf message to the message handler
    start = time.time()
    for msg in omessages:
        handle_message(ses, msg)
    duration = time.time() - start

    if isinstance(ses, osrf.ses.ServerSession):
        osrf.log.log_info("Message processing duration %f" % duration)

def handle_message(session, message):

    osrf.log.log_internal("handle_message(): processing message of "
        "type %s" % message.type())

    if isinstance(session, osrf.ses.ClientSession):

        if message.type() == OSRF_MESSAGE_TYPE_RESULT:
            session.push_response_queue(message)
            return

        if message.type() == OSRF_MESSAGE_TYPE_STATUS:

            status_code = int(message.payload().statusCode())
            status_text = message.payload().status()
            osrf.log.log_internal("handle_message(): processing STATUS, "
                "status_code =  %d" % status_code)

        if status_code == OSRF_STATUS_COMPLETE:
            # The server has informed us that this request is complete
            req = session.find_request(message.threadTrace())
            if req: 
                osrf.log.log_internal("marking request as complete: %d" % req.rid)
                req.set_complete()
            return

        if status_code == OSRF_STATUS_OK:
            # We have connected successfully
            osrf.log.log_debug("Successfully connected to " + session.service)
            session.state = OSRF_APP_SESSION_CONNECTED
            return

        if status_code == OSRF_STATUS_CONTINUE:
            # server is telling us to reset our wait timeout and keep waiting for a response
            session.reset_request_timeout(message.threadTrace())
            return

        if status_code == OSRF_STATUS_TIMEOUT:
            osrf.log.log_debug("The server did not receive a request from us in time...")
            session.state = OSRF_APP_SESSION_DISCONNECTED
            return

        if status_code == OSRF_STATUS_NOTFOUND:
            osrf.log.log_error("Requested method was not found on the server: %s" % status_text)
            session.state = OSRF_APP_SESSION_DISCONNECTED
            raise osrf.ex.OSRFServiceException(status_text)

        raise osrf.ex.OSRFProtocolException("Unknown message status: %d" % status_code)




