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

import traceback, sys, os, re, threading
from osrf.const import *
LOG_SEMAPHORE = threading.BoundedSemaphore(value=1)


LOG_LEVEL = OSRF_LOG_DEBUG
LOG_TYPE = OSRF_LOG_TYPE_STDERR
LOG_FILE = None
FRGX = re.compile('/.*/')


def initialize(level, facility=None, logfile=None):
    """Initialize the logging subsystem."""
    global LOG_LEVEL, LOG_TYPE, LOG_FILE

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


# -----------------------------------------------------------------------
# Define wrapper functions for the log levels
# -----------------------------------------------------------------------
def log_internal(s):
    __log(OSRF_LOG_INTERNAL, s)
def logDebug(s):
    __log(OSRF_LOG_DEBUG, s)
def log_info(s):
    __log(OSRF_LOG_INFO, s)
def log_warn(s):
    __log(OSRF_LOG_WARN, s)
def logError(s):
    __log(OSRF_LOG_ERR, s)

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
    msg = '[%s:%d:%s:%s:%s] %s' % (lvl, os.getpid(), filename, tb[1], threading.currentThread().getName(), msg)

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

    global LOG_FILE, LOG_TYPE

    logfile = None
    try:
        logfile = open(LOG_FILE, 'a')
    except:
        sys.stderr.write("cannot open log file for writing: %s\n", LOG_FILE)
        LOG_TYPE = OSRF_LOG_TYPE_STDERR
        return
    try:
        LOG_SEMAPHORE.acquire()
        logfile.write("%s\n" % msg)
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

