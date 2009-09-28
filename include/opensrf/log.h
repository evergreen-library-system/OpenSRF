#ifndef OSRF_LOG_INCLUDED
#define OSRF_LOG_INCLUDED

#include <opensrf/utils.h>

#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* log levels */
#define OSRF_LOG_ERROR 1
#define OSRF_LOG_WARNING 2
#define OSRF_LOG_INFO 3
#define OSRF_LOG_DEBUG 4
#define OSRF_LOG_INTERNAL 5
#define OSRF_LOG_ACTIVITY -1

#define OSRF_LOG_TYPE_FILE 1
#define OSRF_LOG_TYPE_SYSLOG 2
#define OSRF_LOG_TYPE_STDERR 3

#define OSRF_LOG_MARK __FILE__, __LINE__


/* Initializes the logger. */
void osrfLogInit( int type, const char* appname, int maxlevel );

/** Sets the systlog facility for the regular logs */
void osrfLogSetSyslogFacility( int facility );

/** Sets the systlog facility for the activity logs */
void osrfLogSetSyslogActFacility( int facility );

/** Reroutes logged messages to standard error; */
/** intended for development and debugging */
void osrfLogToStderr( void );

/** Undoes the effects of osrfLogToStderr() */
void osrfRestoreLogType( void );

/** Sets the log file to use if we're logging to a file */
void osrfLogSetFile( const char* logfile );

/* once we know which application we're running, call this method to
 * set the appname so log lines can include the app name */
void osrfLogSetAppname( const char* appname );

/** Set or Get the global log level.  Any log statements with a higher level
 * than "level" will not be logged */
void osrfLogSetLevel( int loglevel );
int osrfLogGetLevel( void );

/* Log an error message */
void osrfLogError( const char* file, int line, const char* msg, ... );

/* Log a warning message */
void osrfLogWarning( const char* file, int line, const char* msg, ... );

/* log an info message */
void osrfLogInfo( const char* file, int line, const char* msg, ... );

/* Log a debug message */
void osrfLogDebug( const char* file, int line, const char* msg, ... );

/* Log an internal debug message */
void osrfLogInternal( const char* file, int line, const char* msg, ... );

/* Log an activity message */
void osrfLogActivity( const char* file, int line, const char* msg, ... );

void osrfLogCleanup( void );

void osrfLogClearXid( void );
/**
 * Set the log XID.  This only sets the xid if we are not the origin client 
 */
void osrfLogSetXid(char* xid);
/**
 * Set the log XID regardless of whether we are the origin client
 */
void osrfLogForceXid(char* xid);
void osrfLogMkXid( void );
void osrfLogSetIsClient(int is);
char* osrfLogGetXid( void );

/* sets the activity flag */
void osrfLogSetActivityEnabled( int enabled );

/* returns the int representation of the log facility based on the facility name
 * if the facility name is invalid, LOG_LOCAL0 is returned 
 */
int osrfLogFacilityToInt( const char* facility );

#ifdef __cplusplus
}
#endif

#endif
