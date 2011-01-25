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

import time
import osrf.log, osrf.ses, osrf.json


class Method(object):
    def __init__(self, **kwargs):
        self.name = kwargs['api_name']
        self.handler = kwargs['method']
        self.stream = kwargs.get('stream', False)
        self.argc = kwargs.get('argc', 0)
        self.atomic = kwargs.get('atomic', False)

    def get_func(self):
        ''' Returns the function handler reference '''
        return getattr(Application.application, self.handler)

    def get_doc(self):
        ''' Returns the function documentation '''
        return self.get_func().func_doc
        


class Application(object):
    ''' Base class for OpenSRF applications.  Provides static methods
        for loading and registering applications as well as common 
        application methods. '''

    # global application handle
    application = None
    name = None
    methods = {}

    def __init__(self):
        ''' Sets the application name and loads the application methods '''
        self.name = None

    def global_init(self):
        ''' Override this method to run code at application startup '''
        pass

    def child_init(self):
        ''' Override this method to run code at child startup.
            This is useful for initializing database connections or
            initializing other persistent resources '''
        pass

    def child_exit(self):
        ''' Override this method to run code at process exit time.
            This is useful for cleaning up resources like databaes 
            handles, etc. '''
        pass


    @staticmethod
    def load(name, module_name):
        ''' Loads the provided application module '''
        Application.name = name
        try:
            osrf.log.log_info("Loading application module %s" % module_name)
            exec('import %s' % module_name)
        except Exception, e:
            osrf.log.log_error("Error importing application module %s:%s" % (
                module_name, unicode(e)))

    @staticmethod
    def register_app(app):
        ''' Registers an application for use '''
        app.name = Application.name
        Application.application = app

    @staticmethod
    def register_method(**kwargs):
        Application.methods[kwargs['api_name']] = Method(**kwargs)
        if kwargs.get('stream'):
            kwargs['atomic'] = 1 
            kwargs['api_name'] +=  '.atomic'
            Application.methods[kwargs['api_name']] = Method(**kwargs)


    @staticmethod
    def handle_request(session, osrf_msg):
        ''' Find the handler, construct the server request, then run the method '''

        req_method = osrf_msg.payload()
        params = req_method.params() or []
        method_name = req_method.method()
        method = Application.methods.get(method_name)

        if method is None:
            session.send_method_not_found(osrf_msg.threadTrace(), method_name)
            return
            
        handler = method.get_func()

        param_json = osrf.json.to_json(params)
        param_json = param_json[1:len(param_json)-1]

        osrf.log.log_info("CALL: %s %s %s" % (session.service, method.name, param_json))
        server_req = osrf.ses.ServerRequest(session, osrf_msg.threadTrace(), method, params)

        result = None
        try:
            result = handler(server_req, *params)
        except Exception, e:
            osrf.log.log_error("Error running method %s %s %s" % (method.name, param_json, unicode(e)))
            session.send_status(
                osrf_msg.threadTrace(),
                osrf.net_obj.NetworkObject.osrfMethodException({   
                    'status' : unicode(e),
                    'statusCode': osrf.const.OSRF_STATUS_INTERNALSERVERERROR
                })
            )
            return

        server_req.respond_complete(result)

    @staticmethod
    def register_sysmethods():
        ''' Registers the global system methods '''

        Application.register_method(
            api_name = 'opensrf.system.time',
            method = 'sysmethod_time',
            argc = 0,
        )
        
        Application.register_method(
            api_name = 'opensrf.system.introspect',
            method = 'sysmethod_introspect',
            argc = 0,
            stream = True
        )

        Application.register_method(
            api_name = 'opensrf.system.echo',
            method = 'sysmethod_echo',
            argc = 1,
            stream = True
        )

    def sysmethod_time(self, request):
        '''@return type:number The current epoch time '''
        return time.time()

    def sysmethod_echo(self, request, *args):
        '''@return type:string The current epoch time '''
        for a in args:
            request.respond(a)

    def sysmethod_introspect(self, request, prefix=None):
        ''' Generates a list of methods with method metadata 
            @param type:string The limiting method name prefix.  If defined,
            only methods matching the given prefix will be returned.
            @return type:array List of method information '''

        for name, method in self.methods.iteritems():
            if prefix is not None and prefix != name[:len(prefix)]:
                continue

            request.respond({
                'api_name' : name,
                'method' : method.handler,
                'service' : self.name,
                'argc' : method.argc,
                'params' : [], # XXX parse me
                'desc' : method.get_doc() # XXX parse me
            })


