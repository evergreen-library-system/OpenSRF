/**
	@file osrf_system.c
	@brief Launch a collection of servers.
*/

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>

#include "opensrf/utils.h"
#include "opensrf/log.h"
#include "opensrf/osrf_system.h"
#include "opensrf/osrf_application.h"
#include "opensrf/osrf_prefork.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

static void report_child_status( pid_t pid, int status );
struct child_node;
typedef struct child_node ChildNode;
osrfStringArray* log_protect_arr = NULL;

/**
	@brief Represents a child process.
*/
struct child_node
{
	ChildNode* pNext;  /**< Linkage pointer for doubly linked list. */
	ChildNode* pPrev;  /**< Linkage pointer for doubly linked list. */
	pid_t pid;         /**< Process ID of the child process. */
	char* app;
	char* libfile;
};

/** List of child processes. */
static ChildNode* child_list;

/** Pointer to the global transport_client; i.e. our connection to Jabber. */
static transport_client* osrfGlobalTransportClient = NULL;

/** Switch to be set by signal handler */
static volatile sig_atomic_t sig_caught;

/** Boolean: set to true when we finish shutting down. */
static int shutdownComplete = 0;

/** Name of file to which to write the process ID of the child process */
char* pidfile_name = NULL;

static void add_child( pid_t pid, const char* app, const char* libfile );
static void delete_child( ChildNode* node );
static void delete_all_children( void );
static ChildNode* seek_child( pid_t pid );

/**
	@brief Wait on all dead child processes so that they won't be zombies.
*/
static void reap_children( void ) {
	if( sig_caught ) {
		if( SIGTERM == sig_caught || SIGINT == sig_caught ) {
			osrfLogInfo( OSRF_LOG_MARK, "Killed by %s; terminating",
					SIGTERM == sig_caught ? "SIGTERM" : "SIGINT" );
		} else
			osrfLogInfo( OSRF_LOG_MARK, "Killed by signal %d; terminating", (int) sig_caught );
	}

	// If we caught a signal, then the signal handler already did a kill().
	// If we didn't, then do the kill() now.
	if( ! sig_caught )
		kill( 0, SIGTERM );

	sleep(1); /* Give the children a chance to die before we reap them. */

	// Wait for each dead child.  The WNOHANG option means to return immediately if
	// there are no dead children, instead of waiting for them to die.  It is therefore
	// possible for a child still to be alive when we exit this function, either because
	// it intercepted the SIGTERM and ignored it, or because it took longer to die than
	// the time we gave it.
	pid_t child_pid;
	while( (child_pid = waitpid(-1, NULL, WNOHANG)) > 0 )
		osrfLogInfo(OSRF_LOG_MARK, "Killed child %d", child_pid);

	// Remove all nodes from the list of child processes.
	delete_all_children();
}

/**
	@brief Signal handler for SIGTERM and SIGINT.

	Kill all child processes, and set a switch so that we'll know that the signal arrived.
*/
static void handleKillSignal( int signo ) {
	// First ignore SIGTERM.  Otherwise we would send SIGTERM to ourself, intercept it,
	// and kill() again in an endless loop.
	signal( SIGTERM, SIG_IGN );

	//Kill all child processes.  This is safe to do in a signal handler, because POSIX
	// specifies that kill() is reentrant.  It is necessary because, if we did the kill()
	// only in reap_children() (above), then there would be a narrow window of vulnerability
	// in the main loop: if the signal arrives between checking sig_caught and calling wait(),
	// we would wait indefinitely for a child to die on its own.
	kill( 0, SIGTERM );
	sig_caught = signo;
}

/**
	@brief Return a pointer to the global transport_client.
	@return Pointer to the global transport_client, or NULL.

	A given process needs only one connection to Jabber, so we keep it a pointer to it at
	file scope.  This function returns that pointer.

	If the connection has been opened by a previous call to osrfSystemBootstrapClientResc(),
	return the pointer.  Otherwise return NULL.
*/
transport_client* osrfSystemGetTransportClient( void ) {
	return osrfGlobalTransportClient;
}

/**
	@brief Save a copy of a file name to be used for writing a process ID.
	@param name Designated file name, or NULL.

	Save a file name for later use in saving a process ID.  If @a name is NULL, leave
	the file name NULL.

	When the parent process spawns a child, the child becomes a daemon.  The parent writes the
	child's process ID to the PID file, if one has been designated, so that some other process
	can retrieve the PID later and kill the daemon.
*/
void osrfSystemSetPidFile( const char* name ) {
	if( pidfile_name )
		free( pidfile_name );

	if( name )
		pidfile_name = strdup( name );
	else
		pidfile_name = NULL;
}

/**
	@brief Discard the global transport_client, but without disconnecting from Jabber.

	To be called by a child process in order to disregard the parent's connection without
	disconnecting it, since disconnecting would disconnect the parent as well.
*/
void osrfSystemIgnoreTransportClient() {
	client_discard( osrfGlobalTransportClient );
	osrfGlobalTransportClient = NULL;
}

/**
	@brief Bootstrap a generic application from info in the configuration file.
	@param config_file Name of the configuration file.
	@param contextnode Name of an aggregate within the configuration file, containing the
	relevant subset of configuration stuff.
	@return 1 if successful; zero or -1 if error.

	- Load the configuration file.
	- Open the log.
	- Open a connection to Jabber.

	A thin wrapper for osrfSystemBootstrapClientResc, passing it NULL for a resource.
*/
int osrf_system_bootstrap_client( char* config_file, char* contextnode ) {
	return osrfSystemBootstrapClientResc(config_file, contextnode, NULL);
}

/**
	@brief Connect to one or more cache servers.
	@return Zero in all cases.
*/
int osrfSystemInitCache( void ) {

	jsonObject* cacheServers = osrf_settings_host_value_object("/cache/global/servers/server");
	char* maxCache = osrf_settings_host_value("/cache/global/max_cache_time");

	if( cacheServers && maxCache) {

		if( cacheServers->type == JSON_ARRAY ) {
			int i;
			const char* servers[cacheServers->size];
			for( i = 0; i != cacheServers->size; i++ ) {
				servers[i] = jsonObjectGetString( jsonObjectGetIndex(cacheServers, i) );
				osrfLogInfo( OSRF_LOG_MARK, "Adding cache server %s", servers[i]);
			}
			osrfCacheInit( servers, cacheServers->size, atoi(maxCache) );

		} else {
			const char* servers[] = { jsonObjectGetString(cacheServers) };
			osrfLogInfo( OSRF_LOG_MARK, "Adding cache server %s", servers[0]);
			osrfCacheInit( servers, 1, atoi(maxCache) );
		}

	} else {
		osrfLogError( OSRF_LOG_MARK,  "Missing config value for /cache/global/servers/server _or_ "
			"/cache/global/max_cache_time");
	}

	jsonObjectFree( cacheServers );
	return 0;
}

/**
	@brief Launch a collection of servers, as defined by the settings server.
	@param hostname Full network name of the host where the process is running; or
	'localhost' will do.
	@param configfile Name of the configuration file; normally '/openils/conf/opensrf_core.xml'.
	@param contextNode Name of an aggregate within the configuration file, containing the
	relevant subset of configuration stuff.
	@return - Zero if successful, or -1 if not.
*/
int osrfSystemBootstrap( const char* hostname, const char* configfile,
		const char* contextNode ) {
	if( !(hostname && configfile && contextNode) )
		return -1;

	// Load the conguration, open the log, open a connection to Jabber
	if(!osrfSystemBootstrapClientResc(configfile, contextNode, "settings_grabber" )) {
		osrfLogError( OSRF_LOG_MARK,
			"Unable to bootstrap for host %s from configuration file %s",
			hostname, configfile );
		return -1;
	}

	shutdownComplete = 0;

	// Get a list of applications to launch from the settings server
	int retcode = osrf_settings_retrieve(hostname);
	osrf_system_disconnect_client();

	if( retcode ) {
		osrfLogError( OSRF_LOG_MARK,
			"Unable to retrieve settings for host %s from configuration file %s",
			hostname, configfile );
		return -1;
	}

	// Turn into a daemon.  The parent forks and exits.  Only the
	// child returns, with the standard streams (stdin, stdout, and
	// stderr) redirected to /dev/null.
	daemonize();

	jsonObject* apps = osrf_settings_host_value_object("/activeapps/appname");
	osrfStringArray* arr = osrfNewStringArray(8);

	if(apps) {
		int i = 0;

		if(apps->type == JSON_STRING) {
			osrfStringArrayAdd(arr, jsonObjectGetString(apps));

		} else {
			const jsonObject* app;
			while( (app = jsonObjectGetIndex(apps, i++)) )
				osrfStringArrayAdd(arr, jsonObjectGetString(app));
		}
		jsonObjectFree(apps);

		const char* appname = NULL;
		int first_launch = 1;             // Boolean
		i = 0;
		while( (appname = osrfStringArrayGetString(arr, i++)) ) {

			char* lang = osrf_settings_host_value("/apps/%s/language", appname);

			if(lang && !strcasecmp(lang,"c"))  {

				char* libfile = osrf_settings_host_value("/apps/%s/implementation", appname);

				if(! (appname && libfile) ) {
					osrfLogWarning( OSRF_LOG_MARK, "Missing appname / libfile in settings config");
					continue;
				}

				osrfLogInfo( OSRF_LOG_MARK, "Launching application %s with implementation %s",
						appname, libfile);

				pid_t pid;

				if( (pid = fork()) ) {    // if parent
					// store pid in local list for re-launching dead children...
					add_child( pid, appname, libfile );
					osrfLogInfo( OSRF_LOG_MARK, "Running application child %s: process id %ld",
								 appname, (long) pid );

					if( first_launch ) {
						if( pidfile_name ) {
							// Write our own PID to a PID file so that somebody can use it to
							// send us a signal later.  If we don't find any C apps to launch,
							// then we will quietly exit without writing a PID file, and without
							// waiting to be killed by a signal.

							FILE* pidfile = fopen( pidfile_name, "w" );
							if( !pidfile ) {
								osrfLogError( OSRF_LOG_MARK, "Unable to open PID file \"%s\": %s",
									pidfile_name, strerror( errno ) );
								free( pidfile_name );
								pidfile_name = NULL;
								return -1;
							} else {
								fprintf( pidfile, "%ld\n", (long) getpid() );
								fclose( pidfile );
							}
						}
						first_launch = 0;
					}

				} else {         // if child, run the application

					osrfLogInfo( OSRF_LOG_MARK, " * Running application %s\n", appname);
					if( pidfile_name ) {
						free( pidfile_name );    // tidy up some debris from the parent
						pidfile_name = NULL;
					}
					if( osrfAppRegisterApplication( appname, libfile ) == 0 )
						osrf_prefork_run(appname);

					osrfLogDebug( OSRF_LOG_MARK, "Server exiting for app %s and library %s\n",
							appname, libfile );
					exit(0);
				}
			} // language == c
		} // end while
	}

	osrfStringArrayFree(arr);

	signal(SIGTERM, handleKillSignal);
	signal(SIGINT, handleKillSignal);

	// Wait indefinitely for all the child processes to terminate, or for a signal to
	// tell us to stop.  When there are no more child processes, wait() returns an
	// ECHILD error and we break out of the loop.
	int status;
	pid_t pid;
	while( ! sig_caught ) {
		pid = wait( &status );
		if( -1 == pid ) {
			if( errno == ECHILD )
				osrfLogError( OSRF_LOG_MARK, "We have no more live services... exiting" );
			else if( errno != EINTR )
				osrfLogError(OSRF_LOG_MARK, "Exiting top-level system loop with error: %s",
						strerror( errno ) );

			// Since we're not being killed by a signal as usual, delete the PID file
			// so that no one will try to kill us when we're already dead.
			if( pidfile_name )
				remove( pidfile_name );
			break;
		} else {
			report_child_status( pid, status );
		}
	}

	reap_children();
	osrfConfigCleanup();
	osrf_system_disconnect_client();
	osrf_settings_free_host_config(NULL);
	free( pidfile_name );
	pidfile_name = NULL;
	return 0;
}

/**
	@brief Report the exit status of a dead child process, then remove it from the list.
	@param pid Process ID of the child.
	@param status Exit status as captured by wait().
*/
static void report_child_status( pid_t pid, int status )
{
	const char* app;
	const char* libfile;
	ChildNode* node = seek_child( pid );

	if( node ) {
		app     = node->app     ? node->app     : "[unknown]";
		libfile = node->libfile ? node->libfile : "[none]";
	} else
		app = libfile = "";

	if( WIFEXITED( status ) )
	{
		int rc = WEXITSTATUS( status );  // return code of child process
		if( rc )
			osrfLogError( OSRF_LOG_MARK, "Child process %ld (app %s) exited with return code %d",
					(long) pid, app, rc );
		else
			osrfLogInfo( OSRF_LOG_MARK, "Child process %ld (app %s) exited normally",
					(long) pid, app );
	}
	else if( WIFSIGNALED( status ) )
	{
		osrfLogError( OSRF_LOG_MARK, "Child process %ld (app %s) killed by signal %d",
					  (long) pid, app, WTERMSIG( status) );
	}
	else if( WIFSTOPPED( status ) )
	{
		osrfLogError( OSRF_LOG_MARK, "Child process %ld (app %s) stopped by signal %d",
					  (long) pid, app, (int) WSTOPSIG( status ) );
	}

	delete_child( node );
}

/*----------- Routines to manage list of children --*/

/**
	@brief Add a node to the list of child processes.
	@param pid Process ID of the child process.
	@param app Name of the child application.
	@param libfile Name of the shared library where the child process resides.
*/
static void add_child( pid_t pid, const char* app, const char* libfile )
{
	/* Construct new child node */

	ChildNode* node = safe_malloc( sizeof( ChildNode ) );

	node->pid = pid;

	if( app )
		node->app = strdup( app );
	else
		node->app = NULL;

	if( libfile )
		node->libfile = strdup( libfile );
	else
		node->libfile = NULL;

	/* Add new child node to the head of the list */

	node->pNext = child_list;
	node->pPrev = NULL;

	if( child_list )
		child_list->pPrev = node;

	child_list = node;
}

/**
	@brief Remove a node from the list of child processes.
	@param node Pointer to the node to be removed.
*/
static void delete_child( ChildNode* node ) {

	/* Sanity check */

	if( ! node )
		return;

	/* Detach the node from the list */

	if( node->pPrev )
		node->pPrev->pNext = node->pNext;
	else
		child_list = node->pNext;

	if( node->pNext )
		node->pNext->pPrev = node->pPrev;

	/* Deallocate the node and its payload */

	free( node->app );
	free( node->libfile );
	free( node );
}

/**
	@brief Remove all nodes from the list of child processes, rendering it empty.
*/
static void delete_all_children( void ) {

	while( child_list )
		delete_child( child_list );
}

/**
	@brief Find the node for a child process of a given process ID.
	@param pid The process ID of the child process.
	@return A pointer to the corresponding node if found; otherwise NULL.
*/
static ChildNode* seek_child( pid_t pid ) {

	/* Return a pointer to the child node for the */
	/* specified process ID, or NULL if not found */

	ChildNode* node = child_list;
	while( node ) {
		if( node->pid == pid )
			break;
		else
			node = node->pNext;
	}

	return node;
}

/*----------- End of routines to manage list of children --*/

/**
	@brief Bootstrap a generic application from info in the configuration file.
	@param config_file Name of the configuration file.
	@param contextnode Name of an aggregate within the configuration file, containing the
	relevant subset of configuration stuff.
	@param resource Used to construct a Jabber resource name; may be NULL.
	@return 1 if successful; zero or -1 if error.

	- Load the configuration file.
	- Open the log.
	- Open a connection to Jabber.
*/
int osrfSystemBootstrapClientResc( const char* config_file,
		const char* contextnode, const char* resource ) {

	int failure = 0;

	if(osrfSystemGetTransportClient()) {
		osrfLogInfo(OSRF_LOG_MARK, "Client is already bootstrapped");
		return 1; /* we already have a client connection */
	}

	if( !( config_file && contextnode ) && ! osrfConfigHasDefaultConfig() ) {
		osrfLogError( OSRF_LOG_MARK, "No Config File Specified\n" );
		return -1;
	}

	if( config_file ) {
		osrfConfig* cfg = osrfConfigInit( config_file, contextnode );
		if(cfg)
			osrfConfigSetDefaultConfig(cfg);
		else
			return 0;   /* Can't load configuration?  Bail out */

		// fetch list of configured log redaction marker strings
		log_protect_arr = osrfNewStringArray(8);
		osrfConfig* cfg_shared = osrfConfigInit(config_file, "shared");
		osrfConfigGetValueList( cfg_shared, log_protect_arr, "/log_protect/match_string" );
	}

	char* log_file      = osrfConfigGetValue( NULL, "/logfile");
	if(!log_file) {
		fprintf(stderr, "No log file specified in configuration file %s\n",
				config_file);
		return -1;
	}

	char* log_level      = osrfConfigGetValue( NULL, "/loglevel" );
	osrfStringArray* arr = osrfNewStringArray(8);
	osrfConfigGetValueList(NULL, arr, "/domain");

	char* username       = osrfConfigGetValue( NULL, "/username" );
	char* password       = osrfConfigGetValue( NULL, "/passwd" );
	char* port           = osrfConfigGetValue( NULL, "/port" );
	char* unixpath       = osrfConfigGetValue( NULL, "/unixpath" );
	char* facility       = osrfConfigGetValue( NULL, "/syslog" );
	char* actlog         = osrfConfigGetValue( NULL, "/actlog" );

	/* if we're a source-client, tell the logger */
	char* isclient = osrfConfigGetValue(NULL, "/client");
	if( isclient && !strcasecmp(isclient,"true") )
		osrfLogSetIsClient(1);
	free(isclient);

	int llevel = 0;
	int iport = 0;
	if(port) iport = atoi(port);
	if(log_level) llevel = atoi(log_level);

	if(!strcmp(log_file, "syslog")) {
		osrfLogInit( OSRF_LOG_TYPE_SYSLOG, contextnode, llevel );
		osrfLogSetSyslogFacility(osrfLogFacilityToInt(facility));
		if(actlog) osrfLogSetSyslogActFacility(osrfLogFacilityToInt(actlog));

	} else {
		osrfLogInit( OSRF_LOG_TYPE_FILE, contextnode, llevel );
		osrfLogSetFile( log_file );
	}


	/* Get a domain, if one is specified */
	const char* domain = osrfStringArrayGetString( arr, 0 ); /* just the first for now */
	if(!domain) {
		fprintf(stderr, "No domain specified in configuration file %s\n", config_file);
		osrfLogError( OSRF_LOG_MARK, "No domain specified in configuration file %s\n",
				config_file );
		failure = 1;
	}

	if(!username) {
		fprintf(stderr, "No username specified in configuration file %s\n", config_file);
		osrfLogError( OSRF_LOG_MARK, "No username specified in configuration file %s\n",
				config_file );
		failure = 1;
	}

	if(!password) {
		fprintf(stderr, "No password specified in configuration file %s\n", config_file);
		osrfLogError( OSRF_LOG_MARK, "No password specified in configuration file %s\n",
				config_file);
		failure = 1;
	}

	if((iport <= 0) && !unixpath) {
		fprintf(stderr, "No unixpath or valid port in configuration file %s\n", config_file);
		osrfLogError( OSRF_LOG_MARK, "No unixpath or valid port in configuration file %s\n",
			config_file);
		failure = 1;
	}

	if (failure) {
		osrfStringArrayFree(arr);
		free(log_file);
		free(log_level);
		free(username);
		free(password);
		free(port);
		free(unixpath);
		free(facility);
		free(actlog);
		return 0;
	}

	osrfLogInfo( OSRF_LOG_MARK, "Bootstrapping system with domain %s, port %d, and unixpath %s",
		domain, iport, unixpath ? unixpath : "(none)" );
	transport_client* client = client_init( domain, iport, unixpath, 0 );

	char host[HOST_NAME_MAX + 1] = "";
	gethostname(host, sizeof(host) );
	host[HOST_NAME_MAX] = '\0';

	char tbuf[32];
	tbuf[0] = '\0';
	snprintf(tbuf, 32, "%f", get_timestamp_millis());

	if(!resource) resource = "";

	int len = strlen(resource) + 256;
	char buf[len];
	buf[0] = '\0';
	snprintf(buf, len - 1, "%s_%s_%s_%ld", resource, host, tbuf, (long) getpid() );

	if(client_connect( client, username, password, buf, 10, AUTH_DIGEST )) {
		osrfGlobalTransportClient = client;
	}

	osrfStringArrayFree(arr);
	free(actlog);
	free(facility);
	free(log_level);
	free(log_file);
	free(username);
	free(password);
	free(port);
	free(unixpath);

	if(osrfGlobalTransportClient)
		return 1;

	return 0;
}

/**
	@brief Disconnect from Jabber.
	@return Zero in all cases.
*/
int osrf_system_disconnect_client( void ) {
	client_disconnect( osrfGlobalTransportClient );
	client_free( osrfGlobalTransportClient );
	osrfGlobalTransportClient = NULL;
	return 0;
}

/**
	@brief Shut down a laundry list of facilities typically used by servers.

	Things to shut down:
	- Settings from configuration file
	- Cache
	- Connection to Jabber
	- Settings from settings server
	- Application sessions
	- Logs
*/
int osrf_system_shutdown( void ) {
	if(shutdownComplete)
		return 0;
	else {
		osrfConfigCleanup();
		osrfCacheCleanup();
		osrf_system_disconnect_client();
		osrf_settings_free_host_config(NULL);
		osrfAppSessionCleanup();
		osrfLogCleanup();
		shutdownComplete = 1;
		return 1;
	}
}
