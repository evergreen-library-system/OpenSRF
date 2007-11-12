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
logSema = threading.BoundedSemaphore(value=1)


loglevel = OSRF_LOG_DEBUG
logtype = OSRF_LOG_TYPE_STDERR
logfile = None

def osrfInitLog(level, facility=None, file=None):
    """Initialize the logging subsystem."""
    global loglevel, logtype, logfile

    loglevel = level

    if facility: 
        try:
            import syslog
        except ImportError:
            sys.stderr.write("syslog not found, logging to stderr\n")
            return

        logtype = OSRF_LOG_TYPE_SYSLOG
        osrfInitSyslog(facility, level)
        return
        
    if file:
        logtype = OSRF_LOG_TYPE_FILE
        logfile = file


# -----------------------------------------------------------------------
# Define wrapper functions for the log levels
# -----------------------------------------------------------------------
def osrfLogInternal(s): __osrfLog(OSRF_LOG_INTERNAL,s)
def osrfLogDebug(s): __osrfLog(OSRF_LOG_DEBUG,s)
def osrfLogInfo(s): __osrfLog(OSRF_LOG_INFO,s)
def osrfLogWarn(s): __osrfLog(OSRF_LOG_WARN,s)
def osrfLogErr(s): __osrfLog(OSRF_LOG_ERR,s)


frgx = re.compile('/.*/')

def __osrfLog(level, msg):
    """Builds the log message and passes the message off to the logger."""
    global loglevel, logtype

    try:
        import syslog
    except:
        if level == OSRF_LOG_ERR:
            sys.stderr.write('ERR ' + msg)
        return
        
    if int(level) > int(loglevel): return

    # find the caller info for logging the file and line number
    tb = traceback.extract_stack(limit=3)
    tb = tb[0]
    lvl = 'DEBG'

    if level == OSRF_LOG_INTERNAL: lvl = 'INT '
    if level == OSRF_LOG_INFO: lvl = 'INFO'
    if level == OSRF_LOG_WARN: lvl = 'WARN'
    if level == OSRF_LOG_ERR:  lvl = 'ERR '

    file = frgx.sub('',tb[0])
    msg = '[%s:%d:%s:%s:%s] %s' % (lvl, os.getpid(), file, tb[1], threading.currentThread().getName(), msg)

    if logtype == OSRF_LOG_TYPE_SYSLOG:
        __logSyslog(level, msg)
    else:
        if logtype == OSRF_LOG_TYPE_FILE:
            __logFile(msg)
        else:
            sys.stderr.write("%s\n" % msg)

    if level == OSRF_LOG_ERR and logtype != OSRF_LOG_TYPE_STDERR:
        sys.stderr.write(msg + '\n')

def __logSyslog(level, msg):
    ''' Logs the message to syslog '''
    import syslog

    slvl = syslog.LOG_DEBUG
    if level == OSRF_LOG_INTERNAL: slvl=syslog.LOG_DEBUG
    if level == OSRF_LOG_INFO: slvl = syslog.LOG_INFO
    if level == OSRF_LOG_WARN: slvl = syslog.LOG_WARNING
    if level == OSRF_LOG_ERR:  slvl = syslog.LOG_ERR

    sys.stderr.write("logging at level: %s\n" % str(slvl))
    syslog.syslog(slvl, msg)

def __logFile(msg):
    ''' Logs the message to a file. '''

    global logfile, logtype

    f = None
    try:
        f = open(logfile, 'a')
    except:
        sys.stderr.write("cannot open log file for writing: %s\n", logfile)
        logtype = OSRF_LOG_TYPE_STDERR
        return
    try:
        logSema.acquire()
        f.write("%s\n" % msg)
    finally:
        logSema.release()
        
    f.close()
    


def osrfInitSyslog(facility, level):
    """Connect to syslog and set the logmask based on the level provided."""

    import syslog
    level = int(level)

    if facility == 'local0': facility = syslog.LOG_LOCAL0
    if facility == 'local1': facility = syslog.LOG_LOCAL1
    if facility == 'local2': facility = syslog.LOG_LOCAL2
    if facility == 'local3': facility = syslog.LOG_LOCAL3
    if facility == 'local4': facility = syslog.LOG_LOCAL4
    if facility == 'local5': facility = syslog.LOG_LOCAL5
    if facility == 'local6': facility = syslog.LOG_LOCAL6
    # add other facility maps if necessary...

    syslog.openlog(sys.argv[0], 0, facility)

