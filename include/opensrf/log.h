#ifndef OSRF_LOG_INCLUDED
#define OSRF_LOG_INCLUDED

/**
	@file log.h
	@brief Header for logging routines.
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
