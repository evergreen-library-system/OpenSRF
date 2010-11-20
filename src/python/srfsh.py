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

  router <query>
    - Queries the router.  Query options: services service-stats service-nodes

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

import os, sys, time, readline, atexit, re, pydoc
import osrf.json, osrf.system, osrf.ses, osrf.conf, osrf.log, osrf.net

router_command_map = {
    'services' : 'opensrf.router.info.class.list',
    'service-stats' : 'opensrf.router.info.stats.class.node.all',
    'service-nodes' : 'opensrf.router.info.stats.class.all'
}

# List of words to use for readline tab completion
tab_complete_words = [
    'request', 
    'set',
    'router',
    'help', 
    'exit', 
    'quit', 
    'opensrf.settings', 
    'opensrf.math'
]

# add the router commands to the tab-complete list
for rcommand in router_command_map.keys():
    tab_complete_words.append(rcommand)

# -------------------------------------------------------------------
# main listen loop
# -------------------------------------------------------------------
def do_loop():

    command_map = {
        'request' : handle_request,
        'router' : handle_router,
        'math_bench' : handle_math_bench,
        'help' : handle_help,
        'set' : handle_set,
        'get' : handle_get,
    }

    while True:

        try:
            report("", True)
            line = raw_input("srfsh# ")

            if not len(line): 
                continue

            if str.lower(line) == 'exit' or str.lower(line) == 'quit': 
                break

            parts = str.split(line)
            command = parts.pop(0)

            if command not in command_map:
                report("unknown command: '%s'\n" % command)
                continue

            command_map[command](parts)

        except EOFError: # ^-d
            sys.exit(0)

        except KeyboardInterrupt: # ^-c
            report("\n")

        except Exception, e:
            report("%s\n" % e)

    cleanup()

def handle_router(parts):

    if len(parts) == 0:
        report("usage: router <query>\n")
        return

    query = parts[0]

    if query not in router_command_map:
        report("router query options: %s\n" % ','.join(router_command_map.keys()))
        return

    return handle_request(['router', router_command_map[query]])

# -------------------------------------------------------------------
# Set env variables to control behavior
# -------------------------------------------------------------------
def handle_set(parts):
    cmd = "".join(parts)
    pattern = re.compile('(.*)=(.*)').match(cmd)
    key = pattern.group(1)
    val = pattern.group(2)
    set_var(key, val)
    report("%s = %s\n" % (key, val))

def handle_get(parts):
    try:
        report("%s=%s\n" % (parts[0], get_var(parts[0])))
    except:
        report("\n")


# -------------------------------------------------------------------
# Prints help info
# -------------------------------------------------------------------
def handle_help(foo):
    report(__doc__)

# -------------------------------------------------------------------
# performs an opensrf request
# -------------------------------------------------------------------
def handle_request(parts):

    if len(parts) < 2:
        report("usage: request <service> <api_name> [<param1>, <param2>, ...]\n")
        return

    service = parts.pop(0)
    method = parts.pop(0)
    locale = __get_locale()
    jstr = '[%s]' % "".join(parts)
    params = None

    try:
        params = osrf.json.to_object(jstr)
    except:
        report("Error parsing JSON: %s\n" % jstr)
        return

    ses = osrf.ses.ClientSession(service, locale=locale)

    start = time.time()

    req = ses.request2(method, tuple(params))


    while True:
        resp = None

        try:
            resp = req.recv(timeout=120)
        except osrf.net.XMPPNoRecipient:
            report("Unable to communicate with %s\n" % service)
            total = 0
            break

        if not resp: break

        total = time.time() - start
        content = resp.content()

        if content is not None:
            if get_var('SRFSH_OUTPUT_NET_OBJ_KEYS') == 'true':
                report("Received Data: %s\n" % osrf.json.debug_net_object(content))
            else:
                if get_var('SRFSH_OUTPUT_FORMAT_JSON') == 'true':
                    report("Received Data: %s\n" % osrf.json.pprint(osrf.json.to_json(content)))
                else:
                    report("Received Data: %s\n" % osrf.json.to_json(content))

    req.cleanup()
    ses.cleanup()

    report('-'*60 + "\n")
    report("Total request time: %f\n" % total)
    report('-'*60 + "\n")


def handle_math_bench(parts):

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




# -------------------------------------------------------------------
# Defines the tab-completion handling and sets up the readline history 
# -------------------------------------------------------------------
def setup_readline():

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

                sys.stdout.write('\n%s\nsrfsh# %s' % 
                    (' '.join(self.matching_words), readline.get_line_buffer()))

                return None

    completer = SrfshCompleter(tuple(tab_complete_words))
    readline.parse_and_bind("tab: complete")
    readline.set_completer(completer.complete)

    histfile = os.path.join(get_var('HOME'), ".srfsh_history")
    try:
        readline.read_history_file(histfile)
    except IOError:
        pass
    atexit.register(readline.write_history_file, histfile)

    readline.set_completer_delims(readline.get_completer_delims().replace('-',''))


def do_connect():
    file = os.path.join(get_var('HOME'), ".srfsh.xml")
    osrf.system.System.connect(config_file=file, config_context='srfsh')

def load_plugins():
    # Load the user defined external plugins
    # XXX Make this a real module interface, with tab-complete words, commands, etc.
    try:
        plugins = osrf.conf.get('plugins')

    except:
        # XXX standard srfsh.xml does not yet define <plugins> element
        #print("No plugins defined in /srfsh/plugins/plugin\n")
        return

    plugins = osrf.conf.get('plugins.plugin')
    if not isinstance(plugins, list):
        plugins = [plugins]

    for module in plugins:
        name = module['module']
        init = module['init']
        print "Loading module %s..." % name

        try:
            string = 'import %s\n%s.%s()' % (name, name, init)
            exec(string)
            print 'OK'

        except Exception, e:
            sys.stderr.write("\nError importing plugin %s, with init symbol %s: \n%s\n" % (name, init, e))

def cleanup():
    osrf.system.System.net_disconnect()
    
_output_buffer = '' # collect output for pager
def report(text, flush=False):
    global _output_buffer

    if get_var('SRFSH_OUTPUT_PAGED') == 'true':
        _output_buffer += text

        if flush and _output_buffer != '':
            pipe = os.popen('less -EX', 'w') 
            pipe.write(_output_buffer)
            pipe.close()
            _output_buffer = ''

    else:
        sys.stdout.write(text)
        if flush:
            sys.stdout.flush()

def set_vars():

    if not get_var('SRFSH_OUTPUT_NET_OBJ_KEYS'):
        set_var('SRFSH_OUTPUT_NET_OBJ_KEYS', 'false')

    if not get_var('SRFSH_OUTPUT_FORMAT_JSON'):
        set_var('SRFSH_OUTPUT_FORMAT_JSON', 'true')

    if not get_var('SRFSH_OUTPUT_PAGED'):
        set_var('SRFSH_OUTPUT_PAGED', 'true')

    # XXX Do we need to differ between LANG and LC_MESSAGES?
    if not get_var('SRFSH_LOCALE'):
        set_var('SRFSH_LOCALE', get_var('LC_ALL'))

def set_var(key, val):
    os.environ[key] = val

def get_var(key):
    return os.environ.get(key, '')
    
def __get_locale():
    """
    Return the defined locale for this srfsh session.

    A locale in OpenSRF is currently defined as a [a-z]{2}-[A-Z]{2} pattern.
    This function munges the LC_ALL setting to conform to that pattern; for
    example, trimming en_CA.UTF-8 to en-CA.

    >>> import srfsh
    >>> srfsh.set_var('SRFSH_LOCALE', 'zz-ZZ')
    >>> print __get_locale()
    zz-ZZ
    >>> srfsh.set_var('SRFSH_LOCALE', 'en_CA.UTF-8')
    >>> print __get_locale()
    en-CA
    """

    env_locale = get_var('SRFSH_LOCALE')
    if env_locale:
        pattern = re.compile(r'^\s*([a-z]+)[^a-zA-Z]([A-Z]+)').search(env_locale)
        lang = pattern.group(1)
        region = pattern.group(2)
        locale = "%s-%s" % (lang, region)
    else:
        locale = 'en-US'

    return locale
    
if __name__ == '__main__':

    # Kick it off
    set_vars()
    setup_readline()
    do_connect()
    load_plugins()
    do_loop()

