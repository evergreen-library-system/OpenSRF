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
static void _osrfLogToFile( const char* msg, ... );
static void _osrfLogSetXid( const char* xid );

#define OSRF_LOG_GO(f,li,m,l)	\
        if(!m) return;		\
        VA_LIST_TO_STRING(m);	\
        _osrfLogDetail( l, f, li, VA_BUF );

void osrfLogCleanup( void ) {
	free(_osrfLogAppname);
	_osrfLogAppname = NULL;
	free(_osrfLogFile);
	_osrfLogFile = NULL;
	_osrfLogType = OSRF_LOG_TYPE_STDERR;
}


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

/** Sets the type of logging to perform.  See log types */
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

void osrfLogError( const char* file, int line, const char* msg, ... ) 
	{ OSRF_LOG_GO(file, line, msg, OSRF_LOG_ERROR); }
void osrfLogWarning( const char* file, int line, const char* msg, ... ) 
	{ OSRF_LOG_GO(file, line, msg, OSRF_LOG_WARNING); }
void osrfLogInfo( const char* file, int line, const char* msg, ... ) 
	{ OSRF_LOG_GO(file, line, msg, OSRF_LOG_INFO); }
void osrfLogDebug( const char* file, int line, const char* msg, ... ) 
	{ OSRF_LOG_GO(file, line, msg, OSRF_LOG_DEBUG); }
void osrfLogInternal( const char* file, int line, const char* msg, ... ) 
	{ OSRF_LOG_GO(file, line, msg, OSRF_LOG_INTERNAL); }
void osrfLogActivity( const char* file, int line, const char* msg, ... ) { 
	OSRF_LOG_GO(file, line, msg, OSRF_LOG_ACTIVITY); 
	_osrfLogDetail( OSRF_LOG_INFO, file, line, VA_BUF ); /* also log at info level */
}

/** Actually does the logging */
static void _osrfLogDetail( int level, const char* filename, int line, char* msg ) {

	if( level == OSRF_LOG_ACTIVITY && ! _osrfLogActivityEnabled ) return;
	if( level > _osrfLogLevel ) return;
	if(!msg) return;
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
		_osrfLogToFile( "[%s:%ld:%s:%d:%s] %s", label, (long) getpid(), filename, line, xid, msg );

	else if( logtype == OSRF_LOG_TYPE_STDERR )
		fprintf( stderr, "[%s:%ld:%s:%d:%s] %s\n", label, (long) getpid(), filename, line, xid, msg );
}


static void _osrfLogToFile( const char* msg, ... ) {

	if(!msg) return;
	if(!_osrfLogFile) return;
	VA_LIST_TO_STRING(msg);

	if(!_osrfLogAppname) _osrfLogAppname = strdup("osrf");

	char datebuf[36];
	time_t t = time(NULL);
	struct tm* tms = localtime(&t);
	strftime(datebuf, sizeof( datebuf ), "%Y-%m-%d %H:%M:%S", tms);

	FILE* file = fopen(_osrfLogFile, "a");
	if(!file) {
		fprintf(stderr,
			"Unable to fopen log file %s for writing; logging to standard error\n", _osrfLogFile);
		fprintf(stderr, "%s %s %s\n", _osrfLogAppname, datebuf, VA_BUF );

		return;
	}

	fprintf(file, "%s %s %s\n", _osrfLogAppname, datebuf, VA_BUF );
	if( fclose(file) != 0 ) 
		fprintf( stderr, "Error closing log file: %s", strerror(errno));

}


int osrfLogFacilityToInt( char* facility ) {
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


