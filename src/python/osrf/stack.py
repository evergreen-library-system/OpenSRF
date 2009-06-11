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

import time
import osrf.json, osrf.log, osrf.ex, osrf.ses, osrf.const, osrf.app

def push(net_msg):

    ses = osrf.ses.Session.find_or_create(net_msg.thread)
    ses.set_remote_id(net_msg.sender)
    if not ses.service:
        ses.service = osrf.app.Application.name

    omessages = osrf.json.to_object(net_msg.body)

    osrf.log.log_internal("stack.push(): received %d messages" % len(omessages))

    # Pass each bundled opensrf message to the message handler
    start = time.time()
    for msg in omessages:
        handle_message(ses, msg)
    duration = time.time() - start

    if isinstance(ses, osrf.ses.ServerSession):
        osrf.log.log_info("Message processing duration %f" % duration)

    return ses

def handle_message(session, message):

    osrf.log.log_internal("handle_message(): processing message of "
        "type %s" % message.type())

    if isinstance(session, osrf.ses.ClientSession):
        handle_client(session, message)
    else:
        handle_server(session, message)


def handle_client(session, message):

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_RESULT:
        session.push_response_queue(message)
        return

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_STATUS:

        status_code = int(message.payload().statusCode())
        status_text = message.payload().status()
        osrf.log.log_internal("handle_message(): processing STATUS, "
            "status_code =  %d" % status_code)

        if status_code == osrf.const.OSRF_STATUS_COMPLETE:
            # The server has informed us that this request is complete
            req = session.find_request(message.threadTrace())
            if req: 
                osrf.log.log_internal("marking request as complete: %d" % req.rid)
                req.set_complete()
            return

        if status_code == osrf.const.OSRF_STATUS_OK:
            # We have connected successfully
            osrf.log.log_debug("Successfully connected to " + session.service)
            session.state = OSRF_APP_SESSION_CONNECTED
            return

        if status_code == osrf.const.OSRF_STATUS_CONTINUE:
            # server is telling us to reset our wait timeout and keep waiting for a response
            session.reset_request_timeout(message.threadTrace())
            return

        if status_code == osrf.const.OSRF_STATUS_TIMEOUT:
            osrf.log.log_debug("The server did not receive a request from us in time...")
            session.state = OSRF_APP_SESSION_DISCONNECTED
            return

        if status_code == osrf.const.OSRF_STATUS_NOTFOUND:
            osrf.log.log_error("Requested method was not found on the server: %s" % status_text)
            session.state = OSRF_APP_SESSION_DISCONNECTED
            raise osrf.ex.OSRFServiceException(status_text)

        if status_code == osrf.const.OSRF_STATUS_INTERNALSERVERERROR:
            raise osrf.ex.OSRFServiceException("Server error %d : %s" % (status_code, status_text))

        raise osrf.ex.OSRFProtocolException("Unknown message status: %d" % status_code)


def handle_server(session, message):

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_REQUEST:
        osrf.log.log_debug("server received REQUEST from %s" % session.remote_id)
        session.run_callback('pre_request')
        osrf.app.Application.handle_request(session, message)
        session.run_callback('post_request')
        return

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_CONNECT:
        osrf.log.log_debug("server received CONNECT from %s" % session.remote_id)
        session.state = osrf.const.OSRF_APP_SESSION_CONNECTED 
        session.send_connect_ok(message.threadTrace())
        return

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_DISCONNECT:
        osrf.log.log_debug("server received DISCONNECT from %s" % session.remote_id)
        session.state = osrf.const.OSRF_APP_SESSION_DISCONNECTED
        session.run_callback('disconnect')
        return

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_STATUS:
        osrf.log.log_debug("server ignoring STATUS from %s" % session.remote_id)
        return

    if message.type() == osrf.const.OSRF_MESSAGE_TYPE_RESULT:
        osrf.log.log_debug("server ignoring RESULT from %s" % session.remote_id)
        return


