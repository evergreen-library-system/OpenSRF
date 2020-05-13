/**
	@file osrf_prefork.c
	@brief Spawn and manage a collection of child process to service requests.

	Spawn a collection of child processes, replacing them as needed.  Forward requests to them
	and let the children do the work.

	Each child processes some maximum number of requests before it terminates itself.  When a
	child dies, either deliberately or otherwise, we can spawn another one to replace it,
	keeping the number of children within a predefined range.

	Use a doubly-linked circular list to keep track of the children to whom we have forwarded
	a request, and who are still working on them.  Use a separate linear linked list to keep
	track of children that are currently idle.  Move them back and forth as needed.

	For each child, set up two pipes:
	- One for the parent to send requests to the child.
	- One for the child to notify the parent that it is available for another request.

	The message sent to the child represents an XML stanza as received from Jabber.

	When the child finishes processing the request, it writes the string "available" back
	to the parent.  Then the parent knows that it can send that child another request.
*/

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>

#include "opensrf/utils.h"
#include "opensrf/log.h"
#include "opensrf/transport_client.h"
#include "opensrf/osrf_stack.h"
#include "opensrf/osrf_settings.h"
#include "opensrf/osrf_application.h"

#define READ_BUFSIZE 1024
#define ABS_MAX_CHILDREN 256

typedef struct {
	int max_requests;     /**< How many requests a child processes before terminating. */
	int min_children;     /**< Minimum number of children to maintain. */
	int max_children;     /**< Maximum number of children to maintain. */
	int max_backlog_queue; /**< Maximum size of backlog queue. */
	int fd;               /**< Unused. */
	int data_to_child;    /**< Unused. */
	int data_to_parent;   /**< Unused. */
	int current_num_children;   /**< How many children are currently on the list. */
	int keepalive;        /**< Keepalive time for stateful sessions. */
	char* appname;        /**< Name of the application. */
	/** Points to a circular linked list of children. */
	struct prefork_child_struct* first_child;
	/** List of of child processes that aren't doing anything at the moment and are
		therefore available to service a new request. */
	struct prefork_child_struct* idle_list;
	/** List of allocated but unused prefork_children, available for reuse.  Each one is just
		raw memory, apart from the "next" pointer used to stitch them together.  In particular,
		there is no child process for them, and the file descriptors are not open. */
	struct prefork_child_struct* free_list;
    struct prefork_child_struct* sighup_pending_list;
	transport_client* connection;  /**< Connection to Jabber. */
} prefork_simple;

struct prefork_child_struct {
	pid_t pid;            /**< Process ID of the child. */
	int read_data_fd;     /**< Child uses to read request. */
	int write_data_fd;    /**< Parent uses to write request. */
	int read_status_fd;   /**< Parent reads to see if child is available. */
	int write_status_fd;  /**< Child uses to notify parent when it's available again. */
	int max_requests;     /**< How many requests a child can process before terminating. */
	const char* appname;  /**< Name of the application. */
	int keepalive;        /**< Keepalive time for stateful sessions. */
	struct prefork_child_struct* next;  /**< Linkage pointer for linked list. */
	struct prefork_child_struct* prev;  /**< Linkage pointer for linked list. */
};

typedef struct prefork_child_struct prefork_child;

/** Boolean.  Set to true by a signal handler when it traps SIGCHLD. */
static volatile sig_atomic_t child_dead;

static int prefork_simple_init( prefork_simple* prefork, transport_client* client,
	int max_requests, int min_children, int max_children, int max_backlog_queue );
static prefork_child* launch_child( prefork_simple* forker );
static void prefork_launch_children( prefork_simple* forker );
static void prefork_run( prefork_simple* forker );
static void add_prefork_child( prefork_simple* forker, prefork_child* child );

static void del_prefork_child( prefork_simple* forker, pid_t pid );
static int check_children( prefork_simple* forker, int forever );
static int  prefork_child_process_request( prefork_child*, char* data );
static int prefork_child_init_hook( prefork_child* );
static prefork_child* prefork_child_init( prefork_simple* forker,
	int read_data_fd, int write_data_fd,
	int read_status_fd, int write_status_fd );

/* listens on the 'data_to_child' fd and wait for incoming data */
static void prefork_child_wait( prefork_child* child );
static void prefork_clear( prefork_simple*, bool graceful);
static void prefork_child_free( prefork_simple* forker, prefork_child* );
static void osrf_prefork_register_routers( const char* appname, bool unregister );
static void osrf_prefork_child_exit( prefork_child* );

static void sigchld_handler( int sig );
static void sigusr1_handler( int sig );
static void sigusr2_handler( int sig );
static void sigterm_handler( int sig );
static void sigint_handler( int sig );
static void sighup_handler( int sig );

/** Maintain a global pointer to the prefork_simple object
 *  for the current process so we can refer to it later
 *  for signal handling.  There will only ever be one
 *  forker per process.
 */
static prefork_simple *global_forker = NULL;

/**
	@brief Spawn and manage a collection of drone processes for servicing requests.
	@param appname Name of the application.
	@return 0 if successful, or -1 if error.
*/
int osrf_prefork_run( const char* appname ) {

	if( !appname ) {
		osrfLogError( OSRF_LOG_MARK, "osrf_prefork_run requires an appname to run!");
		return -1;
	}

	set_proc_title( "OpenSRF Listener [%s]", appname );

	int maxr = 1000;
	int maxc = 10;
	int maxbq = 1000;
	int minc = 3;
	int kalive = 5;

	// Get configuration settings
	osrfLogInfo( OSRF_LOG_MARK, "Loading config in osrf_forker for app %s", appname );

	char* max_req      = osrf_settings_host_value( "/apps/%s/unix_config/max_requests", appname );
	char* min_children = osrf_settings_host_value( "/apps/%s/unix_config/min_children", appname );
	char* max_children = osrf_settings_host_value( "/apps/%s/unix_config/max_children", appname );
	char* max_backlog_queue = osrf_settings_host_value( "/apps/%s/unix_config/max_backlog_queue", appname );
	char* keepalive    = osrf_settings_host_value( "/apps/%s/keepalive", appname );

	if( !keepalive )
		osrfLogWarning( OSRF_LOG_MARK, "Keepalive is not defined, assuming %d", kalive );
	else
		kalive = atoi( keepalive );

	if( !max_req )
		osrfLogWarning( OSRF_LOG_MARK, "Max requests not defined, assuming %d", maxr );
	else
		maxr = atoi( max_req );

	if( !min_children )
		osrfLogWarning( OSRF_LOG_MARK, "Min children not defined, assuming %d", minc );
	else
		minc = atoi( min_children );

	if( !max_children )
		osrfLogWarning( OSRF_LOG_MARK, "Max children not defined, assuming %d", maxc );
	else
		maxc = atoi( max_children );

	if( !max_backlog_queue )
		osrfLogWarning( OSRF_LOG_MARK, "Max backlog queue size not defined, assuming %d", maxbq );
	else
		maxbq = atoi( max_backlog_queue );

	free( keepalive );
	free( max_req );
	free( min_children );
	free( max_children );
	free( max_backlog_queue );
	/* --------------------------------------------------- */

	char* resc = va_list_to_string( "%s_listener", appname );

	// Make sure that we haven't already booted
	if( !osrfSystemBootstrapClientResc( NULL, NULL, resc )) {
		osrfLogError( OSRF_LOG_MARK, "Unable to bootstrap client for osrf_prefork_run()" );
		free( resc );
		return -1;
	}

	free( resc );

	prefork_simple forker;

	if( prefork_simple_init( &forker, osrfSystemGetTransportClient(), maxr, minc, maxc, maxbq )) {
		osrfLogError( OSRF_LOG_MARK,
			"osrf_prefork_run() failed to create prefork_simple object" );
		return -1;
	}

	// Finish initializing the prefork_simple.
	forker.appname   = strdup( appname );
	forker.keepalive = kalive;
	global_forker = &forker;

	// Spawn the children; put them in the idle list.
	prefork_launch_children( &forker );

	// Tell the router that you're open for business.
	osrf_prefork_register_routers( appname, false );

	signal( SIGUSR1, sigusr1_handler);
	signal( SIGUSR2, sigusr2_handler);
	signal( SIGTERM, sigterm_handler);
	signal( SIGINT,  sigint_handler );
	signal( SIGQUIT, sigint_handler );
	signal( SIGHUP,  sighup_handler );

	// Sit back and let the requests roll in
	osrfLogInfo( OSRF_LOG_MARK, "Launching osrf_forker for app %s", appname );
	prefork_run( &forker );

	osrfLogWarning( OSRF_LOG_MARK, "prefork_run() returned - how??" );
	prefork_clear( &forker, false );
	return 0;
}

/**
	@brief Register the application with a specified router.
	@param appname Name of the application.
	@param routerName Name of the router.
	@param routerDomain Domain of the router.

	Tell the router that you're open for business so that it can route requests to you.

	Called only by the parent process.
*/
static void osrf_prefork_send_router_registration(
		const char* appname, const char* routerName, 
            const char* routerDomain, bool unregister ) {

	// Get a pointer to the global transport_client
	transport_client* client = osrfSystemGetTransportClient();

	// Construct the Jabber address of the router
	char* jid = va_list_to_string( "%s@%s/router", routerName, routerDomain );

	// Create the registration message, and send it
	transport_message* msg;
    if (unregister) {

	    osrfLogInfo( OSRF_LOG_MARK, "%s un-registering with router %s", appname, jid );
	    msg = message_init( "unregistering", NULL, NULL, jid, NULL );
	    message_set_router_info( msg, NULL, NULL, appname, "unregister", 0 );

    } else {

	    osrfLogInfo( OSRF_LOG_MARK, "%s registering with router %s", appname, jid );
	    msg = message_init( "registering", NULL, NULL, jid, NULL );
	    message_set_router_info( msg, NULL, NULL, appname, "register", 0 );
    }

	client_send_message( client, msg );

	// Clean up
	message_free( msg );
	free( jid );
}

/**
	@brief Register with a router, or not, according to some config settings.
	@param appname Name of the application
	@param RouterChunk A representation of part of the config file.

	Parse a "complex" router configuration chunk.

	Examine the services listed for a given router (normally in opensrf_core.xml).  If
	there is an entry for this service, or if there are @em no services listed, then
	register with this router.  Otherwise don't.

	Called only by the parent process.
*/
static void osrf_prefork_parse_router_chunk( 
    const char* appname, const jsonObject* routerChunk, bool unregister ) {

	const char* routerName = jsonObjectGetString( jsonObjectGetKeyConst( routerChunk, "name" ));
	const char* domain = jsonObjectGetString( jsonObjectGetKeyConst( routerChunk, "domain" ));
	const jsonObject* services = jsonObjectGetKeyConst( routerChunk, "services" );
	osrfLogDebug( OSRF_LOG_MARK, "found router config with domain %s and name %s",
		routerName, domain );

	if( services && services->type == JSON_HASH ) {
		osrfLogDebug( OSRF_LOG_MARK, "investigating router information..." );
		const jsonObject* service_obj = jsonObjectGetKeyConst( services, "service" );
		if( !service_obj )
			;    // do nothing (shouldn't happen)
		else if( JSON_ARRAY == service_obj->type ) {
			// There are multiple services listed.  Register with this router
			// if and only if this service is on the list.
			int j;
			for( j = 0; j < service_obj->size; j++ ) {
				const char* service = jsonObjectGetString( jsonObjectGetIndex( service_obj, j ));
				if( service && !strcmp( appname, service ))
					osrf_prefork_send_router_registration( appname, routerName, domain, unregister );
			}
		}
		else if( JSON_STRING == service_obj->type ) {
			// There's only one service listed.  Register with this router
			// if and only if this service is the one listed.
			if( !strcmp( appname, jsonObjectGetString( service_obj )) )
				osrf_prefork_send_router_registration( appname, routerName, domain, unregister );
		}
	} else {
		// This router is not restricted to any set of services,
		// so go ahead and register with it.
		osrf_prefork_send_router_registration( appname, routerName, domain, unregister );
	}
}

/**
	@brief Register the application with one or more routers, according to the configuration.
	@param appname Name of the application.

	Called only by the parent process.
*/
static void osrf_prefork_register_routers( const char* appname, bool unregister ) {

	jsonObject* routerInfo = osrfConfigGetValueObject( NULL, "/routers/router" );

	int i;
	for( i = 0; i < routerInfo->size; i++ ) {
		const jsonObject* routerChunk = jsonObjectGetIndex( routerInfo, i );

		if( routerChunk->type == JSON_STRING ) {
			/* this accomodates simple router configs */
			char* routerName = osrfConfigGetValue( NULL, "/router_name" );
			char* domain = osrfConfigGetValue( NULL, "/routers/router" );
			osrfLogDebug( OSRF_LOG_MARK, "found simple router settings with router name %s",
				routerName );
			osrf_prefork_send_router_registration( appname, routerName, domain, unregister );

			free( routerName );
			free( domain );
		} else {
			osrf_prefork_parse_router_chunk( appname, routerChunk, unregister );
		}
	}

	jsonObjectFree( routerInfo );
}

/**
	@brief Initialize a child process.
	@param child Pointer to the prefork_child representing the new child process.
	@return Zero if successful, or -1 if not.

	Called only by child processes.  Actions:
	- Connect to one or more cache servers
	- Reconfigure logger, if necessary
	- Discard parent's Jabber connection and open a new one
	- Dynamically call an application-specific initialization routine
	- Change the command line as reported by ps
*/
static int prefork_child_init_hook( prefork_child* child ) {

	if( !child ) return -1;
	osrfLogDebug( OSRF_LOG_MARK, "Child init hook for child %d", child->pid );

	// Connect to cache server(s).
	osrfSystemInitCache();
	char* resc = va_list_to_string( "%s_drone", child->appname );

	// If we're a source-client, tell the logger now that we're a new process.
	char* isclient = osrfConfigGetValue( NULL, "/client" );
	if( isclient && !strcasecmp( isclient,"true" ))
		osrfLogSetIsClient( 1 );
	free( isclient );

	// Remove traces of our parent's socket connection so we can have our own.
    // TODO: not necessary if parent disconnects first
	osrfSystemIgnoreTransportClient();

	// Connect to Jabber
	if( !osrfSystemBootstrapClientResc( NULL, NULL, resc )) {
		osrfLogError( OSRF_LOG_MARK, "Unable to bootstrap client for osrf_prefork_run()" );
		free( resc );
		return -1;
	}

	free( resc );

	// Dynamically call the application-specific initialization function
	// from a previously loaded shared library.
	if( ! osrfAppRunChildInit( child->appname )) {
		osrfLogDebug( OSRF_LOG_MARK, "Prefork child_init succeeded\n" );
	} else {
		osrfLogError( OSRF_LOG_MARK, "Prefork child_init failed\n" );
		return -1;
	}

	// Change the command line as reported by ps
	set_proc_title( "OpenSRF Drone [%s]", child->appname );
	return 0;
}

/**
	@brief Respond to a client request forwarded by the parent.
	@param child Pointer to the state of the child process.
	@param data Pointer to the raw XMPP message received from the parent.
	@return 0 on success; non-zero means that the child process should clean itself up
		and terminate immediately, presumably due to a fatal error condition.

	Called only by a child process.
*/
static int prefork_child_process_request( prefork_child* child, char* data ) {
	if( !child ) return 0;

	transport_client* client = osrfSystemGetTransportClient();

	// Make sure that we're still connected to Jabber; reconnect if necessary.
	if( !client_connected( client )) {
		osrfSystemIgnoreTransportClient();
		osrfLogWarning( OSRF_LOG_MARK, "Reconnecting child to opensrf after disconnect..." );
		if( !osrf_system_bootstrap_client( NULL, NULL )) {
			osrfLogError( OSRF_LOG_MARK,
				"Unable to bootstrap client in prefork_child_process_request()" );
			sleep( 1 );
			osrf_prefork_child_exit( child );
		}
	}

	// Construct the message from the xml.
	transport_message* msg = new_message_from_xml( data );

	// Respond to the transport message.  This is where method calls are buried.
	osrfAppSession* session = osrf_stack_transport_handler( msg, child->appname );
	if( !session )
		return 0;

	int rc = session->panic;

	if( rc ) {
		osrfLogWarning( OSRF_LOG_MARK,
			"Drone for session %s terminating immediately", session->session_id );
		osrfAppSessionFree( session );
		return rc;
	}

	if( session->stateless && session->state != OSRF_SESSION_CONNECTED ) {
		// We're no longer connected to the client, which presumably means that
		// we're done with this request.  Bail out.
		osrfAppSessionFree( session );
		return rc;
	}

	// If we get this far, then the client has opened an application connection so that it
	// can send multiple requests directly to the same server drone, bypassing the router
	// and the listener.  For example, it may need to do a database transaction, requiring
	// multiple method calls within the same database session.

	// Hence we go into a loop, responding to successive requests from the same client, until
	// either the client disconnects or an error occurs.

	osrfLogDebug( OSRF_LOG_MARK, "Entering keepalive loop for session %s", session->session_id );
	int keepalive = child->keepalive;
	int retval;
	int recvd;
	time_t start;
	time_t end;

	while( 1 ) {

		// Respond to any input messages.  This is where the method calls are buried.
		osrfLogDebug( OSRF_LOG_MARK,
			"osrf_prefork calling queue_wait [%d] in keepalive loop", keepalive );
		start   = time( NULL );
		retval  = osrf_app_session_queue_wait( session, keepalive, &recvd );
		end     = time( NULL );

		osrfLogDebug( OSRF_LOG_MARK, "Data received == %d", recvd );

		// Now we check a number of possible reasons to exit the loop.

		// If the method call decided to terminate immediately,
		// note that for future reference.
		if( session->panic )
			rc = 1;

		// If an error occurred when we tried to service the request, exit the loop.
		if( retval ) {
			osrfLogError( OSRF_LOG_MARK, "queue-wait returned non-success %d", retval );
			break;
		}

		// If the client disconnected, exit the loop.
		if( session->state != OSRF_SESSION_CONNECTED )
			break;

		// If we timed out while waiting for a request, exit the loop.
		if( !recvd && (end - start) >= keepalive ) {
			osrfLogInfo( OSRF_LOG_MARK,
				"No request was received in %d seconds, exiting stateful session", keepalive );
			osrfAppSessionStatus(
				session,
				OSRF_STATUS_TIMEOUT,
				"osrfConnectStatus",
				0, "Disconnected on timeout" );

			break;
		}

		// If the child process has decided to terminate immediately, exit the loop.
		if( rc )
			break;
	}

	osrfLogDebug( OSRF_LOG_MARK, "Exiting keepalive loop for session %s", session->session_id );
	osrfAppSessionFree( session );
	return rc;
}

/**
	@brief Partially initialize a prefork_simple provided by the caller.
	@param prefork Pointer to a a raw prefork_simple to be initialized.
	@param client Pointer to a transport_client (connection to Jabber).
	@param max_requests The maximum number of requests that a child process may service
			before terminating.
	@param min_children Minimum number of child processes to maintain.
	@param max_children Maximum number of child processes to maintain.
	@param max_backlog_queue Maximum size of backlog queue.
	@return 0 if successful, or 1 if not (due to invalid parameters).
*/
static int prefork_simple_init( prefork_simple* prefork, transport_client* client,
		int max_requests, int min_children, int max_children, int max_backlog_queue ) {

	if( min_children > max_children ) {
		osrfLogError( OSRF_LOG_MARK,  "min_children (%d) is greater "
			"than max_children (%d)", min_children, max_children );
		return 1;
	}

	if( max_children > ABS_MAX_CHILDREN ) {
		osrfLogError( OSRF_LOG_MARK,  "max_children (%d) is greater than ABS_MAX_CHILDREN (%d)",
			max_children, ABS_MAX_CHILDREN );
		return 1;
	}

	osrfLogInfo( OSRF_LOG_MARK, "Prefork launching child with max_request=%d,"
		"min_children=%d, max_children=%d", max_requests, min_children, max_children );

	/* flesh out the struct */
	prefork->max_requests = max_requests;
	prefork->min_children = min_children;
	prefork->max_children = max_children;
	prefork->max_backlog_queue = max_backlog_queue;
	prefork->fd           = 0;
	prefork->data_to_child = 0;
	prefork->data_to_parent = 0;
	prefork->current_num_children = 0;
	prefork->keepalive    = 0;
	prefork->appname      = NULL;
	prefork->first_child  = NULL;
	prefork->idle_list    = NULL;
	prefork->free_list    = NULL;
	prefork->connection   = client;
	prefork->sighup_pending_list = NULL;

	return 0;
}

/**
	@brief Spawn a new child process and put it in the idle list.
	@param forker Pointer to the prefork_simple that will own the process.
	@return Pointer to the new prefork_child, or not at all.

	Spawn a new child process.  Create a prefork_child for it and put it in the idle list.

	After forking, the parent returns a pointer to the new prefork_child.  The child
	services its quota of requests and then terminates without returning.
*/
static prefork_child* launch_child( prefork_simple* forker ) {

	pid_t pid;
	int data_fd[2];
	int status_fd[2];

	// Set up the data and status pipes
	if( pipe( data_fd ) < 0 ) { /* build the data pipe*/
		osrfLogError( OSRF_LOG_MARK,  "Pipe making error" );
		return NULL;
	}

	if( pipe( status_fd ) < 0 ) {/* build the status pipe */
		osrfLogError( OSRF_LOG_MARK,  "Pipe making error" );
		close( data_fd[1] );
		close( data_fd[0] );
		return NULL;
	}

	osrfLogInternal( OSRF_LOG_MARK, "Pipes: %d %d %d %d",
		data_fd[0], data_fd[1], status_fd[0], status_fd[1] );

	// Create and initialize a prefork_child for the new process
	prefork_child* child = prefork_child_init( forker, data_fd[0],
		data_fd[1], status_fd[0], status_fd[1] );

	if( (pid=fork()) < 0 ) {
		osrfLogError( OSRF_LOG_MARK, "Forking Error" );
		prefork_child_free( forker, child );
		return NULL;
	}

	// Add the new child to the head of the idle list
	child->next = forker->idle_list;
	forker->idle_list = child;

	if( pid > 0 ) {  /* parent */

		signal( SIGCHLD, sigchld_handler );
		( forker->current_num_children )++;
		child->pid = pid;

		osrfLogDebug( OSRF_LOG_MARK, "Parent launched %d", pid );
		/* *no* child pipe FD's can be closed or the parent will re-use fd's that
			the children are currently using */
		return child;
	}

	else { /* child */

		// we don't want to adopt our parent's handlers.
		signal( SIGUSR1, SIG_DFL );
		signal( SIGUSR2, SIG_DFL );
		signal( SIGTERM, SIG_DFL );
		signal( SIGINT,  SIG_DFL );
		signal( SIGQUIT, SIG_DFL );
		signal( SIGCHLD, SIG_DFL );
		signal( SIGHUP,  SIG_DFL );

		osrfLogInternal( OSRF_LOG_MARK,
			"I am new child with read_data_fd = %d and write_status_fd = %d",
			child->read_data_fd, child->write_status_fd );

		child->pid = getpid();
		close( child->write_data_fd );
		close( child->read_status_fd );

		/* do the initing */
		if( prefork_child_init_hook( child ) == -1 ) {
			osrfLogError( OSRF_LOG_MARK,
				"Forker child going away because we could not connect to OpenSRF..." );
			osrf_prefork_child_exit( child );
		}

		prefork_child_wait( child );      // Should exit without returning
		osrf_prefork_child_exit( child ); // Just to be sure
		return NULL;  // Unreachable, but it keeps the compiler happy
	}
}

/**
	@brief Terminate a child process.
	@param child Pointer to the prefork_child representing the child process (not used).

	Called only by child processes.  Dynamically call an application-specific shutdown
	function from a previously loaded shared library; then exit.
*/
static void osrf_prefork_child_exit( prefork_child* child ) {
	osrfAppRunExitCode();
	exit( 0 );
}

/**
	@brief Launch all the child processes, putting them in the idle list.
	@param forker Pointer to the prefork_simple that will own the children.

	Called only by the parent process (in order to become a parent).
*/
static void prefork_launch_children( prefork_simple* forker ) {
	if( !forker ) return;
	int c = 0;
	while( c++ < forker->min_children )
		launch_child( forker );
}

/**
	@brief Signal handler for SIGCHLD: note that a child process has terminated.
	@param sig The value of the trapped signal; always SIGCHLD.

	Set a boolean to be checked later.
*/
static void sigchld_handler( int sig ) {
	signal( SIGCHLD, sigchld_handler );
	child_dead = 1;
}

/**
	@brief Signal handler for SIGUSR1
	@param sig The value of the trapped signal; always SIGUSR1.

	Send unregister command to all registered routers.
*/
static void sigusr1_handler( int sig ) {
	if (!global_forker) return;
	osrf_prefork_register_routers(global_forker->appname, true);
	signal( SIGUSR1, sigusr1_handler );
}

/**
	@brief Signal handler for SIGUSR2
	@param sig The value of the trapped signal; always SIGUSR2.

	Send register command to all known routers
*/
static void sigusr2_handler( int sig ) {
	if (!global_forker) return;
	osrf_prefork_register_routers(global_forker->appname, false);
	signal( SIGUSR2, sigusr2_handler );
}

/**
	@brief Signal handler for SIGTERM
	@param sig The value of the trapped signal; always SIGTERM

	Perform a graceful prefork server shutdown.
*/
static void sigterm_handler(int sig) {
	if (!global_forker) return;
	osrfLogInfo(OSRF_LOG_MARK, "server: received SIGTERM, shutting down");
	prefork_clear(global_forker, true);
	_exit(0);
}

/**
	@brief Signal handler for SIGINT or SIGQUIT
	@param sig The value of the trapped signal

	Perform a non-graceful prefork server shutdown.
*/
static void sigint_handler(int sig) {
	if (!global_forker) return;
	osrfLogInfo(OSRF_LOG_MARK, "server: received SIGINT/QUIT, shutting down");
	prefork_clear(global_forker, false);
	_exit(0);
}

static void sighup_handler(int sig) {
    if (!global_forker) return;
    osrfLogInfo(OSRF_LOG_MARK, "server: received SIGHUP, reloading config");

    osrfConfig* oldConfig = osrfConfigGetDefaultConfig();
    osrfConfig* newConfig = osrfConfigInit(
        oldConfig->configFileName, oldConfig->configContext);

    if (!newConfig) {
        osrfLogError(OSRF_LOG_MARK, "Config reload failed");
        return;
    }

    // frees oldConfig
    osrfConfigSetDefaultConfig(newConfig); 
    
    // apply the log level from the reloaded file
    char* log_level = osrfConfigGetValue(NULL, "/loglevel");
    if(log_level) {
        int level = atoi(log_level);
        osrfLogSetLevel(level);
    }

    // Copy the list of active children into the sighup_pending list.
    // Cloning is necessary, since the nodes in the active list, particularly
    // their next/prev pointers, will start changing once we exit this func.
    // sighup_pending_list is a non-circular, singly linked list.
    prefork_child* cur_child = global_forker->first_child;
    prefork_child* clone;

    // the first_pid lets us know when we've made a full circle of the active 
    // children
    pid_t first_pid = 0;
    while (cur_child && cur_child->pid != first_pid) {

        if (!first_pid) first_pid = cur_child->pid;

        // all we need to keep track of is the pid
        clone = safe_malloc(sizeof(prefork_child));
        clone->pid = cur_child->pid;
        clone->next = NULL;

        osrfLogDebug(OSRF_LOG_MARK, 
            "Adding child %d to sighup pending list", clone->pid);

        // add the clone to the front of the list
        if (global_forker->sighup_pending_list) 
            clone->next = global_forker->sighup_pending_list;
        global_forker->sighup_pending_list = clone;

        cur_child = cur_child->next;
    }

    // Kill all idle children.
    // Let them get cleaned up through the normal response-handling cycle
    cur_child = global_forker->idle_list;
    while (cur_child) {
        osrfLogDebug(OSRF_LOG_MARK, "Killing child in SIGHUP %d", cur_child->pid);
        kill(cur_child->pid, SIGKILL);
        cur_child = cur_child->next;
    }
}
    

/**
	@brief Replenish the collection of child processes, after one has terminated.
	@param forker Pointer to the prefork_simple that manages the child processes.

	The parent calls this function when it notices (via a signal handler) that
	a child process has died.

	Wait on the dead children so that they won't be zombies.  Spawn new ones as needed
	to maintain at least a minimum number.
*/
static void reap_children( prefork_simple* forker ) {

	pid_t child_pid;

	// Reset our boolean so that we can detect any further terminations.
	child_dead = 0;

	// Bury the children so that they won't be zombies.  WNOHANG means that waitpid() returns
	// immediately if there are no waitable children, instead of waiting for more to die.
	// Ignore the return code of the child.  We don't do an autopsy.
	while( (child_pid = waitpid( -1, NULL, WNOHANG )) > 0 ) {
		--forker->current_num_children;
		del_prefork_child( forker, child_pid );
	}

	// Spawn more children as needed.
	while( forker->current_num_children < forker->min_children )
		launch_child( forker );
}

/**
	@brief Read transport_messages and dispatch them to child processes for servicing.
	@param forker Pointer to the prefork_simple that manages the child processes.

	This is the main loop of the parent process, and once entered, does not exit.

	For each usable transport_message received: look for an idle child to service it.  If
	no idle children are available, either spawn a new one or, if we've already spawned the
	maximum number of children, wait for one to become available.  Once a child is available
	by whatever means, write an XML version of the input message, to a pipe designated for
	use by that child.
*/
static void prefork_run( prefork_simple* forker ) {

	if( NULL == forker->idle_list )
		return;   // No available children, and we haven't even started yet

	transport_message* cur_msg = NULL;

	// The backlog queue accumulates messages received while there
	// are not yet children available to process them. While the
	// transport client maintains its own queue of messages, sweeping
	// the transport client's queue in the backlog queue gives us the
	// ability to set a limit on the size of the backlog queue (and
	// then to drop messages once the backlog queue has filled up)
	transport_message* backlog_queue_head = NULL;
	transport_message* backlog_queue_tail = NULL;
	int backlog_queue_size = 0;

	while( 1 ) {

		if( forker->first_child == NULL && forker->idle_list == NULL ) {/* no more children */
			osrfLogWarning( OSRF_LOG_MARK, "No more children..." );
			return;
		}

		int received_from_network = 0;
		if ( backlog_queue_size == 0 ) {
			// Wait indefinitely for an input message
			osrfLogDebug( OSRF_LOG_MARK, "Forker going into wait for data..." );
			cur_msg = client_recv( forker->connection, -1 );
			received_from_network = 1;
		} else {
			// We have queued messages, which means all of our drones
			// are occupied.  See if any new messages are available on the
			// network while waiting up to 1 second to allow time for a drone
			// to become available to handle the next request in the queue.
			cur_msg = client_recv( forker->connection, 1 );
			if ( cur_msg != NULL )
				received_from_network = 1;
		}

		if (received_from_network) {
			if( cur_msg == NULL ) {
				// most likely a signal was received.  clean up any recently
				// deceased children and try again.
				if(child_dead)
					reap_children(forker);
				continue;
			}

			if (cur_msg->error_type) {
				osrfLogInfo(OSRF_LOG_MARK,
					"Listener received an XMPP error message.  "
					"Likely a bounced message. sender=%s", cur_msg->sender);
				if(child_dead)
					reap_children(forker);
				continue;
			}

			message_prepare_xml( cur_msg );
			const char* msg_data = cur_msg->msg_xml;
			if( ! msg_data || ! *msg_data ) {
				osrfLogWarning( OSRF_LOG_MARK, "Received % message from %s, thread %",
					(msg_data ? "empty" : "NULL"), cur_msg->sender, cur_msg->thread );
				message_free( cur_msg );
				continue;       // Message not usable; go on to the next one.
			}

			// stick message onto queue
			cur_msg->next = NULL;
			if (backlog_queue_size == 0) {
				backlog_queue_head = cur_msg;
				backlog_queue_tail = cur_msg;
			} else {
				if (backlog_queue_size >= forker->max_backlog_queue) {
					osrfLogWarning ( OSRF_LOG_MARK, "Reached backlog queue limit of %d; dropping "
						"latest message",
						forker->max_backlog_queue );
					osrfMessage* err = osrf_message_init( STATUS, 1, 1 );
					osrf_message_set_status_info( err, "osrfMethodException",
						"Service unavailable: no available children and backlog queue at limit",
						OSRF_STATUS_SERVICEUNAVAILABLE );
					char *data = osrf_message_serialize( err );
					osrfMessageFree( err );
					transport_message* tresponse = message_init( data, "", cur_msg->thread, cur_msg->router_from, cur_msg->recipient );
					message_set_osrf_xid(tresponse, cur_msg->osrf_xid);
					free( data );
					transport_client* client = osrfSystemGetTransportClient();
					client_send_message( client, tresponse );
					message_free( tresponse );
					message_free(cur_msg);
					continue;
				}
				backlog_queue_tail->next = cur_msg;
				backlog_queue_tail = cur_msg;
				osrfLogWarning( OSRF_LOG_MARK, "Adding message to non-empty backlog queue." );
			}
			backlog_queue_size++;
		}

		if (backlog_queue_size == 0) {
			// strictly speaking, this check may be redundant, but
			// from this point forward we can be sure that the
			// backlog queue has at least one message in it and
			// that if we can find a child to process it, we want to
			// process the head of that queue.
			continue;
		}

		cur_msg = backlog_queue_head;

		int honored = 0;     /* will be set to true when we service the request */
		int no_recheck = 0;

		while( ! honored ) {

			if( !no_recheck ) {
				if(check_children( forker, 0 ) < 0) {
                    continue; // check failed, try again
				}
            }
            no_recheck = 0;

			osrfLogDebug( OSRF_LOG_MARK, "Server received inbound data" );

			prefork_child* cur_child = NULL;

			// Look for an available child in the idle list.  Since the idle list operates
			// as a stack, the child we get is the one that was most recently active, or
			// most recently spawned.  That means it's the one most likely still to be in
			// physical memory, and the one least likely to have to be swapped in.
			while( forker->idle_list ) {

				osrfLogDebug( OSRF_LOG_MARK, "Looking for idle child" );
				// Grab the prefork_child at the head of the idle list
				cur_child = forker->idle_list;
				forker->idle_list = cur_child->next;
				cur_child->next = NULL;

				osrfLogInternal( OSRF_LOG_MARK,
					"Searching for available child. cur_child->pid = %d", cur_child->pid );
				osrfLogInternal( OSRF_LOG_MARK, "Current num children %d",
					forker->current_num_children );

				osrfLogDebug( OSRF_LOG_MARK, "forker sending data to %d", cur_child->pid );
				osrfLogInternal( OSRF_LOG_MARK, "Writing to child fd %d",
					cur_child->write_data_fd );

				const char* msg_data = cur_msg->msg_xml;
				int written = write( cur_child->write_data_fd, msg_data, strlen( msg_data ) + 1 );
				if( written < 0 ) {
					// This child appears to be dead or unusable.  Discard it.
					osrfLogWarning( OSRF_LOG_MARK, "Write returned error %d: %s",
						errno, strerror( errno ));
					kill( cur_child->pid, SIGKILL );
					del_prefork_child( forker, cur_child->pid );
					continue;
				}

				add_prefork_child( forker, cur_child );  // Add it to active list
				honored = 1;
				break;
			}

			/* if none available, add a new child if we can */
			if( ! honored ) {
				osrfLogDebug( OSRF_LOG_MARK, "Not enough children, attempting to add..." );

				if( forker->current_num_children < forker->max_children ) {
					osrfLogDebug( OSRF_LOG_MARK,  "Launching new child with current_num = %d",
						forker->current_num_children );

					launch_child( forker );  // Put a new child into the idle list
					if( forker->idle_list ) {

						// Take the new child from the idle list
						prefork_child* new_child = forker->idle_list;
						forker->idle_list = new_child->next;
						new_child->next = NULL;

						osrfLogDebug( OSRF_LOG_MARK, "Writing to new child fd %d : pid %d",
							new_child->write_data_fd, new_child->pid );

						const char* msg_data = cur_msg->msg_xml;
						int written = write(
							new_child->write_data_fd, msg_data, strlen( msg_data ) + 1 );
						if( written < 0 ) {
							// This child appears to be dead or unusable.  Discard it.
							osrfLogWarning( OSRF_LOG_MARK, "Write returned error %d: %s",
								errno, strerror( errno ));
							kill( cur_child->pid, SIGKILL );
							del_prefork_child( forker, cur_child->pid );
						} else {
							add_prefork_child( forker, new_child );
							honored = 1;
						}
					}
                } else {
                    osrfLogWarning( OSRF_LOG_MARK, "Could not launch a new child as %d children "
                        "were already running; consider increasing max_children for this "
                        "application higher than %d in the OpenSRF configuration if this "
                        "message occurs frequently",
                        forker->current_num_children, forker->max_children );
                }
            }

			if( child_dead )
				reap_children( forker );

			if( !honored ) {
				break;
			}

		} // end while( ! honored )

		if ( honored ) {
			backlog_queue_head = cur_msg->next;
			backlog_queue_size--;
			cur_msg->next = NULL;
			message_free( cur_msg );
		}

	} /* end top level listen loop */
}


/**
	@brief See if any children have become available.
	@param forker Pointer to the prefork_simple that owns the children.
	@param forever Boolean: true if we should wait indefinitely.
    @return 0 or greater if successful, -1 on select error/interrupt

	Call select() for all the children in the active list.  Read each active file
	descriptor and move the corresponding child to the idle list.

	If @a forever is true, wait indefinitely for input.  Otherwise return immediately if
	there are no active file descriptors.
*/
static int check_children( prefork_simple* forker, int forever ) {

	if( child_dead )
		reap_children( forker );

	if( NULL == forker->first_child ) {
		// If forever is true, then we're here because we've run out of idle
		// processes, so there should be some active ones around, except during
		// graceful shutdown, as we wait for all active children to become idle.
		// If forever is false, then the children may all be idle, and that's okay.
		if( forever )
			osrfLogDebug( OSRF_LOG_MARK, "No active child processes to check" );
		return 0;
	}

	int select_ret;
	fd_set read_set;
	FD_ZERO( &read_set );
	int max_fd = 0;
	int n;

	// Prepare to select() on pipes from all the active children
	prefork_child* cur_child = forker->first_child;
	do {
		if( cur_child->read_status_fd > max_fd )
			max_fd = cur_child->read_status_fd;
		FD_SET( cur_child->read_status_fd, &read_set );
		cur_child = cur_child->next;
	} while( cur_child != forker->first_child );

	FD_CLR( 0, &read_set ); /* just to be sure */

	if( forever ) {

		if( (select_ret=select( max_fd + 1, &read_set, NULL, NULL, NULL )) == -1 ) {
			osrfLogWarning( OSRF_LOG_MARK, "Select returned error %d on check_children: %s",
				errno, strerror( errno ));
		}
		osrfLogInfo( OSRF_LOG_MARK,
			"select() completed after waiting on children to become available" );

	} else {

		struct timeval tv;
		tv.tv_sec   = 0;
		tv.tv_usec  = 0;

		if( (select_ret=select( max_fd + 1, &read_set, NULL, NULL, &tv )) == -1 ) {
			osrfLogWarning( OSRF_LOG_MARK, "Select returned error %d on check_children: %s",
				errno, strerror( errno ));
		}
	}

    if( select_ret <= 0 ) // we're done here
		return select_ret;

	// Check each child in the active list.
	// If it has responded, move it to the idle list.
	cur_child = forker->first_child;
	prefork_child* next_child = NULL;
	int num_handled = 0;
	do {
		next_child = cur_child->next;
		if( FD_ISSET( cur_child->read_status_fd, &read_set )) {
			osrfLogDebug( OSRF_LOG_MARK,
				"Server received status from a child %d", cur_child->pid );

			num_handled++;

			/* now suck off the data */
			char buf[64];
			if( (n=read( cur_child->read_status_fd, buf, sizeof( buf ) - 1 )) < 0 ) {
				osrfLogWarning( OSRF_LOG_MARK,
					"Read error after select in child status read with errno %d: %s",
					errno, strerror( errno ));
			}
			else {
				buf[n] = '\0';
				osrfLogDebug( OSRF_LOG_MARK,  "Read %d bytes from status buffer: %s", n, buf );
			}


            // if this child is in the sighup_pending list, kill the child,
            // but leave it in the active list so that it won't be picked
            // for new work.  When reap_children() next runs, it will be 
            // properly cleaned up.
            prefork_child* hup_child = forker->sighup_pending_list;
            prefork_child* prev_hup_child = NULL;
            int hup_cleanup = 0;

            while (hup_child) {
                pid_t hup_pid = hup_child->pid;
                if (hup_pid == cur_child->pid) {

                    osrfLogDebug(OSRF_LOG_MARK, 
                        "server: killing previously-active child after "
                        "receiving SIGHUP: %d", hup_pid);

                    if (forker->sighup_pending_list == hup_child) {
                        // hup_child is the first (maybe only) in the list
                        forker->sighup_pending_list = hup_child->next;
                    } else {
                        // splice it from the list
                        prev_hup_child->next = hup_child->next;
                    }

                    free(hup_child); // clean up the thin clone
                    kill(hup_pid, SIGKILL);
                    hup_cleanup = 1;
                    break;
                }

                prev_hup_child = hup_child;
                hup_child = hup_child->next;
            }

            if (!hup_cleanup) {

                // Remove the child from the active list
                if( forker->first_child == cur_child ) {
                    if( cur_child->next == cur_child ) {
                        // only child in the active list
                        forker->first_child = NULL;   
                    } else {
                        forker->first_child = cur_child->next;
                    }
                }
                cur_child->next->prev = cur_child->prev;
                cur_child->prev->next = cur_child->next;

                // Add it to the idle list
                cur_child->prev = NULL;
                cur_child->next = forker->idle_list;
                forker->idle_list = cur_child;
            }
        }

        cur_child = next_child;
    } while( forker->first_child && forker->first_child != next_child );

    return select_ret;
}

/**
	@brief Service up a set maximum number of requests; then shut down.
	@param child Pointer to the prefork_child representing the child process.

	Called only by child process.

	Enter a loop, for up to max_requests iterations.  On each iteration:
	- Wait indefinitely for a request from the parent.
	- Service the request.
	- Increment a counter.  If the limit hasn't been reached, notify the parent that you
	are available for another request.

	After exiting the loop, shut down and terminate the process.
*/
static void prefork_child_wait( prefork_child* child ) {

	int i,n;
	growing_buffer* gbuf = buffer_init( READ_BUFSIZE );
	char buf[READ_BUFSIZE];

	for( i = 0; i < child->max_requests; i++ ) {

		n = -1;
		int gotdata = 0;    // boolean; set to true if we get data
		clr_fl( child->read_data_fd, O_NONBLOCK );

		// Read a request from the parent, via a pipe, into a growing_buffer.
		while( (n = read( child->read_data_fd, buf, READ_BUFSIZE-1 )) > 0 ) {
			buf[n] = '\0';
			osrfLogDebug( OSRF_LOG_MARK, "Prefork child read %d bytes of data", n );
			if( !gotdata ) {
				set_fl( child->read_data_fd, O_NONBLOCK );
				gotdata = 1;
			}
			buffer_add_n( gbuf, buf, n );
		}

		if( errno == EAGAIN )
			n = 0;

		if( errno == EPIPE ) {
			osrfLogDebug( OSRF_LOG_MARK, "C child attempted read on broken pipe, exiting..." );
			break;
		}

		int terminate_now = 0;     // Boolean

		if( n < 0 ) {
			osrfLogWarning( OSRF_LOG_MARK,
				"Prefork child read returned error with errno %d", errno );
			break;

		} else if( gotdata ) {
			// Process the request
			osrfLogDebug( OSRF_LOG_MARK, "Prefork child got a request.. processing.." );
			terminate_now = prefork_child_process_request( child, gbuf->buf );
			buffer_reset( gbuf );
		}

		if( terminate_now ) {
			// We're terminating prematurely -- presumably due to a fatal error condition.
			osrfLogWarning( OSRF_LOG_MARK, "Prefork child terminating abruptly" );
			break;
		}

		if( i < child->max_requests - 1 ) {
			// Report back to the parent for another request.
			size_t msg_len = 9;
			ssize_t len = write(
				child->write_status_fd, "available" /*less than 64 bytes*/, msg_len );
			if( len != msg_len ) {
				osrfLogError( OSRF_LOG_MARK,
					"Drone terminating: unable to notify listener of availability: %s",
					strerror( errno ));
				buffer_free( gbuf );
				osrf_prefork_child_exit( child );
			}
		}
	}

	buffer_free( gbuf );

	osrfLogDebug( OSRF_LOG_MARK, "Child with max-requests=%d, num-served=%d exiting...[%ld]",
		child->max_requests, i, (long) getpid());

	osrf_prefork_child_exit( child );
}

/**
	@brief Add a prefork_child to the end of the active list.
	@param forker Pointer to the prefork_simple that owns the list.
	@param child Pointer to the prefork_child to be added.
*/
static void add_prefork_child( prefork_simple* forker, prefork_child* child ) {

	if( forker->first_child == NULL ) {
		// Simplest case: list is initially empty.
		forker->first_child = child;
		child->next = child;
		child->prev = child;
	} else {
		// Find the last node in the circular list.
		prefork_child* last_child = forker->first_child->prev;

		// Insert the new child between the last and first children.
		last_child->next = child;
		child->prev      = last_child;
		child->next      = forker->first_child;
		forker->first_child->prev = child;
	}
}

/**
	@brief Delete and destroy a dead child from our list.
	@param forker Pointer to the prefork_simple that owns the dead child.
	@param pid Process ID of the dead child.

	Look for the dead child first in the list of active children.  If you don't find it
	there, look in the list of idle children.  If you find it, remove it from whichever
	list it's on, and destroy it.
*/
static void del_prefork_child( prefork_simple* forker, pid_t pid ) {

	osrfLogDebug( OSRF_LOG_MARK, "Deleting Child: %d", pid );

	prefork_child* cur_child = NULL;

	// Look first in the active list
	if( forker->first_child ) {
		cur_child = forker->first_child; /* current pointer */
		while( cur_child->pid != pid && cur_child->next != forker->first_child )
			cur_child = cur_child->next;

		if( cur_child->pid == pid ) {
			// We found the right node.  Remove it from the list.
			if( cur_child->next == cur_child )
				forker->first_child = NULL;    // only child in the list
			else {
				if( forker->first_child == cur_child )
					forker->first_child = cur_child->next;  // Reseat forker->first_child

				// Stitch the adjacent nodes together
				cur_child->prev->next = cur_child->next;
				cur_child->next->prev = cur_child->prev;
			}
		} else
			cur_child = NULL;  // Didn't find it in the active list
	}

	if( ! cur_child ) {
		// Maybe it's in the idle list.  This can happen if, for example,
		// a child is killed by a signal while it's between requests.

		prefork_child* prev = NULL;
		cur_child = forker->idle_list;
		while( cur_child && cur_child->pid != pid ) {
			prev = cur_child;
			cur_child = cur_child->next;
		}

		if( cur_child ) {
			// Detach from the list
			if( prev )
				prev->next = cur_child->next;
			else
				forker->idle_list = cur_child->next;
		} // else we can't find it
	}

	// If we found the node, destroy it.
	if( cur_child )
		prefork_child_free( forker, cur_child );
}

/**
	@brief Create and initialize a prefork_child.
	@param forker Pointer to the prefork_simple that will own the prefork_child.
	@param read_data_fd Used by child to read request from parent.
	@param write_data_fd Used by parent to write request to child.
	@param read_status_fd Used by parent to read status from child.
	@param write_status_fd Used by child to write status to parent.
	@return Pointer to the newly created prefork_child.

	The calling code is responsible for freeing the prefork_child by calling
	prefork_child_free().
*/
static prefork_child* prefork_child_init( prefork_simple* forker,
	int read_data_fd, int write_data_fd,
	int read_status_fd, int write_status_fd ) {

	// Allocate a prefork_child -- from the free list if possible, or from
	// the heap if necessary.  The free list is a non-circular, singly-linked list.
	prefork_child* child;
	if( forker->free_list ) {
		child = forker->free_list;
		forker->free_list = child->next;
	} else
		child = safe_malloc( sizeof( prefork_child ));

	child->pid              = 0;
	child->read_data_fd     = read_data_fd;
	child->write_data_fd    = write_data_fd;
	child->read_status_fd   = read_status_fd;
	child->write_status_fd  = write_status_fd;
	child->max_requests     = forker->max_requests;
	child->appname          = forker->appname;  // We don't make a separate copy
	child->keepalive        = forker->keepalive;
	child->next             = NULL;
	child->prev             = NULL;

	return child;
}

/**
	@brief Terminate all child processes and clear out a prefork_simple.
	@param prefork Pointer to the prefork_simple to be cleared out.

	We do not deallocate the prefork_simple itself, just its contents.
*/
static void prefork_clear( prefork_simple* prefork, bool graceful ) {

	// always de-register routers before killing child processes (or waiting
	// for them to complete) so that new requests are directed elsewhere.
	osrf_prefork_register_routers(global_forker->appname, true);

	while( prefork->first_child ) {

		if (graceful) {
			// wait for at least one active child to become idle, then repeat.
			// once complete, all children will be idle and cleaned up below.
			osrfLogInfo(OSRF_LOG_MARK, "graceful shutdown waiting...");
			check_children(prefork, 1);

		} else {
			// Kill and delete all the active children
			kill( prefork->first_child->pid, SIGKILL );
			del_prefork_child( prefork, prefork->first_child->pid );
		}
	}

	if (graceful) {
		osrfLogInfo(OSRF_LOG_MARK,
			"all active children are now idle in graceful shutdown");
	}

	// Kill all the idle prefork children, close their file
	// descriptors, and move them to the free list.
	prefork_child* child = prefork->idle_list;
	prefork->idle_list = NULL;
	while( child ) {
		prefork_child* temp = child->next;
		kill( child->pid, SIGKILL );
		prefork_child_free( prefork, child );
		child = temp;
	}
	//prefork->current_num_children = 0;

	// Physically free the free list of prefork_children.
	child = prefork->free_list;
	prefork->free_list = NULL;
	while( child ) {
		prefork_child* temp = child->next;
		free( child );
		child = temp;
	}

	// Close the Jabber connection
	client_free( prefork->connection );
	prefork->connection = NULL;

	// After giving the child processes a second to terminate, wait on them so that they
	// don't become zombies.  We don't wait indefinitely, so it's possible that some
	// children will survive a bit longer.
	sleep( 1 );
	while( (waitpid( -1, NULL, WNOHANG )) > 0 ) {
		--prefork->current_num_children;
	}

	free( prefork->appname );
	prefork->appname = NULL;
}

/**
	@brief Destroy and deallocate a prefork_child.
	@param forker Pointer to the prefork_simple that owns the prefork_child.
	@param child Pointer to the prefork_child to be destroyed.
*/
static void prefork_child_free( prefork_simple* forker, prefork_child* child ) {
	close( child->read_data_fd );
	close( child->write_data_fd );
	close( child->read_status_fd );
	close( child->write_status_fd );

	// Stick the prefork_child in a free list for potential reuse.  This is a
	// non-circular, singly linked list.
	child->prev = NULL;
	child->next = forker->free_list;
	forker->free_list = child;
}
