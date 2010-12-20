#!/usr/bin/python
# vim:et:ts=4
"""
srfsh.py - provides a basic shell for issuing OpenSRF requests

  help
    - show this menu

  math_bench <count>
    - runs <count> opensrf.math requests and prints the average time

  request <service> <method> [<param1>, <param2>, ...]
    - performs an opensrf request
    - parameters are JSON strings

  router <query>
    - Queries the router.  Query options: services service-stats service-nodes

  introspect <service> [<api_name_prefix>]
    - List API calls for a service.  
    - api_name_prefix is a bare string or JSON string.

  set VAR=<value>
    - sets an environment variable

  get VAR
    - Returns the value for the environment variable

  Environment variables:
    SRFSH_OUTPUT_NET_OBJ_KEYS  = true - If a network object is array-encoded and key registry exists for the object type, annotate the object with field names
                               = false - Print JSON

    SRFSH_OUTPUT_FORMAT_JSON = true - Use JSON pretty printer
                             = false - Print raw JSON

    SRFSH_OUTPUT_PAGED = true - Paged output.  Uses "less -EX"
                       = false - Output is not paged

    SRFSH_LOCALE = <locale> - request responses to be returned in locale <locale> if available

"""
import os, sys, time, readline, atexit, re, pydoc, traceback
import osrf.json, osrf.system, osrf.ses, osrf.conf, osrf.log, osrf.net

class Srfsh(object):


    def __init__(self, script_file=None):

        # used for paging
        self.output_buffer = '' 

        # true if invoked with a script file 
        self.reading_script = False

        # multi-request sessions
        self.active_session = None 

        # default opensrf request timeout
        self.timeout = 120

        # map of command name to handler
        self.command_map = {}

        if script_file:
            self.open_script(script_file)
            self.reading_script = True

        # map of router sub-commands to router API calls
        self.router_command_map = {
            'services'      : 'opensrf.router.info.class.list',
            'service-stats' : 'opensrf.router.info.stats.class.node.all',
            'service-nodes' : 'opensrf.router.info.stats.class.all'
        }

        # seed the tab completion word bank
        self.tab_complete_words = self.router_command_map.keys() + [
            'exit', 
            'quit', 
            'opensrf.settings', 
            'opensrf.math',
            'opensrf.dbmath',
            'opensrf.py-example'
        ]

        # add the default commands
        for command in ['request', 'router', 'help', 'set', 
                'get', 'math_bench', 'introspect', 'connect', 'disconnect' ]:

            self.add_command(command = command, handler = getattr(Srfsh, 'handle_' + command))

        # for compat w/ srfsh.c
        self.add_command(command = 'open', handler = Srfsh.handle_connect)
        self.add_command(command = 'close', handler = Srfsh.handle_disconnect)

    def open_script(self, script_file):
        ''' Opens the script file and redirects the contents to STDIN for reading. '''

        try:
            script = open(script_file, 'r')
            os.dup2(script.fileno(), sys.stdin.fileno())
            script.close()
        except Exception, e:
            self.report_error("Error opening script file '%s': %s" % (script_file, str(e)))
            raise e


    def main_loop(self):
        ''' Main listen loop. '''

        self.set_vars()
        self.do_connect()
        self.load_plugins()
        self.setup_readline()

        while True:

            try:
                self.report("", True)
                line = raw_input("srfsh# ")

                if not len(line): 
                    continue

                if re.search('^\s*#', line): # ignore lines starting with #
                    continue

                if str.lower(line) == 'exit' or str.lower(line) == 'quit': 
                    break

                parts = str.split(line)
                command = parts.pop(0)

                if command not in self.command_map:
                    self.report("unknown command: '%s'\n" % command)
                    continue

                self.command_map[command](self, parts)

            except EOFError: # ctrl-d
                break

            except KeyboardInterrupt: # ctrl-c
                self.report("\n")

            except Exception, e:
                self.report("%s\n" % traceback.format_exc())

        self.cleanup()

    def handle_connect(self, parts):
        ''' Opens a connected session to an opensrf service '''

        if len(parts) == 0:
            self.report("usage: connect <service>")
            return

        service = parts.pop(0)

        if self.active_session:
            if self.active_session['service'] == service:
                return # use the existing active session
            else:
                # currently, we only support one active session at a time
                self.handle_disconnect([self.active_session['service']])

        self.active_session = {
            'ses' : osrf.ses.ClientSession(service, locale = self.__get_locale()),
            'service' : service
        }

        self.active_session['ses'].connect()

    def handle_disconnect(self, parts):
        ''' Disconnects the currently active session. '''

        if len(parts) == 0:
            self.report("usage: disconnect <service>")
            return

        service = parts.pop(0)

        if self.active_session:
            if self.active_session['service'] == service:
                self.active_session['ses'].disconnect()
                self.active_session['ses'].cleanup()
                self.active_session = None
            else:
                self.report_error("There is no open connection for service '%s'" % service)

    def handle_introspect(self, parts):
        ''' Introspect an opensrf service. '''

        if len(parts) == 0:
            self.report("usage: introspect <service> [api_prefix]\n")
            return

        service = parts.pop(0)
        args = [service, 'opensrf.system.method']

        if len(parts) > 0:
            api_pfx = parts[0]
            if api_pfx[0] != '"': # json-encode if necessary
                api_pfx = '"%s"' % api_pfx
            args.append(api_pfx)
        else:
            args[1] += '.all'

        return handle_request(args)


    def handle_router(self, parts):
        ''' Send requests to the router. '''

        if len(parts) == 0:
            self.report("usage: router <query>\n")
            return

        query = parts[0]

        if query not in self.router_command_map:
            self.report("router query options: %s\n" % ','.join(self.router_command_map.keys()))
            return

        return handle_request(['router', self.router_command_map[query]])

    def handle_set(self, parts):
        ''' Set env variables to control srfsh behavior. '''

        cmd = "".join(parts)
        pattern = re.compile('(.*)=(.*)').match(cmd)
        key = pattern.group(1)
        val = pattern.group(2)
        self.set_var(key, val)
        self.report("%s = %s\n" % (key, val))

    def handle_get(self, parts):
        ''' Returns environment variable value '''
        try:
            self.report("%s=%s\n" % (parts[0], self.get_var(parts[0])))
        except:
            self.report("\n")


    def handle_help(self, foo):
        ''' Prints help info '''
        self.report(__doc__)

    def handle_request(self, parts):
        ''' Performs an OpenSRF request and reports the results. '''

        if len(parts) < 2:
            self.report("usage: request <service> <api_name> [<param1>, <param2>, ...]\n")
            return

        self.report("\n")

        service = parts.pop(0)
        method = parts.pop(0)
        locale = self.__get_locale()
        jstr = '[%s]' % "".join(parts)
        params = None

        try:
            params = osrf.json.to_object(jstr)
        except:
            self.report("Error parsing JSON: %s\n" % jstr)
            return

        using_active = False
        if self.active_session and self.active_session['service'] == service:
            # if we have an open connection to the same service, use it
            ses = self.active_session['ses']
            using_active = True
        else:
            ses = osrf.ses.ClientSession(service, locale=locale)

        start = time.time()

        req = ses.request2(method, tuple(params))

        last_content = None
        while True:
            resp = None

            try:
                resp = req.recv(timeout=self.timeout)
            except osrf.net.XMPPNoRecipient:
                self.report("Unable to communicate with %s\n" % service)
                total = 0
                break

            if not resp: break

            total = time.time() - start
            content = resp.content()

            if content is not None:
                last_content = content
                if self.get_var('SRFSH_OUTPUT_NET_OBJ_KEYS') == 'true':
                    self.report("Received Data: %s\n" % osrf.json.debug_net_object(content))
                else:
                    if self.get_var('SRFSH_OUTPUT_FORMAT_JSON') == 'true':
                        self.report("Received Data: %s\n" % osrf.json.pprint(osrf.json.to_json(content)))
                    else:
                        self.report("Received Data: %s\n" % osrf.json.to_json(content))

        req.cleanup()
        if not using_active:
            ses.cleanup()

        self.report("\n" + '-'*60 + "\n")
        self.report("Total request time: %f\n" % total)
        self.report('-'*60 + "\n")

        return last_content


    def handle_math_bench(self, parts):
        ''' Sends a series of request to the opensrf.math service and collects timing stats. '''

        count = int(parts.pop(0))
        ses = osrf.ses.ClientSession('opensrf.math')
        times = []

        for cnt in range(100):
            if cnt % 10:
                sys.stdout.write('.')
            else:
                sys.stdout.write( str( cnt / 10 ) )
        print ""

        for cnt in range(count):
        
            starttime = time.time()
            req = ses.request('add', 1, 2)
            resp = req.recv(timeout=2)
            endtime = time.time()
        
            if resp.content() == 3:
                sys.stdout.write("+")
                sys.stdout.flush()
                times.append( endtime - starttime )
            else:
                print "What happened? %s" % str(resp.content())
        
            req.cleanup()
            if not ( (cnt + 1) % 100):
                print ' [%d]' % (cnt + 1)
        
        ses.cleanup()
        total = 0
        for cnt in times:
            total += cnt 
        print "\naverage time %f" % (total / len(times))




    def setup_readline(self):
        ''' Initialize readline history and tab completion. '''

        class SrfshCompleter(object):

            def __init__(self, words):
                self.words = words
                self.prefix = None
        
            def complete(self, prefix, index):

                if prefix != self.prefix:

                    self.prefix = prefix

                    # find all words that start with this prefix
                    self.matching_words = [
                        w for w in self.words if w.startswith(prefix)
                    ]

                    if len(self.matching_words) == 0:
                        return None

                    if len(self.matching_words) == 1:
                        return self.matching_words[0]

                    # re-print the prompt w/ all of the possible word completions
                    sys.stdout.write('\n%s\nsrfsh# %s' % 
                        (' '.join(self.matching_words), readline.get_line_buffer()))

                    return None

        completer = SrfshCompleter(tuple(self.tab_complete_words))
        readline.parse_and_bind("tab: complete")
        readline.set_completer(completer.complete)

        histfile = os.path.join(self.get_var('HOME'), ".srfsh_history")
        try:
            readline.read_history_file(histfile)
        except IOError:
            pass
        atexit.register(readline.write_history_file, histfile)

        readline.set_completer_delims(readline.get_completer_delims().replace('-',''))


    def do_connect(self):
        ''' Connects this instance to the OpenSRF network. '''

        file = os.path.join(self.get_var('HOME'), ".srfsh.xml")
        osrf.system.System.connect(config_file=file, config_context='srfsh')

    def add_command(self, **kwargs):
        ''' Adds a new command to the supported srfsh commands.

        Command is also added to the tab-completion word bank.

        kwargs :
            command : the command name
            handler : reference to a two-argument function.  
                Arguments are Srfsh instance and command arguments.
        '''

        command = kwargs['command']
        self.command_map[command] = kwargs['handler']
        self.tab_complete_words.append(command)


    def load_plugins(self):
        ''' Load plugin modules from the srfsh configuration file '''

        try:
            plugins = osrf.conf.get('plugins.plugin')
        except:
            return

        if not isinstance(plugins, list):
            plugins = [plugins]

        for plugin in plugins:
            module = plugin['module']
            init = plugin.get('init', 'load')
            self.report("Loading module %s..." % module, True, True)

            try:
                mod = __import__(module, fromlist=' ')
                getattr(mod, init)(self, plugin)
                self.report("OK.\n", True, True)

            except Exception, e:
                self.report_error("Error importing plugin '%s' : %s\n" % (module, traceback.format_exc()))

    def cleanup(self):
        ''' Disconnects from opensrf. '''
        osrf.system.System.net_disconnect()

    def report_error(self, msg):
        ''' Log to stderr. '''
        sys.stderr.write("%s\n" % msg)
        sys.stderr.flush()
        
    def report(self, text, flush=False, no_page=False):
        ''' Logs to the pager or stdout, depending on env vars and context '''

        if self.reading_script or no_page or self.get_var('SRFSH_OUTPUT_PAGED') != 'true':
            sys.stdout.write(text)
            if flush:
                sys.stdout.flush()
        else:
            self.output_buffer += text

            if flush and self.output_buffer != '':
                pipe = os.popen('less -EX', 'w') 
                pipe.write(self.output_buffer)
                pipe.close()
                self.output_buffer = ''

    def set_vars(self):
        ''' Set defaults for environment variables. '''

        if not self.get_var('SRFSH_OUTPUT_NET_OBJ_KEYS'):
            self.set_var('SRFSH_OUTPUT_NET_OBJ_KEYS', 'false')

        if not self.get_var('SRFSH_OUTPUT_FORMAT_JSON'):
            self.set_var('SRFSH_OUTPUT_FORMAT_JSON', 'true')

        if not self.get_var('SRFSH_OUTPUT_PAGED'):
            self.set_var('SRFSH_OUTPUT_PAGED', 'true')

        # XXX Do we need to differ between LANG and LC_MESSAGES?
        if not self.get_var('SRFSH_LOCALE'):
            self.set_var('SRFSH_LOCALE', self.get_var('LC_ALL'))

    def set_var(self, key, val):
        ''' Sets an environment variable's value. '''
        os.environ[key] = val

    def get_var(self, key):
        ''' Returns an environment variable's value. '''
        return os.environ.get(key, '')
        
    def __get_locale(self):
        """
        Return the defined locale for this srfsh session.

        A locale in OpenSRF is currently defined as a [a-z]{2}-[A-Z]{2} pattern.
        This function munges the LC_ALL setting to conform to that pattern; for
        example, trimming en_CA.UTF-8 to en-CA.

        >>> import srfsh
        >>> shell = srfsh.Srfsh()
        >>> shell.set_var('SRFSH_LOCALE', 'zz-ZZ')
        >>> print shell.__get_locale()
        zz-ZZ
        >>> shell.set_var('SRFSH_LOCALE', 'en_CA.UTF-8')
        >>> print shell.__get_locale()
        en-CA
        """

        env_locale = self.get_var('SRFSH_LOCALE')
        if env_locale:
            pattern = re.compile(r'^\s*([a-z]+)[^a-zA-Z]([A-Z]+)').search(env_locale)
            lang = pattern.group(1)
            region = pattern.group(2)
            locale = "%s-%s" % (lang, region)
        else:
            locale = 'en-US'

        return locale
    
if __name__ == '__main__':
    script = sys.argv[1] if len(sys.argv) > 1 else None
    Srfsh(script).main_loop()

