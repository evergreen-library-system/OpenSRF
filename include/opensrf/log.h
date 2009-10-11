#ifndef OSRF_LOG_INCLUDED
#define OSRF_LOG_INCLUDED

/**
	@file log.h
	@brief Header for logging routines.

	The OSRF logging routines support five different levels of log messages, and route them
	to any of three kinds of destinations.  Utility routines are available to control various
	details.

	The message levels are as follows:

	- OSRF_LOG_ERROR for error messages
	- OSRF_LOG_WARNING for warning messages
	- OSRF_LOG_INFO for informational messages
	- OSRF_LOG_DEBUG for debug messages
	- OSRF_LOG_INTERNAL for internal messages

	The application can set a message level so as to suppress some of the messages.
	Each level includes the ones above it.  For example, if the message level is set to
	OSRF_LOG_INFO, then the logging routines will issue error messages, warning messages, and
	informational messages, but suppress debug messages and internal messages.

	There are also activity messages, which are similar to informational messages, but can be
	enabled or disabled separately.

	The messages may be written to standard error (the default), to a designated log file, or
	to the Syslog (see man syslog).  In the latter case, the application can specify a facility
	number in order to control what Syslog does with the messages.  The facility number for
	activity numbers is controlled separately from that of the other message types.

	The messages are formatted like the following example:

	srfsh 2009-10-09 15:58:23 [DEBG:10530:socket_bundle.c:394:] removing socket 5

	The message includes:

	-# An application name
	-# A time stamp
	-# A tag indicating the message level
	-# The file name and line number from whence the message was issued
	-# An optional transaction id (not present in this example)
	-# The message text, formatted using printf-style conventions
*/

#include <opensrf/utils.h>

#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/* log levels */
#define OSRF_LOG_ERROR 1        /**< Level for error messages */
#define OSRF_LOG_WARNING 2      /**< Level for warning messages */
#define OSRF_LOG_INFO 3         /**< Level for informational messages */
#define OSRF_LOG_DEBUG 4        /**< Level for debug messages */
#define OSRF_LOG_INTERNAL 5     /**< Level for internal messages */
#define OSRF_LOG_ACTIVITY -1    /**< Pseudo message level for activity messages */

#define OSRF_LOG_TYPE_FILE 1    /**< Direct messages to a log file */
#define OSRF_LOG_TYPE_SYSLOG 2  /**< Direct messages to Syslog */
#define OSRF_LOG_TYPE_STDERR 3  /**< Direct messages to standard error */

/**
	Convenience macro for passing source file name and line number
	to the logging routines
*/
#define OSRF_LOG_MARK __FILE__, __LINE__

void osrfLogInit( int type, const char* appname, int maxlevel );

void osrfLogSetSyslogFacility( int facility );

void osrfLogSetSyslogActFacility( int facility );

void osrfLogToStderr( void );

void osrfRestoreLogType( void );

void osrfLogSetFile( const char* logfile );

void osrfLogSetAppname( const char* appname );

void osrfLogSetLevel( int loglevel );

int osrfLogGetLevel( void );

void osrfLogError( const char* file, int line, const char* msg, ... );

void osrfLogWarning( const char* file, int line, const char* msg, ... );

void osrfLogInfo( const char* file, int line, const char* msg, ... );

void osrfLogDebug( const char* file, int line, const char* msg, ... );

void osrfLogInternal( const char* file, int line, const char* msg, ... );

void osrfLogActivity( const char* file, int line, const char* msg, ... );

void osrfLogCleanup( void );

void osrfLogClearXid( void );

void osrfLogSetXid(char* xid);

void osrfLogForceXid(char* xid);

void osrfLogMkXid( void );

void osrfLogSetIsClient(int is);

const char* osrfLogGetXid( void );

void osrfLogSetActivityEnabled( int enabled );

int osrfLogFacilityToInt( const char* facility );

#ifdef __cplusplus
}
#endif

#endif
