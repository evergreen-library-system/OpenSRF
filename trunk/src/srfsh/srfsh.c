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
static osrf_message* last_result = NULL;

/* functions */
static int parse_request( char* request );

/* handles router requests */
static int handle_router( char* words[] );

/* utility method for print time data */
static int handle_time( char* words[] ); 

/* handles app level requests */
static int handle_request( char* words[], int relay );
static int handle_set( char* words[]);
static int handle_print( char* words[]);
static int send_request( char* server, 
				  char* method, growing_buffer* buffer, int relay );
static int parse_error( char* words[] );
static int router_query_servers( const char* server );
static int print_help( void );

//static int srfsh_client_connect();
//static char* tabs(int count);
//static void sig_child_handler( int s );
//static void sig_int_handler( int s );

static int load_history( void );
static int handle_math( char* words[] );
static int do_math( int count, int style );
static int handle_introspect(char* words[]);
static int handle_login( char* words[]);

static int recv_timeout = 120;
static int is_from_script = 0;

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
		/* for now.. the first arg is used as a script file for processing */
		int f;
		if( (f = open(argv[1], O_RDONLY)) == -1 ) {
			osrfLogError( OSRF_LOG_MARK, "Unable to open file %s for reading, exiting...", argv[1]);
			return -1;
		}

		if(dup2(f, STDIN_FILENO) == -1) {
			osrfLogError( OSRF_LOG_MARK, "Unable to duplicate STDIN, exiting...");
			return -1;
		}

		close(f);
		is_from_script = 1;
	}
		
	/* --------------------------------------------- */
	load_history();


	client = osrfSystemGetTransportClient();

	/* main process loop */
	int newline_needed = 1;  /* used as boolean */
	char* request;
	while((request=readline(prompt))) {

		// Find first non-whitespace character
		
		char * cmd = request;
		while( isspace( (unsigned char) *cmd ) )
			++cmd;

		// ignore comments and empty lines

		if( '\0' == *cmd || '#' == *cmd )
			continue;

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
		
		char* req_copy = strdup(cmd);

		parse_request( req_copy ); 
		if( request && *cmd ) {
			add_history(request);
		}

		free(request);
		free(req_copy);

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

	free(request);
	free(login_session);

	osrf_system_shutdown();
	return 0;
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


static int parse_error( char* words[] ) {

	if( ! words )
		return 0;

	growing_buffer * gbuf = buffer_init( 64 );
	buffer_add( gbuf, *words );
	while( *++words ) {
		buffer_add( gbuf, " " );
		buffer_add( gbuf, *words );
	}
	fprintf( stderr, "???: %s\n", gbuf->buf );
	buffer_free( gbuf );
	
	return 0;

}


static int parse_request( char* request ) {

	if( request == NULL )
		return 0;

	char* original_request = strdup( request );
	char* words[COMMAND_BUFSIZE]; 
	
	int ret_val = 0;
	int i = 0;


	char* req = request;
	char* cur_tok = strtok( req, " " );

	if( cur_tok == NULL )
	{
		free( original_request );
		return 0;
	}

	/* Load an array with pointers to    */
	/* the tokens as defined by strtok() */
	
	while(cur_tok != NULL) {
		if( i < COMMAND_BUFSIZE - 1 ) {
			words[i++] = cur_tok;
			cur_tok = strtok( NULL, " " );
		} else {
			fprintf( stderr, "Too many tokens in command\n" );
			free( original_request );
			return 1;
		}
	}

	words[i] = NULL;
	
	/* pass off to the top level command */
	if( !strcmp(words[0],"router") ) 
		ret_val = handle_router( words );

	else if( !strcmp(words[0],"time") ) 
		ret_val = handle_time( words );

	else if (!strcmp(words[0],"request"))
		ret_val = handle_request( words, 0 );

	else if (!strcmp(words[0],"relay"))
		ret_val = handle_request( words, 1 );

	else if (!strcmp(words[0],"help"))
		ret_val = print_help();

	else if (!strcmp(words[0],"set"))
		ret_val = handle_set(words);

	else if (!strcmp(words[0],"print"))
		ret_val = handle_print(words);

	else if (!strcmp(words[0],"math_bench"))
		ret_val = handle_math(words);

	else if (!strcmp(words[0],"introspect"))
		ret_val = handle_introspect(words);

	else if (!strcmp(words[0],"login"))
		ret_val = handle_login(words);

	else if (words[0][0] == '!') {
		system( original_request + 1 );
		ret_val = 1;
	}
	
	free( original_request );
	
	if(!ret_val)
		return parse_error( words );
	else
		return 1;
}


static int handle_introspect(char* words[]) {

	if( ! words[1] )
		return 0;

	fprintf(stderr, "--> %s\n", words[1]);

	// Build a command in a suitably-sized
	// buffer and then parse it
	
	size_t len;
	if( words[2] ) {
		static const char text[] = "request %s opensrf.system.method %s";
		len = sizeof( text ) + strlen( words[1] ) + strlen( words[2] );
		char buf[len];
		snprintf( buf, sizeof(buf), text, words[1], words[2] );
		return parse_request( buf );

	} else {
		static const char text[] = "request %s opensrf.system.method.all";
		len = sizeof( text ) + strlen( words[1] );
		char buf[len];
		snprintf( buf, sizeof(buf), text, words[1] );
		return parse_request( buf );

	}
}


static int handle_login( char* words[]) {

	if( words[1] && words[2]) {

		char* username		= words[1];
		char* password		= words[2];
		char* type			= words[3];
		char* orgloc		= words[4];
		char* workstation	= words[5];
		int orgloci = (orgloc) ? atoi(orgloc) : 0;
		if(!type) type = "opac";

		char login_text[] = "request open-ils.auth open-ils.auth.authenticate.init \"%s\"";
		size_t len = sizeof( login_text ) + strlen(username) + 1;

		char buf[len];
		snprintf( buf, sizeof(buf), login_text, username );
		parse_request(buf);

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
		buffer_add(argbuf, "}");

		free(pass_buf);
		free(mess_buf);

		parse_request( argbuf->buf );
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

static int handle_set( char* words[]) {

	char* variable;
	if( (variable=words[1]) ) {

		char* val;
		if( (val=words[2]) ) {

			if(!strcmp(variable,"pretty_print")) {
				if(!strcmp(val,"true")) {
					pretty_print = 1;
					printf("pretty_print = true\n");
					return 1;
				} 
				if(!strcmp(val,"false")) {
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
				} 
				if(!strcmp(val,"false")) {
					raw_print = 0;
					printf("raw_print = false\n");
					return 1;
				} 
			}

		}
	}

	return 0;
}


static int handle_print( char* words[]) {

	char* variable;
	if( (variable=words[1]) ) {
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

static int handle_router( char* words[] ) {

	if(!client)
		return 1;

	int i;

	if( words[1] ) { 
		if( !strcmp(words[1],"query") ) {
			
			if( words[2] && !strcmp(words[2],"servers") ) {
				for(i=3; i < COMMAND_BUFSIZE - 3 && words[i]; i++ ) {	
					router_query_servers( words[i] );
				}
				return 1;
			}
			return 0;
		}
		return 0;
	}
	return 0;
}



static int handle_request( char* words[], int relay ) {

	if(!client)
		return 1;

	if(words[1]) {
		char* server = words[1];
		char* method = words[2];
		int i;
		growing_buffer* buffer = NULL;
		if(!relay) {
			buffer = buffer_init(128);
			buffer_add(buffer, "[");
			for(i = 3; words[i] != NULL; i++ ) {
				/* removes trailing semicolon if user accidentally enters it */
				if( words[i][strlen(words[i])-1] == ';' )
					words[i][strlen(words[i])-1] = '\0';
				buffer_add( buffer, words[i] );
				buffer_add(buffer, " ");
			}
			buffer_add(buffer, "]");
		}

		int rc = send_request( server, method, buffer, relay );
		buffer_free( buffer );
		return rc;
	} 

	return 0;
}

int send_request( char* server, 
		char* method, growing_buffer* buffer, int relay ) {
	if( server == NULL || method == NULL )
		return 0;

	jsonObject* params = NULL;
	if( !relay ) {
		if( buffer != NULL && buffer->n_used > 0 ) 
			params = jsonParseString(buffer->buf);
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

	osrfAppSession* session = osrfAppSessionClientInit(server);

	if(!osrf_app_session_connect(session)) {
		fprintf(stderr, "Unable to communicate with service %s\n", server);
		osrfLogWarning( OSRF_LOG_MARK,  "Unable to connect to remote service %s\n", server );
		jsonObjectFree(params);
		return 1;
	}

	double start = get_timestamp_millis();

	int req_id = osrfAppSessionMakeRequest( session, params, method, 1, NULL );
	jsonObjectFree(params);

	osrf_message* omsg = osrfAppSessionRequestRecv( session, req_id, recv_timeout );

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
					//content = json_printer(j); 
					content = jsonFormatString(j);
					free(j);
				} else {
					const char * temp_content = jsonObjectGetString(omsg->_result_content);
					if( ! temp_content )
						temp_content = "[null]";
					content = strdup( temp_content );
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
			}

		} else {

			if(omsg->_result_content) {
	
				osrfMessageFree(last_result);
				last_result = omsg;
	
				char* content;
	
				if( pretty_print && omsg->_result_content ) {
					char* j = jsonObjectToJSON(omsg->_result_content);
					//content = json_printer(j); 
					content = jsonFormatString(j);
					free(j);
				} else {
					const char * temp_content = jsonObjectGetString(omsg->_result_content);
					if( temp_content )
						content = strdup( temp_content );
					else
						content = NULL;
				}

				buffer_add( resp_buffer, "\nReceived Data: " ); 
				buffer_add( resp_buffer, content );
				buffer_add( resp_buffer, "\n" );
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
	osrf_app_session_disconnect( session );
	osrfAppSessionFree( session );


	return 1;


}

static int handle_time( char* words[] ) {
	if(!words[1]) {
		printf("%f\n", get_timestamp_millis());
    } else {
        time_t epoch = (time_t) atoi(words[1]);
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
			"Commands:\n"
			"---------------------------------------------------------------------------------\n"
			"help                   - Display this message\n"
			"!<command> [args]      - Forks and runs the given command in the shell\n"
		/*
			"time			- Prints the current time\n"
			"time <timestamp>	- Formats seconds since epoch into readable format\n"
		*/
			"set <variable> <value> - set a srfsh variable (e.g. set pretty_print true )\n"
			"print <variable>       - Displays the value of a srfsh variable\n"
			"---------------------------------------------------------------------------------\n"

			"router query servers <server1 [, server2, ...]>\n"
			"       - Returns stats on connected services\n"
			"\n"
			"\n"
			"request <service> <method> [ <json formatted string of params> ]\n"
			"       - Anything passed in will be wrapped in a json array,\n"
			"               so add commas if there is more than one param\n"
			"\n"
			"\n"
			"relay <service> <method>\n"
			"       - Performs the requested query using the last received result as the param\n"
			"\n"
			"\n"
			"math_bench <num_batches> [0|1|2]\n"
			"       - 0 means don't reconnect, 1 means reconnect after each batch of 4, and\n"
			"                2 means reconnect after every request\n"
			"\n"
			"introspect <service>\n"
			"       - prints the API for the service\n"
			"\n"
			"\n"
			"---------------------------------------------------------------------------------\n"
			" Commands for Open-ILS\n"
			"---------------------------------------------------------------------------------\n"
			"login <username> <password> [type] [org_unit] [workstation]\n"
			"       - Logs into the 'server' and displays the session id\n"
			"       - To view the session id later, enter: print login\n"
			"---------------------------------------------------------------------------------\n"
			"\n"
			"\n"
			"Note: long output is piped through 'less'. To search in 'less', type: /<search>\n"
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


static int handle_math( char* words[] ) {
	if( words[1] )
		return do_math( atoi(words[1]), 0 );
	return 0;
}


static int do_math( int count, int style ) {

	osrfAppSession* session = osrfAppSessionClientInit( "opensrf.math" );
	osrf_app_session_connect(session);

	jsonObject* params = jsonParseString("[]");
	jsonObjectPush(params,jsonNewObject("1"));
	jsonObjectPush(params,jsonNewObject("2"));

	char* methods[] = { "add", "sub", "mult", "div" };
	char* answers[] = { "3", "-1", "2", "0.5" };

	float times[ count * 4 ];
	memset(times, 0, sizeof(times));

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
			int req_id = osrfAppSessionMakeRequest( session, params, methods[j], 1, NULL );
			osrf_message* omsg = osrfAppSessionRequestRecv( session, req_id, 5 );
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
