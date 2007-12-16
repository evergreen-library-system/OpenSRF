#!/usr/bin/python
# vim:et:ts=4
"""
srfsh.py - provides a basic shell for issuing OpenSRF requests

  help
    - show this menu

  math_bench <count>
    - runs <count> opensrf.math requests and reports the average time

  request <service> <method> [<param1>, <param2>, ...]
    - performs an opensrf request

  set VAR=<value>
    - sets an environment variable

  Environment variables:
    SRFSH_OUTPUT = pretty - print pretty JSON and key/value pairs for network objects
                 = raw - print formatted JSON 

    SRFSH_LOCALE = <locale> - request responses to be returned in locale <locale> if available
"""

import os, sys, time, readline, atexit, re
import osrf.json
import osrf.system
import osrf.ses
import osrf.conf
import osrf.log

# -------------------------------------------------------------------
# main listen loop
# -------------------------------------------------------------------
def do_loop():
    while True:

        try:
            #line = raw_input("srfsh% ")
            line = raw_input("\033[01;32msrfsh\033[01;34m% \033[00m")
            if not len(line): 
                continue
            if str.lower(line) == 'exit' or str.lower(line) == 'quit': 
                break
            parts = str.split(line)

            command = parts[0]
        
            if command == 'request':
                parts.pop(0)
                handle_request(parts)
                continue

            if command == 'math_bench':
                parts.pop(0)
                handle_math_bench(parts)
                continue

            if command == 'help':
                handle_help()
                continue

            if command == 'set':
                parts.pop(0)
                handle_set(parts)

            if command == 'get':
                parts.pop(0)
                handle_get(parts)



        except KeyboardInterrupt:
            print ""

        except EOFError:
            print "exiting..."
            sys.exit(0)


# -------------------------------------------------------------------
# Set env variables to control behavior
# -------------------------------------------------------------------
def handle_set(parts):
    cmd = "".join(parts)
    pattern = re.compile('(.*)=(.*)').match(cmd)
    key = pattern.group(1)
    val = pattern.group(2)
    set_var(key, val)
    print "%s = %s" % (key, val)

def handle_get(parts):
    try:
        print get_var(parts[0])
    except:
        print ""


# -------------------------------------------------------------------
# Prints help info
# -------------------------------------------------------------------
def handle_help():
    print __doc__

# -------------------------------------------------------------------
# performs an opensrf request
# -------------------------------------------------------------------
def handle_request(parts):
    service = parts.pop(0)
    method = parts.pop(0)
    locale = __get_locale()
    jstr = '[%s]' % "".join(parts)
    params = None

    try:
        params = osrf.json.to_object(jstr)
    except:
        print "Error parsing JSON: %s" % jstr
        return

    ses = osrf.ses.ClientSession(service, locale=locale)

    start = time.time()

    req = ses.request2(method, tuple(params))


    while True:
        resp = req.recv(timeout=120)
        osrf.log.log_internal("Looping through receive request")
        if not resp:
            break
        total = time.time() - start

        otp = get_var('SRFSH_OUTPUT')
        if otp == 'pretty':
            print "\n" + osrf.json.debug_net_object(resp.content())
        else:
            print osrf.json.pprint(osrf.json.to_json(resp.content()))

    req.cleanup()
    ses.cleanup()

    print '-'*60
    print "Total request time: %f" % total
    print '-'*60


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
                # find all words that start with this prefix
                self.matching_words = [
                    w for w in self.words if w.startswith(prefix)
                ]
                self.prefix = prefix
                try:
                    return self.matching_words[index]
                except IndexError:
                    return None
    
    words = 'request', 'help', 'exit', 'quit', 'opensrf.settings', 'opensrf.math', 'set'
    completer = SrfshCompleter(words)
    readline.parse_and_bind("tab: complete")
    readline.set_completer(completer.complete)

    histfile = os.path.join(get_var('HOME'), ".srfsh_history")
    try:
        readline.read_history_file(histfile)
    except IOError:
        pass
    atexit.register(readline.write_history_file, histfile)

def do_connect():
    file = os.path.join(get_var('HOME'), ".srfsh.xml")
    print_green("Connecting to opensrf...")
    osrf.system.connect(file, 'srfsh')
    print_red('OK\n')

def load_plugins():
    # Load the user defined external plugins
    # XXX Make this a real module interface, with tab-complete words, commands, etc.
    try:
        plugins = osrf.conf.get('plugins')

    except:
        # XXX standard srfsh.xml does not yet define <plugins> element
        print_red("No plugins defined in /srfsh/plugins/plugin\n")
        return

    plugins = osrf.conf.get('plugins.plugin')
    if not isinstance(plugins, list):
        plugins = [plugins]

    for module in plugins:
        name = module['module']
        init = module['init']
        print_green("Loading module %s..." % name)

        try:
            string = 'from %s import %s\n%s()' % (name, init, init)
            exec(string)
            print_red('OK\n')

        except Exception, e:
            sys.stderr.write("\nError importing plugin %s, with init symbol %s: \n%s\n" % (name, init, e))

def set_vars():
    if not get_var('SRFSH_OUTPUT'):
        set_var('SRFSH_OUTPUT', 'pretty')

    # XXX Do we need to differ between LANG and LC_MESSAGES?
    if not get_var('SRFSH_LOCALE'):
        set_var('SRFSH_LOCALE', get_var('LC_ALL'))

def set_var(key, val):
    os.environ[key] = val

def get_var(key):
    try:
        return os.environ[key]
    except:
        return ''
    
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
    pattern = re.compile(r'^\s*([a-z]+)[^a-zA-Z]([A-Z]+)').search(env_locale)
    lang = pattern.group(1)
    region = pattern.group(2)
    locale = "%s-%s" % (lang, region)
    return locale
    
def print_green(string):
    sys.stdout.write("\033[01;32m")
    sys.stdout.write(string)
    sys.stdout.write("\033[00m")
    sys.stdout.flush()

def print_red(string):
    sys.stdout.write("\033[01;31m")
    sys.stdout.write(string)
    sys.stdout.write("\033[00m")
    sys.stdout.flush()


if __name__ == '__main__':

    # Kick it off
    set_vars()
    setup_readline()
    do_connect()
    load_plugins()
    do_loop()

