#include <opensrf/log.h>

#define OSRF_NO_LOG_TYPE -1

static int _prevLogType             = OSRF_NO_LOG_TYPE;
static int _osrfLogType				= OSRF_LOG_TYPE_STDERR;
static int _osrfLogFacility			= LOG_LOCAL0;
static int _osrfLogActFacility		= LOG_LOCAL1;
static char* _osrfLogFile			= NULL;
static char* _osrfLogAppname		= NULL;
static int _osrfLogLevel			= OSRF_LOG_INFO;
static int _osrfLogActivityEnabled	= 1;
static int _osrfLogIsClient         = 0;

static char* _osrfLogXid            = NULL; /* current xid */
static char* _osrfLogXidPfx         = NULL; /* xid prefix string */

static void osrfLogSetType( int logtype );
static void _osrfLogDetail( int level, const char* filename, int line, char* msg );
static void _osrfLogToFile( const char* label, long pid, const char* filename, int line,
							const char* xid, const char* msg );
static void _osrfLogSetXid( const char* xid );

void osrfLogCleanup( void ) {
	free(_osrfLogAppname);
	_osrfLogAppname = NULL;
	free(_osrfLogFile);
	_osrfLogFile = NULL;
	_osrfLogType = OSRF_LOG_TYPE_STDERR;
}


/**
	@brief Record some options for later reference by the logging routines.
	@param type Type of logging; i.e. where the log messages go.
	@param appname Pointer to the application name (may be NULL).
	@param maxlevel Which levels of message to issue or suppress.

	Typically the values for these parameters come from a configuration file.

	There are three valid values for @a type:

	- OSRF_LOG_TYPE_FILE -- write messages to a log file
	- OSRF_LOG_TYPE_SYSLOG -- write messages to the syslog facility
	- OSRF_LOG_TYPE_STDERR  -- write messages to standard error

	If @a type has any other value, log messages will be written to standard error.

	The logging type may be set separately by calling osrfLogSetType().  See also
	osrfLogToStderr() and osrfRestoreLogType().

	The @a appname string prefaces every log message written to a log file or to standard
	error.  It also identifies the application to the Syslog facility, if the application
	uses Syslog.  The default application name, if not overridden by this function or by
	osrfLogSetAppname(), is "osrf".

	Here are the valid values for @a maxlevel, with the corresponding macros:

	- 1 OSRF_LOG_ERROR
	- 2 OSRF_LOG_WARNING
	- 3 OSRF_LOG_INFO (the default)
	- 4 OSRF_LOG_DEBUG
	- 5 OSRF_LOG_INTERNAL

	With the special exception of activity messages (see osrfLogActivity()), the logging
	routines will suppress any messages at a level greater than that specified by
	@a maxlevel.  Setting @a maxlevel to zero or less suppresses all levels of message.
	Setting it to 5 or more enables all levels of message.

	The message level may be set separately by calling osrfLogSetLevel().
*/
void osrfLogInit( int type, const char* appname, int maxlevel ) {
	osrfLogSetType(type);
	if(appname) osrfLogSetAppname(appname);
	osrfLogSetLevel(maxlevel);
	if( type == OSRF_LOG_TYPE_SYSLOG ) 
		openlog(_osrfLogAppname, 0, _osrfLogFacility );
}

static void _osrfLogSetXid( const char* xid ) {
   if(xid) {
      if(_osrfLogXid) free(_osrfLogXid);
      _osrfLogXid = strdup(xid);
   }
}

void osrfLogClearXid( void ) { _osrfLogSetXid(""); }
void osrfLogSetXid(char* xid) {
   if(!_osrfLogIsClient) _osrfLogSetXid(xid);
}
void osrfLogForceXid(char* xid) {
   _osrfLogSetXid(xid);
}

void osrfLogMkXid( void ) {
   if(_osrfLogIsClient) {
      static int _osrfLogXidInc = 0; /* increments with each new xid for uniqueness */
      char buf[32];
      snprintf(buf, sizeof(buf), "%s%d", _osrfLogXidPfx, _osrfLogXidInc);
      _osrfLogSetXid(buf);
      _osrfLogXidInc++;
   }
}

char* osrfLogGetXid( void ) {
   return _osrfLogXid;
}

void osrfLogSetIsClient(int is) {
   _osrfLogIsClient = is;
   if(!is) return;
   /* go ahead and create the xid prefix so it will be consistent later */
   static char buff[32];
   snprintf(buff, sizeof(buff), "%d%ld", (int)time(NULL), (long) getpid());
   _osrfLogXidPfx = buff;
}

/**
	@brief Specify what kind of logging to perform.
	@param logtype A code indicating the type of logging.

	There are three valid values for @a logtype:

	- OSRF_LOG_TYPE_FILE -- write messages to a log file
	- OSRF_LOG_TYPE_SYSLOG -- write messages to the syslog facility
	- OSRF_LOG_TYPE_STDERR  -- write messages to standard error

	If @a logtype has any other value, log messages will be written to standard error.

	This function merely records the log type for future reference.  It does not open
	or close any files.

	See also osrfLogInit(), osrfLogToStderr() and osrfRestoreLogType().

*/
static void osrfLogSetType( int logtype ) {

	switch( logtype )
	{
		case OSRF_LOG_TYPE_FILE :
		case OSRF_LOG_TYPE_SYSLOG :
		case OSRF_LOG_TYPE_STDERR :
			_osrfLogType = logtype;
			break;
		default :
			fprintf(stderr, "Unrecognized log type.  Logging to stderr\n");
			_osrfLogType = OSRF_LOG_TYPE_STDERR;
			break;
	}
}

void osrfLogToStderr( void )
{
	if( OSRF_NO_LOG_TYPE == _prevLogType ) {
		_prevLogType = _osrfLogType;
		_osrfLogType = OSRF_LOG_TYPE_STDERR;
	}
}

void osrfRestoreLogType( void )
{
	if( _prevLogType != OSRF_NO_LOG_TYPE ) {
		_osrfLogType = _prevLogType;
		_prevLogType = OSRF_NO_LOG_TYPE;
	}
}

void osrfLogSetFile( const char* logfile ) {
	if(!logfile) return;
	if(_osrfLogFile) free(_osrfLogFile);
	_osrfLogFile = strdup(logfile);
}

void osrfLogSetActivityEnabled( int enabled ) {
	_osrfLogActivityEnabled = enabled;
}

void osrfLogSetAppname( const char* appname ) {
	if(!appname) return;
	if(_osrfLogAppname) free(_osrfLogAppname);
	_osrfLogAppname = strdup(appname);

	/* if syslogging, re-open the log with the appname */
	if( _osrfLogType == OSRF_LOG_TYPE_SYSLOG) {
		closelog();
		openlog(_osrfLogAppname, 0, _osrfLogFacility);
	}
}

void osrfLogSetSyslogFacility( int facility ) {
	_osrfLogFacility = facility;
}
void osrfLogSetSyslogActFacility( int facility ) {
	_osrfLogActFacility = facility;
}

/** Sets the global log level.  Any log statements with a higher level
 * than "level" will not be logged */
void osrfLogSetLevel( int loglevel ) {
	_osrfLogLevel = loglevel;
}

/** Gets the current global log level. **/
int osrfLogGetLevel( void ) {
	return _osrfLogLevel;
}

void osrfLogError( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_ERROR ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_ERROR, file, line, VA_BUF );
}

void osrfLogWarning( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_WARNING ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_WARNING, file, line, VA_BUF );
}

void osrfLogInfo( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_INFO ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_INFO, file, line, VA_BUF );
}

void osrfLogDebug( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_DEBUG ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_DEBUG, file, line, VA_BUF );
}

void osrfLogInternal( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_INTERNAL ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_INTERNAL, file, line, VA_BUF );
}

void osrfLogActivity( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel >= OSRF_LOG_INFO
		|| ( _osrfLogActivityEnabled && _osrfLogLevel >= OSRF_LOG_ACTIVITY ) )
	{
		VA_LIST_TO_STRING( msg );

		if( _osrfLogActivityEnabled && _osrfLogLevel >= OSRF_LOG_ACTIVITY )
			_osrfLogDetail( OSRF_LOG_ACTIVITY, file, line, VA_BUF );

		/* also log at info level */
		if( _osrfLogLevel >= OSRF_LOG_INFO )
			_osrfLogDetail( OSRF_LOG_INFO, file, line, VA_BUF );
	}
}

/**
	@brief Issue a log message.
	@param level The message level.
	@param filename The name of the source file from whence the message is issued.
	@param line The line number from whence the message is issued.
	@param msg The text of the message.

	This function is the final common pathway for all messages.

	The @a level parameter determines the tag to be incorporated into the message: "ERR",
	"WARN", "INFO", "DEBG", "INT " or "ACT".

	The @a filename and @a name identify the location in the application code from whence the
	message is being issued.

	Here we format the message and route it to the appropriate output destination, depending
	on the current setting of _osrfLogType: Syslog, a log file, or standard error.
*/
static void _osrfLogDetail( int level, const char* filename, int line, char* msg ) {

	if(!filename) filename = "";

	char* label = "INFO";		/* level name */
	int lvl = LOG_INFO;	/* syslog level */
	int fac = _osrfLogFacility;

	switch( level ) {
		case OSRF_LOG_ERROR:		
			label = "ERR "; 
			lvl = LOG_ERR;
			break;

		case OSRF_LOG_WARNING:	
			label = "WARN"; 
			lvl = LOG_WARNING;
			break;

		case OSRF_LOG_INFO:		
			label = "INFO"; 
			lvl = LOG_INFO;
			break;

		case OSRF_LOG_DEBUG:	
			label = "DEBG"; 
			lvl = LOG_DEBUG;
			break;

		case OSRF_LOG_INTERNAL: 
			label = "INT "; 
			lvl = LOG_DEBUG;
			break;

		case OSRF_LOG_ACTIVITY: 
			label = "ACT"; 
			lvl = LOG_INFO;
			fac = _osrfLogActFacility;
			break;
	}

   char* xid = (_osrfLogXid) ? _osrfLogXid : "";

   int logtype = _osrfLogType;
   if( logtype == OSRF_LOG_TYPE_FILE && !_osrfLogFile )
   {
	   // No log file defined?  Temporarily reroute to stderr
	   logtype = OSRF_LOG_TYPE_STDERR;
   }

   if( logtype == OSRF_LOG_TYPE_SYSLOG ) {
		char buf[1536];  
		buf[0] = '\0';
		/* give syslog some breathing room, and be cute about it */
		strncat(buf, msg, 1535);
		buf[1532] = '.';
		buf[1533] = '.';
		buf[1534] = '.';
		buf[1535] = '\0';
		syslog( fac | lvl, "[%s:%ld:%s:%d:%s] %s", label, (long) getpid(), filename, line, xid, buf );
	}

	else if( logtype == OSRF_LOG_TYPE_FILE )
		_osrfLogToFile( label, (long) getpid(), filename, line, xid, msg );

	else if( logtype == OSRF_LOG_TYPE_STDERR )
		fprintf( stderr, "[%s:%ld:%s:%d:%s] %s\n", label, (long) getpid(), filename, line, xid, msg );
}


/**
	@brief Write a message to a log file.
	@param label The message type: "ERR", "WARN", etc..
	@param pid The process id.
	@param filename Name of the source file from whence the message was issued.
	@param line Line number from whence the message was issued.
	@param xid Transaction id (or an empty string if there is no transaction).
	@param msg Message text.

	Open the log file named by _osrfLogFile, in append mode; write the message; close the
	file.  If unable to open the log file, write the message to standard error.
*/
static void _osrfLogToFile( const char* label, long pid, const char* filename, int line,
	const char* xid, const char* msg ) {

	if( !label || !filename || !xid || !msg )
		return;           // missing parameter(s)

	if(!_osrfLogFile)
		return;           // No log file defined

	if(!_osrfLogAppname)
		_osrfLogAppname = strdup("osrf");   // apply default application name

	char datebuf[36];
	time_t t = time(NULL);
	struct tm* tms = localtime(&t);
	strftime(datebuf, sizeof( datebuf ), "%Y-%m-%d %H:%M:%S", tms);

	FILE* file = fopen(_osrfLogFile, "a");
	if(!file) {
		fprintf(stderr,
			"Unable to fopen log file %s for writing; logging to standard error\n", _osrfLogFile);
		fprintf(stderr, "%s %s [%s:%ld:%s:%d:%s] %s\n",
			_osrfLogAppname, datebuf, label, (long) getpid(), filename, line, xid, msg );

		return;
	}

	fprintf(file, "%s %s [%s:%ld:%s:%d:%s] %s\n",
		_osrfLogAppname, datebuf, label, (long) getpid(), filename, line, xid, msg );
	if( fclose(file) != 0 ) 
		fprintf( stderr, "Error closing log file: %s", strerror(errno));

}

int osrfLogFacilityToInt( const char* facility ) {
	if(!facility) return LOG_LOCAL0;
	if(strlen(facility) < 6) return LOG_LOCAL0;
	switch( facility[5] ) {
		case '0': return LOG_LOCAL0;
		case '1': return LOG_LOCAL1;
		case '2': return LOG_LOCAL2;
		case '3': return LOG_LOCAL3;
		case '4': return LOG_LOCAL4;
		case '5': return LOG_LOCAL5;
		case '6': return LOG_LOCAL6;
		case '7': return LOG_LOCAL7;
	}
	return LOG_LOCAL0;
}


