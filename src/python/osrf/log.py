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

import traceback, sys, os, re, threading, time
from osrf.const import OSRF_LOG_DEBUG, OSRF_LOG_ERR, OSRF_LOG_INFO, \
    OSRF_LOG_INTERNAL, OSRF_LOG_TYPE_FILE, OSRF_LOG_TYPE_STDERR, \
    OSRF_LOG_TYPE_SYSLOG, OSRF_LOG_WARN
LOG_SEMAPHORE = threading.BoundedSemaphore(value=1)


LOG_LEVEL = OSRF_LOG_DEBUG
LOG_TYPE = OSRF_LOG_TYPE_STDERR
LOG_FILE = None
FRGX = re.compile('/.*/')

_xid = '' # the current XID
_xid_pfx = '' # our XID prefix
_xid_ctr = 0
_xid_is_client = False # true if we are the request origin


def initialize(level, facility=None, logfile=None, is_client=False):
    """Initialize the logging subsystem."""
    global LOG_LEVEL, LOG_TYPE, LOG_FILE, _xid_is_client

    _xid_is_client = is_client
    LOG_LEVEL = level

    if facility: 
        try:
            import syslog
        except ImportError:
            sys.stderr.write("syslog not found, logging to stderr\n")
            return

        LOG_TYPE = OSRF_LOG_TYPE_SYSLOG
        initialize_syslog(facility, level)
        return
        
    if logfile:
        LOG_TYPE = OSRF_LOG_TYPE_FILE
        LOG_FILE = logfile

def make_xid():
    global _xid, _xid_pfx, _xid_is_client, _xid_ctr
    if _xid_is_client:
        if not _xid_pfx:
            _xid_pfx = "%s%s" % (time.time(), os.getpid())
        _xid = "%s%d" % (_xid_pfx, _xid_ctr)
        _xid_ctr += 1
         
def clear_xid():
    global _xid
    _xid =  ''

def set_xid(xid):
    global _xid
    _xid = xid

def get_xid():
    return _xid

# -----------------------------------------------------------------------
# Define wrapper functions for the log levels
# -----------------------------------------------------------------------
def log_internal(debug_str):
    __log(OSRF_LOG_INTERNAL, debug_str)
def log_debug(debug_str):
    __log(OSRF_LOG_DEBUG, debug_str)
def log_info(debug_str):
    __log(OSRF_LOG_INFO, debug_str)
def log_warn(debug_str):
    __log(OSRF_LOG_WARN, debug_str)
def log_error(debug_str):
    __log(OSRF_LOG_ERR, debug_str)

def __log(level, msg):
    """Builds the log message and passes the message off to the logger."""
    global LOG_LEVEL, LOG_TYPE

    try:
        import syslog
    except:
        if level == OSRF_LOG_ERR:
            sys.stderr.write('ERR ' + msg)
        return
        
    if int(level) > int(LOG_LEVEL): return

    # find the caller info for logging the file and line number
    tb = traceback.extract_stack(limit=3)
    tb = tb[0]
    lvl = 'DEBG'

    if level == OSRF_LOG_INTERNAL:
        lvl = 'INT '
    if level == OSRF_LOG_INFO:
        lvl = 'INFO'
    if level == OSRF_LOG_WARN:
        lvl = 'WARN'
    if level == OSRF_LOG_ERR:
        lvl = 'ERR '

    filename = FRGX.sub('', tb[0])
    msg = '[%s:%d:%s:%s:%s:%s] %s' % (lvl, os.getpid(), filename, tb[1], threading.currentThread().getName(), _xid, msg)

    if LOG_TYPE == OSRF_LOG_TYPE_SYSLOG:
        __log_syslog(level, msg)
    else:
        if LOG_TYPE == OSRF_LOG_TYPE_FILE:
            __log_file(msg)
        else:
            sys.stderr.write("%s\n" % msg)

    if level == OSRF_LOG_ERR and LOG_TYPE != OSRF_LOG_TYPE_STDERR:
        sys.stderr.write(msg + '\n')

def __log_syslog(level, msg):
    ''' Logs the message to syslog '''
    import syslog

    slvl = syslog.LOG_DEBUG
    if level == OSRF_LOG_INTERNAL:
        slvl = syslog.LOG_DEBUG
    if level == OSRF_LOG_INFO:
        slvl = syslog.LOG_INFO
    if level == OSRF_LOG_WARN:
        slvl = syslog.LOG_WARNING
    if level == OSRF_LOG_ERR:
        slvl = syslog.LOG_ERR

    syslog.syslog(slvl, msg)

def __log_file(msg):
    ''' Logs the message to a file. '''

    global LOG_TYPE

    epoch = time.time()
    timestamp = time.strftime('%Y-%M-%d %H:%m:%S', time.localtime(epoch))
    timestamp += '.%s' % str('%0.3f' % epoch)[-3:] # grab the millis

    logfile = None
    try:
        logfile = open(LOG_FILE, 'a')
    except:
        sys.stderr.write("cannot open log file for writing: %s\n" % LOG_FILE)
        LOG_TYPE = OSRF_LOG_TYPE_STDERR
        return
    try:
        LOG_SEMAPHORE.acquire()
        logfile.write("%s %s\n" % (timestamp, msg))
    finally:
        LOG_SEMAPHORE.release()
        
    logfile.close()

def initialize_syslog(facility, level):
    """Connect to syslog and set the logmask based on the level provided."""

    import syslog
    level = int(level)

    if facility == 'local0':
        facility = syslog.LOG_LOCAL0
    if facility == 'local1':
        facility = syslog.LOG_LOCAL1
    if facility == 'local2':
        facility = syslog.LOG_LOCAL2
    if facility == 'local3':
        facility = syslog.LOG_LOCAL3
    if facility == 'local4':
        facility = syslog.LOG_LOCAL4
    if facility == 'local5':
        facility = syslog.LOG_LOCAL5
    if facility == 'local6':
        facility = syslog.LOG_LOCAL6
    # add other facility maps if necessary...

    syslog.openlog(sys.argv[0], 0, facility)

