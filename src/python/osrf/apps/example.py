# -----------------------------------------------------------------------
# Copyright (C) 2008  Equinox Software, Inc.
# Bill Erickson <erickson@esilibrary.com>
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
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
# 02110-1301, USA
# -----------------------------------------------------------------------
import os
import osrf.log
from osrf.app import Application

class Example(Application):
    ''' Example OpenSRF application. '''

    # ---------------------------------------------------------
    # Register a new method for this application
    # ---------------------------------------------------------
    Application.register_method(
        api_name = 'opensrf.py-example.reverse', # published API name for the method
        method = 'reverse', # name of def that implements this method
        argc = 1, # expects a single argument
        stream = True # returns a stream of results.  can be called atomic-ly
    )

    # ---------------------------------------------------------
    # This method implements the API call registered above
    # ---------------------------------------------------------
    def reverse(self, request, message=''):
        ''' Returns the given string in reverse order one character at a time
            @param type:string Message to reverse 
            @return type:string The reversed message, one character at a time.  '''
        idx = len(message) - 1
        while idx >= 0:
            request.respond(message[idx])
            idx -= 1

    # ---------------------------------------------------------
    # Session data test
    # ---------------------------------------------------------

    Application.register_method(
        api_name = 'opensrf.stateful_session_test',
        method = 'session_test',
        argc = 0
    )

    def session_test(self, request):
        c = request.session.session_data.get('counter', 0) + 1
        request.session.session_data['counter'] = c
        return c

    # ---------------------------------------------------------
    # Session callbacks test
    # ---------------------------------------------------------
    Application.register_method(
        api_name = 'opensrf.session_callback_test',
        method = 'callback_test',
        argc = 0
    )

    def callback_test(self, request):
        
        def pre_req_cb(ses):
            osrf.log.log_info("running pre_request callback")

        def post_req_cb(ses):
            osrf.log.log_info("running post_request callback")

        def disconnect_cb(ses):
            osrf.log.log_info("running disconnect callback")

        def death_cb(ses):
            osrf.log.log_info("running death callback")

        ses = request.session

        ses.register_callback('pre_request', pre_req_cb)
        ses.register_callback('post_request', post_req_cb)
        ses.register_callback('disconnect', disconnect_cb)
        ses.register_callback('death', death_cb)

        c = ses.session_data.get('counter', 0) + 1
        ses.session_data['counter'] = c
        return c


    # ---------------------------------------------------------
    # These example methods override methods from 
    # osrf.app.Application.  They are not required.
    # ---------------------------------------------------------
    def global_init(self):
        osrf.log.log_debug("Running global init handler for %s" % __name__)

    def child_init(self):
        osrf.log.log_debug("Running child init handler for process %d" % os.getpid())

    def child_exit(self):
        osrf.log.log_debug("Running child exit handler for process %d" % os.getpid())


# ---------------------------------------------------------
# Now register an instance of this class as an application
# ---------------------------------------------------------
Application.register_app(Example())


