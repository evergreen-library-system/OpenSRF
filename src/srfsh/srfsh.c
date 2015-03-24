/**
	@file srfsh.c
	@brief Command-line tool for OSRF
*/

#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_app_session.h>
#include <time.h>
#include <ctype.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <opensrf/utils.h>
#include <opensrf/log.h>

#include <signal.h>

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

/**
	@brief A struct of convenience for parsing a command line
*/
typedef struct {
	const char* itr;            /**< iterator for input buffer */
	growing_buffer* buf;        /**< output buffer */
} ArgParser;

static void get_string_literal( ArgParser* parser );
static void get_json_array( ArgParser* parser );
static void get_json_object( ArgParser* parser );
static void get_misc( ArgParser* parser );

#define SRFSH_PORT 5222
#define COMMAND_BUFSIZE 4096


/* shell prompt */
static const char* prompt = "srfsh# ";

static char* history_file = NULL;

//static int child_dead = 0;

static char* login_session = NULL;

/* true if we're pretty printing json results */
static int pretty_print = 1;
/* true if we're bypassing 'less' */
static int raw_print = 0;

/* our jabber connection */
static transport_client* client = NULL; 

/* the last result we received */
static osrfMessage* last_result = NULL;

/* functions */
static int process_request( const char* request );
static void parse_args( const char* request, osrfStringArray* cmd_array );

/* handles router requests */
static int handle_router( const osrfStringArray* cmd_array );

/* utility method for print time data */
static int handle_time( const osrfStringArray* cmd_array ); 

/* handles app level requests */
static int handle_request( const osrfStringArray* cmd_array, int relay );
static int handle_set( const osrfStringArray* cmd_array );
static int handle_print( const osrfStringArray* cmd_array );
static int send_request( const char* server,
				const char* method, growing_buffer* buffer, int relay );
static int parse_error( const char* request );
static int router_query_servers( const char* server );
static int print_help( void );

//static int srfsh_client_connect();
//static char* tabs(int count);
//static void sig_child_handler( int s );
//static void sig_int_handler( int s );

static char* get_request( void );
static int load_history( void );
static int handle_math( const osrfStringArray* cmd_array );
static int do_math( int count, int style );
static int handle_introspect( const osrfStringArray* cmd_array );
static int handle_login( const osrfStringArray* cmd_array );
static int handle_open( const osrfStringArray* cmd_array );
static int handle_close( const osrfStringArray* cmd_array );
static void close_all_sessions( void );

static int recv_timeout = 120;
static int is_from_script = 0;
static int no_bang = 0;

static osrfHash* server_hash = NULL;

int main( int argc, char* argv[] ) {

	/* --------------------------------------------- */
	/* see if they have a .srfsh.xml in their home directory */
	char* home = getenv("HOME");
	int l = strlen(home) + 36;
	char fbuf[l];
	snprintf(fbuf, sizeof(fbuf), "%s/.srfsh.xml", home);
	
	if(!access(fbuf, R_OK)) {
		if( ! osrf_system_bootstrap_client(fbuf, "srfsh") ) {
			fprintf(stderr,"Unable to bootstrap client for requests\n");
			osrfLogError( OSRF_LOG_MARK,  "Unable to bootstrap client for requests");
			return -1;
		}

	} else {
		fprintf(stderr,"No Config file found at %s\n", fbuf ); 
		return -1;
	}

	if(argc > 1) {
		int f;
		int i;
		for (i = 1; i < argc; i++) {

			if( !strcmp( argv[i], "--safe" ) ) {
				no_bang = 1;
				continue;
			}

			/* for now.. the first unrecognized arg is used as a script file for processing */
			if (is_from_script) continue;

			if( (f = open(argv[i], O_RDONLY)) == -1 ) {
				osrfLogError( OSRF_LOG_MARK, "Unable to open file %s for reading, exiting...", argv[i]);
				return -1;
			}

			if(dup2(f, STDIN_FILENO) == -1) {
				osrfLogError( OSRF_LOG_MARK, "Unable to duplicate STDIN, exiting...");
				return -1;
			}

			close(f);
			is_from_script = 1;
		}
	}
		
	/* --------------------------------------------- */
	load_history();

	client = osrfSystemGetTransportClient();
	osrfAppSessionSetIngress("srfsh");
	
	// Disable special treatment for tabs by readline
	// (by default they invoke command completion, which
	// is not useful for srfsh)
	rl_bind_key( '\t', rl_insert );
	
	/* main process loop */
	int newline_needed = 1;  /* used as boolean */
	char* request;
	while( (request = get_request()) ) {

		// Find first non-whitespace character
		
		char * cmd = request;
		while( isspace( (unsigned char) *cmd ) )
			++cmd;

		// Remove trailing whitespace.  We know at this point that
		// there is at least one non-whitespace character somewhere,
		// or we would have already skipped this line.  Hence we
		// needn't check to make sure that we don't back up past
		// the beginning.

		{
			// The curly braces limit the scope of the end variable
			
			char * end = cmd + strlen(cmd) - 1;
			while( isspace( (unsigned char) *end ) )
				--end;
			end[1] = '\0';
		}

		if( !strcasecmp(cmd, "exit") || !strcasecmp(cmd, "quit"))
		{
			newline_needed = 0;
			break; 
		}

		process_request( cmd );
		if( request && *cmd ) {
			add_history(request);
		}

		free(request);

		fflush(stderr);
		fflush(stdout);
	}

	if( newline_needed ) {
		
		// We left the readline loop after seeing an EOF, not after
		// seeing "quit" or "exit".  So we issue a newline in order
		// to avoid leaving a dangling prompt.

		putchar( '\n' );
	}

	if(history_file != NULL )
		write_history(history_file);

	// Free stuff
	free(request);
	free(login_session);
	if( server_hash ) {
		if( osrfHashGetCount( server_hash ) > 0 )
			close_all_sessions();
		osrfHashFree( server_hash );
	}

	osrf_system_shutdown();
	return 0;
}

// Get a logical line from one or more calls to readline(),
// skipping blank lines and comments.  Stitch continuation
// lines together as needed.  Caller is responsible for
// freeing the string returned.
// If EOF appears before a logical line is completed, 
// return NULL.
static char* get_request( void ) {
	char* line;
	char* p;

	// Get the first physical line of the logical line
	while( 1 ) {
		line = readline( prompt );
		if( ! line )
			return NULL;     // end of file

		// Skip leading white space
		for( p = line; isspace( *p ); ++p )
			;

		if( '\\' == *p && '\0' == p[1] ) {
			// Just a trailing backslash; skip to next line
			free( line );
			continue;
		} else if( '\0' == p[0] || '#' == *p ) {
			free( line );
			continue;  // blank line or comment; skip it
		} else
			break;     // Not blank, not comment; take it
	}

	char* end = line + strlen( line ) - 1;
	if( *end != '\\' )
		return line;    // No continuation line; we're done

	// Remove the trailing backslash and collect
	// the continuation line(s) into a growing_buffer
	*end = '\0';

	growing_buffer* logical_line = buffer_init( 256 );
	buffer_add( logical_line, p );
	free( line );

	// Append any continuation lines
	int finished = 0;      // boolean
	while( !finished ) {
		line = readline( "> " );
		if( line ) {

			// Check for another continuation
			end = line + strlen( line ) - 1;
			if( '\\' == *end )
				*end = '\0';
			else
				finished = 1;

			buffer_add( logical_line, line );
			free( line );
		} else {
			fprintf( stderr, "Expected continuation line; found end of file\n" );
			buffer_free( logical_line );
			return NULL;
		}
	}

	return buffer_release( logical_line );
}

static int load_history( void ) {

	char* home = getenv("HOME");
	int l = strlen(home) + 24;
	char fbuf[l];
	snprintf(fbuf, sizeof(fbuf), "%s/.srfsh_history", home);
	history_file = strdup(fbuf);

	if(!access(history_file, W_OK | R_OK )) {
		history_length = 5000;
		read_history(history_file);
	}
	return 1;
}


static int parse_error( const char* request ) {
	if( ! request )
		return 0;

	fprintf( stderr, "???: %s\n", request );
	return 0;

}


static int process_request( const char* request ) {

	if( request == NULL )
		return 0;

	int ret_val = 0;
	osrfStringArray* cmd_array = osrfNewStringArray( 32 );

	parse_args( request, cmd_array );
	int wordcount = cmd_array->size;
	if( 0 == wordcount ) {
		printf( "No words found in command\n" );
		osrfStringArrayFree( cmd_array );
		return 0;
	}

	/* pass off to the top level command */
	const char* command = osrfStringArrayGetString( cmd_array, 0 );
	if( !strcmp( command, "router" ) )
		ret_val = handle_router( cmd_array );

	else if( !strcmp( command, "time" ) )
		ret_val = handle_time( cmd_array );

	else if ( !strcmp( command, "request" ) )
		ret_val = handle_request( cmd_array, 0 );

	else if ( !strcmp( command, "relay" ) )
		ret_val = handle_request( cmd_array, 1 );

	else if ( !strcmp( command, "help" ) )
		ret_val = print_help();

	else if ( !strcmp( command, "set" ) )
		ret_val = handle_set( cmd_array );

	else if ( !strcmp( command, "print" ) )
		ret_val = handle_print( cmd_array );

	else if ( !strcmp( command, "math_bench" ) )
		ret_val = handle_math( cmd_array );

	else if ( !strcmp( command, "introspect" ) )
		ret_val = handle_introspect( cmd_array );

	else if ( !strcmp( command, "login" ) )
		ret_val = handle_login( cmd_array );

	else if ( !strcmp( command, "open" ) )
		ret_val = handle_open( cmd_array );

	else if ( !strcmp( command, "close" ) )
		ret_val = handle_close( cmd_array );

	else if ( request[0] == '!') {
		if (!no_bang) system( request + 1 );
		ret_val = 1;
	}

	osrfStringArrayFree( cmd_array );

	if(!ret_val)
		return parse_error( request );
	else
		return 1;
}


static int handle_introspect( const osrfStringArray* cmd_array ) {

	const char* service = osrfStringArrayGetString( cmd_array, 1 );
	if( ! service )
		return 0;

	fprintf(stderr, "--> %s\n", service );

	// Build a command in a suitably-sized
	// buffer and then parse it

	size_t len;
	const char* method = osrfStringArrayGetString( cmd_array, 2 );
	if( method ) {
		static const char text[] = "request %s opensrf.system.method %s";
		len = sizeof( text ) + strlen( service ) + strlen( method );
		char buf[len];
		snprintf( buf, sizeof(buf), text, service, method );
		return process_request( buf );

	} else {
		static const char text[] = "request %s opensrf.system.method.all";
		len = sizeof( text ) + strlen( service );
		char buf[len];
		snprintf( buf, sizeof(buf), text, service );
		return process_request( buf );

	}
}


static int handle_login( const osrfStringArray* cmd_array ) {

	const char* username = osrfStringArrayGetString( cmd_array, 1 );
	const char* password = osrfStringArrayGetString( cmd_array, 2 );

	if( username && password ) {

		const char* type		= osrfStringArrayGetString( cmd_array, 3 );
		const char* orgloc		= osrfStringArrayGetString( cmd_array, 4 );
		const char* workstation	= osrfStringArrayGetString( cmd_array, 5 );
		int orgloci = (orgloc) ? atoi(orgloc) : 0;
		if(!type) type = "opac";

		char login_text[] = "request open-ils.auth open-ils.auth.authenticate.init \"%s\"";
		size_t len = sizeof( login_text ) + strlen(username) + 1;

		char buf[len];
		snprintf( buf, sizeof(buf), login_text, username );
		process_request(buf);

		const char* hash;
		if(last_result && last_result->_result_content) {
			jsonObject* r = last_result->_result_content;
			hash = jsonObjectGetString(r);
		} else return 0;

		char* pass_buf = md5sum(password);

		size_t both_len = strlen( hash ) + strlen( pass_buf ) + 1;
		char both_buf[both_len];
		snprintf(both_buf, sizeof(both_buf), "%s%s", hash, pass_buf);

		char* mess_buf = md5sum(both_buf);

		growing_buffer* argbuf = buffer_init(64);
		buffer_fadd(argbuf, 
				"request open-ils.auth open-ils.auth.authenticate.complete "
				"{ \"username\" : \"%s\", \"password\" : \"%s\"", username, mess_buf );

		if(type) buffer_fadd( argbuf, ", \"type\" : \"%s\"", type );
		if(orgloci) buffer_fadd( argbuf, ", \"org\" : %d", orgloci );
		if(workstation) buffer_fadd( argbuf, ", \"workstation\" : \"%s\"", workstation);
		buffer_add_char( argbuf, '}' );

		free(pass_buf);
		free(mess_buf);

		process_request( argbuf->buf );
		buffer_free(argbuf);

		if( login_session != NULL )
			free( login_session );

		const jsonObject* x = last_result->_result_content;
		double authtime = 0;
		if(x) {
			const char* authtoken = jsonObjectGetString(
					jsonObjectGetKeyConst(jsonObjectGetKeyConst(x,"payload"), "authtoken"));
			authtime  = jsonObjectGetNumber(
					jsonObjectGetKeyConst(jsonObjectGetKeyConst(x,"payload"), "authtime"));

			if(authtoken)
				login_session = strdup(authtoken);
			else
				login_session = NULL;
		}
		else login_session = NULL;

		printf("Login Session: %s.  Session timeout: %f\n",
			   (login_session ? login_session : "(none)"), authtime );
		
		return 1;

	}

	return 0;
}

/**
	@brief Open connections to one or more specified services.
	@param cmd_array Pointer to a list of command line chunks.
	@return 1 in all cases.

	The first chunk of the command line is the "open" command.  Subsequent chunks, if any,
	are server names.

	Try to open all specified servers.  If no servers are specified, report what servers are
	currently open.
*/
static int handle_open( const osrfStringArray* cmd_array ) {
	if( NULL == osrfStringArrayGetString( cmd_array, 1 ) ) {
		if( ! server_hash || osrfHashGetCount( server_hash ) == 0 ) {
			printf( "No services are currently open\n" );
			return 1;
		}

		printf( "Service(s) currently open:\n" );

		osrfHashIterator* itr = osrfNewHashIterator( server_hash );
		while( osrfHashIteratorNext( itr ) ) {
			printf( "\t%s\n", osrfHashIteratorKey( itr ) );
		}
		osrfHashIteratorFree( itr );
		return 1;
	}

	if( ! server_hash )
		server_hash = osrfNewHash( 6 );

	int i;
	for( i = 1; ; ++i ) {    // for each requested service
		const char* server = osrfStringArrayGetString( cmd_array, i );
		if( ! server )
			break;

		if( osrfHashGet( server_hash, server ) ) {
			printf( "Service %s is already open\n", server );
			continue;
		}

		// Try to open a session with the current specified server
		osrfAppSession* session = osrfAppSessionClientInit(server);

		if(!osrfAppSessionConnect(session)) {
			fprintf(stderr, "Unable to open service %s\n", server);
			osrfLogWarning( OSRF_LOG_MARK, "Unable to open remote service %s\n", server );
			osrfAppSessionFree( session );
		} else {
			osrfHashSet( server_hash, session, server );
			printf( "Service %s opened\n", server );
		}
	}

	return 1;
}

/**
	@brief Close connections to one or more specified services.
	@param cmd_array Pointer to a list of command line chunks.
	@return 1 if any services were closed, or 0 if there were none to close.

	The first chunk of the command line is the "close" command.  Subsequent chunks, if any,
	are server names.
*/
static int handle_close( const osrfStringArray* cmd_array ) {
	if( cmd_array->size < 2 ) {
		fprintf( stderr, "No service specified for close\n" );
		return 0;
	}

	int i;
	for( i = 1; ; ++i ) {
		const char* server = osrfStringArrayGetString( cmd_array, i );
		if( ! server )
			break;

		osrfAppSession* session = osrfHashRemove( server_hash, server );
		if( ! session ) {
			printf( "Service \"%s\" is not open\n", server );
			continue;
		}

		osrf_app_session_disconnect( session );
		osrfAppSessionFree( session );
		printf( "Service \"%s\" closed\n", server );
	}

	return 1;
}

/**
	@brief Close all currently open connections to services.
 */
static void close_all_sessions( void ) {

	osrfAppSession* session;
	osrfHashIterator* itr = osrfNewHashIterator( server_hash );

	while(( session = osrfHashIteratorNext( itr ) )) {
		osrf_app_session_disconnect( session );
		osrfAppSessionFree( session );
	}

	osrfHashIteratorFree( itr );
}

static int handle_set( const osrfStringArray* cmd_array ) {

	const char* variable = osrfStringArrayGetString( cmd_array, 1 );
	if( variable ) {

		const char* val = osrfStringArrayGetString( cmd_array, 2 );
		if( val ) {

			if(!strcmp(variable,"pretty_print")) {
				if(!strcmp(val,"true")) {
					pretty_print = 1;
					printf("pretty_print = true\n");
					return 1;
				} else if(!strcmp(val,"false")) {
					pretty_print = 0;
					printf("pretty_print = false\n");
					return 1;
				}
			}

			if(!strcmp(variable,"raw_print")) {
				if(!strcmp(val,"true")) {
					raw_print = 1;
					printf("raw_print = true\n");
					return 1;
				} else if(!strcmp(val,"false")) {
					raw_print = 0;
					printf("raw_print = false\n");
					return 1;
				}
			}

		}
	}

	return 0;
}


static int handle_print( const osrfStringArray* cmd_array ) {

	const char* variable = osrfStringArrayGetString( cmd_array, 1 );

	if( variable ) {
		if(!strcmp(variable,"pretty_print")) {
			if(pretty_print) {
				printf("pretty_print = true\n");
				return 1;
			} else {
				printf("pretty_print = false\n");
				return 1;
			}
		}

		if(!strcmp(variable,"raw_print")) {
			if(raw_print) {
				printf("raw_print = true\n");
				return 1;
			} else {
				printf("raw_print = false\n");
				return 1;
			}
		}

		if(!strcmp(variable,"login")) {
			printf("login session = %s\n",
				   login_session ? login_session : "(none)" );
			return 1;
		}

	}
	return 0;
}

static int handle_router( const osrfStringArray* cmd_array ) {

	if(!client)
		return 1;

	int i;

	const char* word_1 = osrfStringArrayGetString( cmd_array, 1 );
	const char* word_2 = osrfStringArrayGetString( cmd_array, 2 );
	if( word_1 ) {
		if( !strcmp( word_1,"query") ) {

			if( word_2 && !strcmp( word_2, "servers" ) ) {
				for( i=3; i < COMMAND_BUFSIZE - 3; i++ ) {
					const char* word = osrfStringArrayGetString( cmd_array, i );
					if( word )
						router_query_servers( word );
					else
						break;
				}
				return 1;
			}
			return 0;
		}
		return 0;
	}
	return 0;
}



static int handle_request( const osrfStringArray* cmd_array, int relay ) {

	if(!client)
		return 1;

	const char* server = osrfStringArrayGetString( cmd_array, 1 );
	const char* method = osrfStringArrayGetString( cmd_array, 2 );

	if( server ) {
		int i;
		growing_buffer* buffer = NULL;
		if( !relay ) {
			int first = 1;   // boolean
			buffer = buffer_init( 128 );
			buffer_add_char( buffer, '[' );
			for(i = 3; ; i++ ) {
				const char* word = osrfStringArrayGetString( cmd_array, i );
				if( !word )
					break;

				if( first )
					first = 0;
				else
					buffer_add( buffer, ", " );

				buffer_add( buffer, word );

				/* remove trailing semicolon if user accidentally entered it */
				if( word[ strlen( word ) - 1 ] == ';' )
					buffer_chomp( buffer );
			}
			buffer_add_char( buffer, ']' );
		}

		int rc = send_request( server, method, buffer, relay );
		buffer_free( buffer );
		return rc;
	} 

	return 0;
}

int send_request( const char* server,
		const char* method, growing_buffer* buffer, int relay ) {
	if( server == NULL || method == NULL )
		return 0;

	jsonObject* params = NULL;
	if( !relay ) {
		if( buffer != NULL && buffer->n_used > 0 ) {
			// Temporarily redirect parsing error messages to stderr
			osrfLogToStderr();
			params = jsonParse( OSRF_BUFFER_C_STR( buffer ) );
			osrfRestoreLogType();
		}
	} else {
		if(!last_result || ! last_result->_result_content) { 
			printf("We're not going to call 'relay' with no result params\n");
			return 1;
		}
		else {
			params = jsonNewObject(NULL);
			jsonObjectPush(params, last_result->_result_content );
		}
	}

	if(buffer->n_used > 0 && params == NULL) {
		fprintf(stderr, "JSON error detected, not executing\n");
		jsonObjectFree(params);
		return 1;
	}

	int session_is_temporary;    // boolean
	osrfAppSession* session = osrfHashGet( server_hash, server );
	if( session ) {
		session_is_temporary = 0;     // use an existing session
	} else {
		session = osrfAppSessionClientInit(server);   // open a session
		session_is_temporary = 1;                     // just for this request
	}

	double start = get_timestamp_millis();

	int req_id = osrfAppSessionSendRequest( session, params, method, 1 );
	if( -1 == req_id ) {
		fprintf(stderr, "Unable to communicate with service %s\n", server);
		osrfLogWarning( OSRF_LOG_MARK,
				"Unable to communicate with remote service %s\n", server );
		osrfAppSessionFree( session );
		jsonObjectFree(params);
		return 1;
	}
	jsonObjectFree(params);

	osrfMessage* omsg = osrfAppSessionRequestRecv( session, req_id, recv_timeout );

	if(!omsg) 
		printf("\nReceived no data from server\n");

	signal(SIGPIPE, SIG_IGN);

	FILE* less; 
	if(!is_from_script) less = popen( "less -EX", "w");
	else less = stdout;

	if( less == NULL ) { less = stdout; }

	growing_buffer* resp_buffer = buffer_init(4096);

	while(omsg) {

		if(raw_print) {

			if(omsg->_result_content) {
	
				osrfMessageFree(last_result);
				last_result = omsg;
	
				char* content;
	
				if( pretty_print ) {
					char* j = jsonObjectToJSON(omsg->_result_content);
					if( j ) {
						content = jsonFormatString(j);
						free(j);
					} else
						content = strdup( "(null)" );
				} else {
					content = jsonObjectToJSON(omsg->_result_content);
					if( ! content )
						content = strdup( "(null)" );
				}

				printf( "\nReceived Data: %s\n", content );
				free(content);
	
			} else {

				char code[16];
				snprintf( code, sizeof(code), "%d", omsg->status_code );
				buffer_add( resp_buffer, code );

				printf( "\nReceived Exception:\nName: %s\nStatus: %s\nStatus: %s\n", 
						omsg->status_name, omsg->status_text, code );

				fflush(stdout);
				osrfMessageFree(omsg);
			}

		} else {

			if(omsg->_result_content) {
	
				osrfMessageFree(last_result);
				last_result = omsg;
	
				char* content;
	
				if( pretty_print && omsg->_result_content ) {
					char* j = jsonObjectToJSON(omsg->_result_content);
					if( j ) {
						content = jsonFormatString(j);
						free(j);
					} else
						content = strdup( "(null)" );
				} else {
					content = jsonObjectToJSON(omsg->_result_content);
					if( ! content )
						content = strdup( "(null)" );
				}

				buffer_add( resp_buffer, "\nReceived Data: " );
				buffer_add( resp_buffer, content );
				buffer_add_char( resp_buffer, '\n' );
				free(content);

			} else {

				buffer_add( resp_buffer, "\nReceived Exception:\nName: " );
				buffer_add( resp_buffer, omsg->status_name );
				buffer_add( resp_buffer, "\nStatus: " );
				buffer_add( resp_buffer, omsg->status_text );
				buffer_add( resp_buffer, "\nStatus: " );
				char code[16];
				snprintf( code, sizeof(code), "%d", omsg->status_code );
				buffer_add( resp_buffer, code );
				osrfMessageFree(omsg);
			}
		}

		omsg = osrfAppSessionRequestRecv( session, req_id, recv_timeout );

	}

	double end = get_timestamp_millis();

	fputs( resp_buffer->buf, less );
	buffer_free( resp_buffer );
	fputs("\n------------------------------------\n", less);
	if( osrf_app_session_request_complete( session, req_id ))
		fputs("Request Completed Successfully\n", less);

	fprintf(less, "Request Time in seconds: %.6f\n", end - start );
	fputs("------------------------------------\n", less);

	pclose(less);

	osrf_app_session_request_finish( session, req_id );

	if( session_is_temporary )
		osrfAppSessionFree( session );

	return 1;


}

static int handle_time( const osrfStringArray* cmd_array ) {

	const char* word_1 = osrfStringArrayGetString( cmd_array, 1 );
	if( !word_1 ) {
		printf("%f\n", get_timestamp_millis());
	} else {
		time_t epoch = (time_t) atoi( word_1 );
		printf("%s", ctime(&epoch));
	}
	return 1;
}


static int router_query_servers( const char* router_server ) {

	if( ! router_server || strlen(router_server) == 0 ) 
		return 0;

	const static char router_text[] = "router@%s/router";
	size_t len = sizeof( router_text ) + strlen( router_server ) + 1;
	char rbuf[len];
	snprintf(rbuf, sizeof(rbuf), router_text, router_server );
		
	transport_message* send = 
		message_init( "servers", NULL, NULL, rbuf, NULL );
	message_set_router_info( send, NULL, NULL, NULL, "query", 0 );

	client_send_message( client, send );
	message_free( send );

	transport_message* recv = client_recv( client, -1 );
	if( recv == NULL ) {
		fprintf(stderr, "NULL message received from router\n");
		return 1;
	}
	
	printf( 
			"---------------------------------------------------------------------------------\n"
			"Received from 'server' query on %s\n"
			"---------------------------------------------------------------------------------\n"
			"original reg time | latest reg time | last used time | class | server\n"
			"---------------------------------------------------------------------------------\n"
			"%s"
			"---------------------------------------------------------------------------------\n"
			, router_server, recv->body );

	message_free( recv );
	
	return 1;
}

static int print_help( void ) {

	fputs(
			"---------------------------------------------------------------------------------\n"
			"General commands:\n"
			"---------------------------------------------------------------------------------\n"
			"help                   - Display this message\n"
			"!<command> [args]      - Forks and runs the given command in the shell\n"
		/*
			"time			- Prints the current time\n"
			"time <timestamp>	- Formats seconds since epoch into readable format\n"
		*/
			"set <variable> <value> - Set a srfsh variable (e.g. set pretty_print true )\n"
			"print <variable>       - Displays the value of a srfsh variable\n"
			"\n"
			"---------------------------------------------------------------------------------\n"
			"Variables:\n"
			"---------------------------------------------------------------------------------\n"
			"pretty_print            - Display nicely formatted JSON results\n"
			"       - Accepted values: true, false\n"
			"       - Default value: true\n"
			"\n"
			"raw_print               - Pass JSON results through 'less' paging command\n"
			"       - Accepted values: true, false\n"
			"       - Default value: false\n"
			"\n"
			"---------------------------------------------------------------------------------\n"
			"Commands for OpenSRF services and methods:\n"
			"---------------------------------------------------------------------------------\n"
			"introspect <service> [\"method-name\"]\n"
			"       - Prints service API, limited to the methods that match the optional\n"
			"                right-truncated method-name parameter\n"
			"\n"
			"request <service> <method> [ <JSON formatted string of params> ]\n"
			"       - Anything passed in will be wrapped in a JSON array,\n"
			"               so add commas if there is more than one param\n"
			"\n"
			"router query servers <server1 [, server2, ...]>\n"
			"       - Returns stats on connected services\n"
			"\n"
			"relay <service> <method>\n"
			"       - Performs the requested query using the last received result as the param\n"
			"\n"
			"math_bench <num_batches> [0|1|2]\n"
			"       - 0 means don't reconnect, 1 means reconnect after each batch of 4, and\n"
			"                2 means reconnect after every request\n"
			"\n"
			"---------------------------------------------------------------------------------\n"
			" Commands for Evergreen\n"
			"---------------------------------------------------------------------------------\n"
			"login <username> <password> [type] [org_unit] [workstation]\n"
			"       - Logs into the 'server' and displays the session id\n"
			"       - To view the session id later, enter: print login\n"
			"---------------------------------------------------------------------------------\n"
			"\n"
			"Note: long output is piped through 'less' unless the 'raw_print' variable\n"
			"is true.  To search in 'less', type: /<search>\n"
			"---------------------------------------------------------------------------------\n"
			"\n",
			stdout );

	return 1;
}


/*
static char* tabs(int count) {
	growing_buffer* buf = buffer_init(24);
	int i;
	for(i=0;i!=count;i++)
		buffer_add(buf, "  ");

	char* final = buffer_data( buf );
	buffer_free( buf );
	return final;
}
*/

/**
	@brief Execute the "math_bench" command.
	@param cmd_array A list of command arguments.
	@return 1 if successful, 0 if not.

	The first command argument is required.  It is the number of iterations requested.  If
	it is less than 1, it is coerced to 1.

	The second command argument is optional, with allowed values of 0 (the default), 1, or 2.
	It controls when and whether we call osrf_app_session_disconnect().  If this argument is
	out of range, it is coerced to a value of 0 or 2.
*/
static int handle_math( const osrfStringArray* cmd_array ) {
	const char* word = osrfStringArrayGetString( cmd_array, 1 );
	if( word ) {
		int count = atoi( word );
		if( count < 1 )
			count = 1;

		int style = 0;
		const char* style_arg = osrfStringArrayGetString( cmd_array, 2 );
		if( style_arg ) {
			style = atoi( style_arg );
			if( style > 2 )
				style = 2;
			else if( style < 0 )
				style = 0;
		}

		return do_math( count, style );
	}
	return 0;
}


static int do_math( int count, int style ) {

	osrfAppSession* session = osrfAppSessionClientInit( "opensrf.math" );
	osrfAppSessionConnect(session);

	jsonObject* params = jsonNewObjectType( JSON_ARRAY );
	jsonObjectPush(params,jsonNewObject("1"));
	jsonObjectPush(params,jsonNewObject("2"));

	char* methods[] = { "add", "sub", "mult", "div" };
	char* answers[] = { "3", "-1", "2", "0.5" };

	// Initialize timings to zero.  This shouldn't make a difference, because
	// we overwrite each timing anyway before reporting them.
	float times[ count * 4 ];
	int fi;
	for( fi = 0; fi < count; ++fi )
		times[ fi ] = 0.0;

	int k;
	for(k=0;k!=100;k++) {
		if(!(k%10)) 
			fprintf(stderr,"|");
		else
			fprintf(stderr,".");
	}

	fprintf(stderr,"\n\n");

	int running = 0;
	int i;
	for(i=0; i!= count; i++) {

		int j;
		for(j=0; j != 4; j++) {

			++running;

			double start = get_timestamp_millis();
			int req_id = osrfAppSessionSendRequest( session, params, methods[j], 1 );
			osrfMessage* omsg = osrfAppSessionRequestRecv( session, req_id, 5 );
			double end = get_timestamp_millis();

			times[(4*i) + j] = end - start;

			if(omsg) {
	
				if(omsg->_result_content) {
					char* jsn = jsonObjectToJSON(omsg->_result_content);
					if(!strcmp(jsn, answers[j]))
						fprintf(stderr, "+");
					else
						fprintf(stderr, "\n![%s] - should be %s\n", jsn, answers[j] );
					free(jsn);
				}


				osrfMessageFree(omsg);
		
			} else { fprintf( stderr, "\nempty message for tt: %d\n", req_id ); }

			osrf_app_session_request_finish( session, req_id );

			if(style == 2)
				osrf_app_session_disconnect( session );

			if(!(running%100))
				fprintf(stderr,"\n");
		}

		if(style==1)
			osrf_app_session_disconnect( session );
	}

	osrfAppSessionFree( session );
	jsonObjectFree(params);

	int c;
	float total = 0;
	for(c=0; c!= count*4; c++) 
		total += times[c];

	float avg = total / (count*4); 
	fprintf(stderr, "\n      Average round trip time: %f\n", avg );

	return 1;
}

/**
	@name Command line parser

	This group of functions parses the command line into a series of chunks, and stores
	the chunks in an osrfStringArray.

	A chunk may consist of a JSON string, complete with square brackets, curly braces, and
	embedded white space.  It wouldn't work simply to break up the line into  tokens
	separated by white space.  Sometimes white space separates chunks, and sometimes it
	occurs within a chunk.

	When it sees a left square bracket or curly brace, the parser goes into JSON mode,
	collecting characters up to the corresponding right square bracket or curly brace.
	It also eliminates most kinds of unnecessary white space.

	The JSON parsing is rudimentary.  It does not validate the syntax -- it merely looks
	for the end of the JSON string.  Eventually the JSON string will be passed to a real
	JSON parser, which will detect and report syntax errors.

	When not in JSON mode, the parser collects tokens separated by white space.  It also
	collects character strings in quotation marks, possibly including embedded white space.
	Within a quoted string, an embedded quotation mark does not terminate the string if it
	is escaped by a preceding backslash.
*/

/**
	@brief Collect a string literal enclosed by quotation marks.
	@param parser Pointer to an ArgParser

	A quotation mark serves as a terminator unless it is escaped by a preceding backslash.
	In the latter case, we collect both the backslash and the escaped quotation mark.
*/
static void get_string_literal( ArgParser* parser ) {
	// Collect character until the first unescaped quotation mark, or EOL
	do {
		OSRF_BUFFER_ADD_CHAR( parser->buf, *parser->itr );

		// Don't stop at a quotation mark if it's escaped
		if( '\\' == *parser->itr && '\"' == *( parser->itr + 1 ) ) {
				OSRF_BUFFER_ADD_CHAR( parser->buf, '\"' );
				++parser->itr;
		}

		++parser->itr;
	} while( *parser->itr && *parser->itr != '\"' );

	OSRF_BUFFER_ADD_CHAR( parser->buf, '\"' );
	++parser->itr;
}

/**
	@brief Collect a JSON array (enclosed by square brackets).
	@param parser Pointer to an ArgParser.

	Collect characters until you find the closing square bracket.  Collect any intervening
	JSON arrays, JSON objects, or string literals recursively.
 */
static void get_json_array( ArgParser* parser ) {

	OSRF_BUFFER_ADD_CHAR( parser->buf, '[' );
	++parser->itr;

	// Collect characters through the closing square bracket
	while( *parser->itr != ']' ) {

		if( '\"' == *parser->itr ) {
			get_string_literal( parser );
		} else if( '[' == *parser->itr ) {
			get_json_array( parser );
		} else if( '{' == *parser->itr ) {
			get_json_object( parser );
		} else if( isspace( (unsigned char) *parser->itr ) ) {
			++parser->itr;   // Ignore white space
		} else if ( '\0' == *parser->itr ) {
			return;   // Ignore failure to close the object
		} else {
			get_misc( parser );
			// make sure that bare words don't run together
			OSRF_BUFFER_ADD_CHAR( parser->buf, ' ' );
		}
	} // end while

	OSRF_BUFFER_ADD_CHAR( parser->buf, ']' );
	++parser->itr;
}

/**
	@brief Collect a JSON object (enclosed by curly braces).
	@param parser Pointer to an ArgParser.

	Collect characters until you find the closing curly brace.  Collect any intervening
	JSON arrays, JSON objects, or string literals recursively.
 */
static void get_json_object( ArgParser* parser ) {

	OSRF_BUFFER_ADD_CHAR( parser->buf, '{' );
	++parser->itr;

	// Collect characters through the closing curly brace
	while( *parser->itr != '}' ) {

		if( '\"' == *parser->itr ) {
			get_string_literal( parser );
		} else if( '[' == *parser->itr ) {
			get_json_array( parser );
		} else if( '{' == *parser->itr ) {
			get_json_object( parser );
		} else if( isspace( (unsigned char) *parser->itr ) ) {
			++parser->itr;   // Ignore white space
		} else if ( '\0' == *parser->itr ) {
			return;   // Ignore failure to close the object
		} else {
			get_misc( parser );
			// make sure that bare words don't run together
			OSRF_BUFFER_ADD_CHAR( parser->buf, ' ' );
		}
	} // end while

	OSRF_BUFFER_ADD_CHAR( parser->buf, '}' );
	++parser->itr;
}

/**
	@brief Collect a token terminated by white space or a ']' or '}' character.
	@param parser Pointer to an ArgParser

	For valid JSON, the chunk collected here would be either a number or one of the
	JSON key words "null", "true", or "false".  However at this stage we're not finicky.
	We just collect whatever we see until we find a terminator.
*/
static void get_misc( ArgParser* parser ) {
	// Collect characters until we see one that doesn't belong
	while( 1 ) {
		OSRF_BUFFER_ADD_CHAR( parser->buf, *parser->itr );
		++parser->itr;
		char c = *parser->itr;
		if( '\0' == c || isspace( (unsigned char) c ) 
			|| '{' == c || '}' == c || '[' == c || ']' == c || '\"' == c ) {
			break;
		}
	}
}

/**
	@brief Parse the command line.
	@param request Pointer to the command line
	@param cmd_array Pointer to an osrfStringArray to hold the output of the parser.

	The parser operates by recursive descent.  We build each chunk of command line in a
	growing_buffer belonging to an ArgParser, and then load the chunk into a slot in an
	osrfStringArray supplied by the calling code.
*/
static void parse_args( const char* request, osrfStringArray* cmd_array )
{
	ArgParser parser;

	// Initialize the ArgParser
	parser.itr = request;
	parser.buf = buffer_init( 128 );

	int done = 0;               // boolean
	while( !done ) {
		OSRF_BUFFER_RESET( parser.buf );

		// skip any white space or commas
		while( *parser.itr
			   && ( isspace( (unsigned char) *parser.itr ) || ',' == *parser.itr ) )
			++parser.itr;

		if( '\0' == *parser.itr )
			done = 1;
		else if( '{' == *parser.itr ) {
			// Load a JSON object
			get_json_object( &parser );
		} else if( '[' == *parser.itr ) {
			// Load a JSON array
			get_json_array( &parser );
		} else if( '\"' == *parser.itr ) {
			// Load a string literal
			get_string_literal( &parser );
		} else {
			// Anything else is delimited by white space
			do {
				OSRF_BUFFER_ADD_CHAR( parser.buf, *parser.itr );
				++parser.itr;
			} while( *parser.itr && ! isspace( (unsigned char) *parser.itr ) );
		}

		// Remove a trailing comma, if present
		char lastc = OSRF_BUFFER_C_STR( parser.buf )[
			strlen( OSRF_BUFFER_C_STR( parser.buf )  ) - 1 ];
		if( ',' == lastc )
			buffer_chomp( parser.buf );

		// Add the chunk to the osrfStringArray
		const char* s = OSRF_BUFFER_C_STR( parser.buf );
		if( s && *s ) {
			osrfStringArrayAdd( cmd_array, s );
		}
	}

	buffer_free( parser.buf );
}
/*@}*/

