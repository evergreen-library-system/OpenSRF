/**
	@file log.c
	@brief Routines for logging messages.
*/

#include <opensrf/log.h>

/** Pseudo-log type indicating that no previous log type was defined.
	See also _prevLogType. */
#define OSRF_NO_LOG_TYPE -1

/** Stores a log type during temporary redirections to standard error. */
static int _prevLogType             = OSRF_NO_LOG_TYPE;
/** Defines the destination of log messages: standard error, a log file, or Syslog. */
static int _osrfLogType				= OSRF_LOG_TYPE_STDERR;
/** Defines the Syslog facility number used for log messages other than activity messages.
	Defaults to LOG_LOCAL0. */
static int _osrfLogFacility			= LOG_LOCAL0;
/** Defines the Syslog facility number used for activity messages.  Defaults to LOG_LOCAL1. */
static int _osrfLogActFacility		= LOG_LOCAL1;
/** Name of the log file. */
static char* _osrfLogFile			= NULL;
/** Application name.  This string will preface every log message. */
static char* _osrfLogAppname		= NULL;
static char* _osrfLogTag		= NULL;
/** Maximum message level.  Messages of higher levels will be suppressed.
	Default: OSRF_LOG_INFO. */
static int _osrfLogLevel			= OSRF_LOG_INFO;
/** Boolean.  If true, activity message are enabled.  Default: true. */
static int _osrfLogActivityEnabled	= 1;
/** Boolean.  If true, the current process is a client; otherwise it's a server.  Clients and
	servers have different options with regard to transaction ids.  See osrfLogSetXid(),
	osrfLogForceXid(), and osrfLogMkXid().  Default: false. */
static int _osrfLogIsClient         = 0;

/** An id identifying the current transaction.  If defined, it is included into every
	log message. */
static char* _osrfLogXid            = NULL; /* current xid */
/** A prefix used to generate transaction ids.  It incorporates a timestamp and a process id. */
static char* _osrfLogXidPfx         = NULL; /* xid prefix string */

static void osrfLogSetType( int logtype );
static void _osrfLogDetail( int level, const char* filename, int line, char* msg );
static void _osrfLogToFile( const char* label, long pid, const char* filename, int line,
							const char* xid, const char* msg );
static void _osrfLogSetXid( const char* xid );

/**
	@brief Reset certain local static variables to their initial values.

	Of the various static variables, we here reset only three:
	- application name (deleted)
	- file name of log file (deleted)
	- log type (reset to OSRF_LOG_TYPE_STDERR)
*/
void osrfLogCleanup( void ) {
	if (_osrfLogTag)
		free(_osrfLogTag);
	_osrfLogTag = NULL;
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

	The @a appname string prefaces every log message.  The default application name, if
	not overridden by this function or by osrfLogSetAppname(), is "osrf".

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

/**
	@brief Store a copy of a transaction id for future use.
	@param xid Pointer to the transaction id to be stored.
*/
static void _osrfLogSetXid( const char* xid ) {
   if(xid) {
      if(_osrfLogXid) free(_osrfLogXid);
      _osrfLogXid = strdup(xid);
   }
}

/**
	@brief Store an empty string as the transaction id.
*/
void osrfLogClearXid( void ) { _osrfLogSetXid(""); }

/**
	@brief Store a transaction id, unless running as a client process.
	@param xid Pointer to the new transaction id
*/
void osrfLogSetXid(const char* xid) {
   if(!_osrfLogIsClient) _osrfLogSetXid(xid);
}

/**
	@brief Store a transaction id for future use, whether running as a client or as a server.
	@param xid Pointer to the new transaction id
*/
void osrfLogForceXid(const char* xid) {
   _osrfLogSetXid(xid);
}

/**
	@brief For client processes only: create and store a unique transaction id.

	The generated transaction id concatenates a prefix (which must have been generated
	previously by osrfLogSetIsClient()) and a sequence number that is incremented on each
	call.

	Since the various pieces of the transaction id are of variable length, and not separated
	by any non-numeric characters, the result is not guaranteed to be unique.  However
	collisions should be rare.
*/
void osrfLogMkXid( void ) {
   if(_osrfLogIsClient) {
      static int _osrfLogXidInc = 0; /* increments with each new xid for uniqueness */
      char buf[32];
      snprintf(buf, sizeof(buf), "%s%d", _osrfLogXidPfx, _osrfLogXidInc);
      _osrfLogSetXid(buf);
      _osrfLogXidInc++;
   }
}

/**
	@brief Return a pointer to the currently stored transaction id, if any.
	@return Pointer to the currently stored transaction id.

	If no transaction id has been stored, return NULL.
*/
const char* osrfLogGetXid( void ) {
   return _osrfLogXid;
}

/**
	@brief Note whether the current process is a client; if so, generate a transaction prefix.
	@param is A boolean; true means the current process is a client, false means it isn't.

	The generated prefix concatenates a timestamp (the return from time()) and a process id.
*/
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

/**
	@brief Switch the logging output to standard error (but remember where it @em was going).

	See also osrfRestoreLogType().
*/
void osrfLogToStderr( void )
{
	if( OSRF_NO_LOG_TYPE == _prevLogType ) {
		_prevLogType = _osrfLogType;
		_osrfLogType = OSRF_LOG_TYPE_STDERR;
	}
}

/**
	@brief Switch the logging output to wherever it was going before calling osrfLogtoStderr().

	By using osrfRestoreLogType together with osrfRestoreLogType(), an application can
	temporarily redirect log messages to standard error, either for an interactive
	session or for debugging, and later revert to the original log output.
*/
void osrfRestoreLogType( void )
{
	if( _prevLogType != OSRF_NO_LOG_TYPE ) {
		_osrfLogType = _prevLogType;
		_prevLogType = OSRF_NO_LOG_TYPE;
	}
}

/**
	@brief Store a file name for a log file.
	@param logfile Pointer to the file name.

	The new file name replaces whatever file name was previously in place, if any.

	This function does not affect the logging type.  The choice of file name makes a
	difference only when the logging type is OSRF_LOG_TYPE_FILE.
*/
void osrfLogSetFile( const char* logfile ) {
	if(!logfile) return;
	if(_osrfLogFile) free(_osrfLogFile);
	_osrfLogFile = strdup(logfile);
}

/**
	@brief Enable the issuance of activity log messages.

	By default, activity messages are enabled.  See also osrfLogActivity().
*/
void osrfLogSetActivityEnabled( int enabled ) {
	_osrfLogActivityEnabled = enabled;
}

/**
	@brief Store an application name for future use.
	@param appname Pointer to the application name to be stored.

	The @a appname string prefaces every log message.  The default application name is "osrf".
*/
void osrfLogSetAppname( const char* appname ) {
	if(!appname) return;

	char buf[256];
	if(_osrfLogTag) {
		snprintf(buf, sizeof(buf), "%s/%s", appname, _osrfLogTag);
	} else {
		snprintf(buf, sizeof(buf), "%s", appname);
	}

	if(_osrfLogAppname) free(_osrfLogAppname);
	_osrfLogAppname = strdup(buf);

	/* if syslogging, re-open the log with the appname */
	if( _osrfLogType == OSRF_LOG_TYPE_SYSLOG) {
		closelog();
		openlog(_osrfLogAppname, 0, _osrfLogFacility);
	}
}

/**
	@brief Store a facility number for future use.
	@param facility The facility number to be stored.

	A facility is a small integer passed to the Syslog system to characterize the source
	of a message.  The value of this integer is typically derived from a configuration file.
	If not otherwise specified, it defaults to LOG_LOCAL0.
*/
void osrfLogSetSyslogFacility( int facility ) {
	_osrfLogFacility = facility;
}

/**
        @brief Store an arbitrary program name tag for future use.
        @param logtag The string to be stored.

        A log tag is a short string that is appended to the appname
        we log under.  This can be used to segregate logs from different
        users in, for instance, rsyslogd.
*/

void osrfLogSetLogTag( const char* logtag ) {
	if (logtag) _osrfLogTag = strdup(logtag);
}

/**
	@brief Store a facility number for future use for activity messages.
	@param facility The facility number to be stored.

	A facility is a small integer passed to the Syslog system to characterize the source
	of a message.  The value of this integer is typically derived from a configuration file.

	The facility used for activity messages is separate and distinct from that used for
	other log messages, and defaults to LOG_LOCAL1.
 */
void osrfLogSetSyslogActFacility( int facility ) {
	_osrfLogActFacility = facility;
}

/**
	@brief Set the maximum level of messages to be issued.
	@param loglevel The maximum message level.

	A log message will be issued only if its level is less than or equal to this maximum.
	For example, if @a loglevel is set to OSRF_LOG_INFO, then the logging routines will
	issue information messages, warning messages, and error messages, but not debugging
	messages or internal messages.

	The default message level is OSRF_LOG_INFO.
*/
void osrfLogSetLevel( int loglevel ) {
	_osrfLogLevel = loglevel;
}

/**
	@brief Get the current log message level.
	@return The current log message level.

	See also osrfLogSetLevel().
 */
int osrfLogGetLevel( void ) {
	return _osrfLogLevel;
}

/**
	@name Message Logging Functions
	
	These five functions are the most widely used of the logging routines.  They all work
	the same way, differing only in the levels of the messages they log, and in the tags
	they use within those messages to indicate the message level.

	The first two parameters define the location in the source code where the function is
	called: the name of the source file and the line number.  In practice these are normally
	provided through use of the OSRF_LOG_MARK macro.

	The third parameter is a printf-style format string, which will be expanded to form the
	message text.  Subsequent parameters, if any, will be formatted and inserted into the
	expanded message text.

	Depending on the current maximum message level, the message may or may not actually be
	issued.  See also osrfLogSetLevel().
*/
/*@{*/

/**
	@brief Log an error message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,

	Tag: "ERR".
*/
void osrfLogError( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_ERROR ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_ERROR, file, line, VA_BUF );
}

/**
	@brief Log a warning message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,

	Tag: "WARN".
 */
void osrfLogWarning( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_WARNING ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_WARNING, file, line, VA_BUF );
}

/**
	@brief Log an informational message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,

	Tag: "INFO".
 */
void osrfLogInfo( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_INFO ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_INFO, file, line, VA_BUF );
}

/**
	@brief Log a debug message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,
 
	Tag: "DEBG".
 */
void osrfLogDebug( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_DEBUG ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_DEBUG, file, line, VA_BUF );
}

/**
	@brief Log an internal message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,

	Tag: "INT ".
 */
void osrfLogInternal( const char* file, int line, const char* msg, ... ) {
	if( !msg ) return;
	if( _osrfLogLevel < OSRF_LOG_INTERNAL ) return;
	VA_LIST_TO_STRING( msg );
	_osrfLogDetail( OSRF_LOG_INTERNAL, file, line, VA_BUF );
}

/*@}*/

/**
	@brief Issue activity log message.
	@param file The file name of the source code calling the function.
	@param line The line number of the source code calling the function.
	@param msg A printf-style format string that will be expanded to form the message text,

	Tag: "ACT".

	Activity messages behave like informational messages, with the following differences:
	- They are tagged "ACT" instead of "INFO";
	- Though enabled by default, they may be disabled by a previous call to
		osrfLogSetActivityEnabled();
	- When Syslog is in use, they are assigned a separate facility number, which defaults
		to LOG_LOCAL1 instead of LOG_LOCAL0.  See also osrfLogSetSyslogActFacility().
*/
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

	If the logging type has been set to OSRF_LOG_TYPE_FILE, but no file name has been
	defined for the log file, the message is written to standard error.
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

/**
	@brief Translate a character string to a facility number for Syslog.
	@param facility The string to be translated.
	@return An integer in the range 0 through 7.

	Take the sixth character (counting from 1).  If it's a digit in the range '0' through '7',
	return the corresponding value: LOG_LOCAL0, LOG_LOCAL1, etc...  Otherwise -- or if the
	string isn't long enough -- return LOG_LOCAL0 as a default.

	Example: "LOCAL3" => LOG_LOCAL3.

	(Syslog uses the LOG_LOCALx macros to designate different kinds of locally defined
	facilities that may issue messages.  Depending on the configuration, Syslog mey handle
	messages from different facilities differently.)
*/
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


