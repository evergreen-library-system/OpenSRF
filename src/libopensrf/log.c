#include <opensrf/log.h>

static int _osrfLogType				= -1;
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
static void _osrfLogToFile( char* msg, ... );
static void _osrfLogSetXid(char* xid);

#define OSRF_LOG_GO(f,li,m,l)	\
        if(!m) return;		\
        VA_LIST_TO_STRING(m);	\
        _osrfLogDetail( l, f, li, VA_BUF );

void osrfLogCleanup() {
	free(_osrfLogAppname);
	free(_osrfLogFile);
}


void osrfLogInit( int type, const char* appname, int maxlevel ) {
	osrfLogSetType(type);
	if(appname) osrfLogSetAppname(appname);
	osrfLogSetLevel(maxlevel);
	if( type == OSRF_LOG_TYPE_SYSLOG ) 
		openlog(_osrfLogAppname, 0, _osrfLogFacility );
}

static void _osrfLogSetXid(char* xid) {
   if(xid) {
      if(_osrfLogXid) free(_osrfLogXid);
      _osrfLogXid = strdup(xid);
   }
}

void osrfLogClearXid() { _osrfLogSetXid(""); }
void osrfLogSetXid(char* xid) {
   if(!_osrfLogIsClient) _osrfLogSetXid(xid);
}

void osrfLogMkXid() {
   if(_osrfLogIsClient) {
      static int _osrfLogXidInc = 0; /* increments with each new xid for uniqueness */
      char buf[32];
      memset(buf, 0x0, 32);
      snprintf(buf, 32, "%s%d", _osrfLogXidPfx, _osrfLogXidInc);
      _osrfLogSetXid(buf);
      _osrfLogXidInc++;
   }
}

char* osrfLogGetXid() {
   return _osrfLogXid;
}

void osrfLogSetIsClient(int is) {
   _osrfLogIsClient = is;
   if(!is) return;
   /* go ahead and create the xid prefix so it will be consistent later */
   static char buff[32];
   memset(buff, 0x0, 32);
   snprintf(buff, 32, "%d%ld", (int)time(NULL), (long) getpid());
   _osrfLogXidPfx = buff;
}

/** Sets the type of logging to perform.  See log types */
static void osrfLogSetType( int logtype ) { 
	if( logtype != OSRF_LOG_TYPE_FILE &&
			logtype != OSRF_LOG_TYPE_SYSLOG ) {
		fprintf(stderr, "Unrecognized log type.  Logging to stderr\n");
		return;
	}
	_osrfLogType = logtype;
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

	char* l = "INFO";		/* level name */
	int lvl = LOG_INFO;	/* syslog level */
	int fac = _osrfLogFacility;

	switch( level ) {
		case OSRF_LOG_ERROR:		
			l = "ERR "; 
			lvl = LOG_ERR;
			break;

		case OSRF_LOG_WARNING:	
			l = "WARN"; 
			lvl = LOG_WARNING;
			break;

		case OSRF_LOG_INFO:		
			l = "INFO"; 
			lvl = LOG_INFO;
			break;

		case OSRF_LOG_DEBUG:	
			l = "DEBG"; 
			lvl = LOG_DEBUG;
			break;

		case OSRF_LOG_INTERNAL: 
			l = "INT "; 
			lvl = LOG_DEBUG;
			break;

		case OSRF_LOG_ACTIVITY: 
			l = "ACT"; 
			lvl = LOG_INFO;
			fac = _osrfLogActFacility;
			break;
	}

   char* xid = (_osrfLogXid) ? _osrfLogXid : "";

	if(_osrfLogType == OSRF_LOG_TYPE_SYSLOG ) {
		char buf[1536];  
		memset(buf, 0x0, 1536);
		/* give syslog some breathing room, and be cute about it */
		strncat(buf, msg, 1535);
		buf[1532] = '.';
		buf[1533] = '.';
		buf[1534] = '.';
		buf[1535] = '\0';
		syslog( fac | lvl, "[%s:%ld:%s:%d:%s] %s", l, (long) getpid(), filename, line, xid, buf );
	}

	else if( _osrfLogType == OSRF_LOG_TYPE_FILE )
		_osrfLogToFile("[%s:%ld:%s:%d:%s] %s", l, (long) getpid(), filename, line, xid, msg );

}


static void _osrfLogToFile( char* msg, ... ) {

	if(!msg) return;
	if(!_osrfLogFile) return;
	VA_LIST_TO_STRING(msg);

	if(!_osrfLogAppname) _osrfLogAppname = strdup("osrf");
	int l = strlen(VA_BUF) + strlen(_osrfLogAppname) + 36;
	char buf[l];
	bzero(buf,l);

	char datebuf[36];
	bzero(datebuf,36);
	time_t t = time(NULL);
	struct tm* tms = localtime(&t);
	strftime(datebuf, 36, "%Y-%m-%d %H:%M:%S", tms);

	FILE* file = fopen(_osrfLogFile, "a");
	if(!file) {
		fprintf(stderr, "Unable to fopen file %s for writing\n", _osrfLogFile);
		return;
	}

	fprintf(file, "%s %s %s\n", _osrfLogAppname, datebuf, VA_BUF );
	if( fclose(file) != 0 ) 
		osrfLogWarning(OSRF_LOG_MARK, "Error closing log file: %s", strerror(errno));

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


