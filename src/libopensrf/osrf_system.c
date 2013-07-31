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

osrfStringArray* log_protect_arr = NULL;

/** Pointer to the global transport_client; i.e. our connection to Jabber. */
static transport_client* osrfGlobalTransportClient = NULL;

/** Boolean: set to true when we finish shutting down. */
static int shutdownComplete = 0;

/** Returns the full path to the pid file for the service */
static char* get_pid_file(const char* path, const char* service);

static int stop_service(const char* path, const char* service);

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

static char* get_pid_file(const char* piddir, const char* service) {
    int nsize = strlen(piddir) + strlen(service) + 6;
    char pfname[nsize];
    snprintf(pfname, nsize, "%s/%s.pid", piddir, service);
    pfname[nsize-1] = '\0';
    return strdup(pfname);
}

// TERM the process and delete the PID file
static int stop_service(const char* piddir, const char* service) {
    char pidstr[16];
    char* pidfile_name = get_pid_file(piddir, service);
    FILE* pidfile = fopen(pidfile_name, "r");

    osrfLogInfo(OSRF_LOG_MARK, "Stopping service %s", service);

    if (pidfile) {

        if (fgets(pidstr, 16, pidfile) != NULL) {
            long pid = atol(pidstr);

            if (pid) {
                // we have a PID, now send the TERM signal the process
                fprintf(stdout, 
                    "* stopping service pid=%ld %s\n", pid, service);
                kill(pid, SIGTERM);
            }

        } else {
            osrfLogWarning(OSRF_LOG_MARK,
                "Unable to read pid file %s", pidfile_name);
        }

        fclose(pidfile);

        if (unlink(pidfile_name) != 0) {
            osrfLogError(OSRF_LOG_MARK, 
                "Unable to delete pid file %s", pidfile_name);
        }

    } else {
        osrfLogWarning(OSRF_LOG_MARK, 
            "Unable to open pidfile %s for reading", pidfile_name);
    }
    
    free(pidfile_name);
    return 0;
}

/**
	@brief Launch one or more opensrf services
	@param hostname Full network name of the host where the process is 
        running; or 'localhost' will do.
	@param config Name of the configuration file; 
        normally '/openils/conf/opensrf_core.xml'.
	@param context Name of an aggregate within the configuration file, 
        containing the relevant subset of configuration stuff.
    @param piddir Name of the PID path the PID file directory
    @param action Name of action.  Options include start, start_all, stop,
        and stop_all
    @param service Name of the service to start/stop.  If no value is 
        specified, all C-based services are affected
	@return - Zero if successful, or -1 if not.
*/

// if service is null, all services are started
int osrf_system_service_ctrl(  
        const char* hostname, const char* config, 
        const char* context, const char* piddir, 
        const char* action, const char* service) {
    
    // Load the conguration, open the log, open a connection to Jabber
    if (!osrfSystemBootstrapClientResc(config, context, "c_launcher")) {
        osrfLogError(OSRF_LOG_MARK,
            "Unable to bootstrap for host %s from configuration file %s",
            hostname, config);
        return -1;
    }
    
    // Get the list of applications from the settings server
    // sometimes the network / settings server is slow to get going, s
    // so give it a few tries before giving up.
    int j;
    int retcode;
    for (j = 0; j < 3; j++) {
        retcode = osrf_settings_retrieve(hostname);
        if (retcode == 0) break; // success
        osrfLogInfo(OSRF_LOG_MARK, 
            "Unable to retrieve settings from settings server, retrying..");
        sleep(1);
    }

    // all done talking to the network
    osrf_system_disconnect_client();

    if (retcode) {
        osrfLogWarning(OSRF_LOG_MARK, "Unable to retrieve settings for "
            "host %s from configuration file %s", hostname, config);
        // this usually means settings server isn't running, which can happen
        // for a variety of reasons.  Log the problem then exit cleanly.
        return 0;
    }

    jsonObject* apps = osrf_settings_host_value_object("/activeapps/appname");

    if (!apps) {
        osrfLogInfo(OSRF_LOG_MARK, "OpenSRF-C found no apps to run");
        osrfConfigCleanup();
        osrf_settings_free_host_config(NULL);
    }

    osrfStringArray* arr = osrfNewStringArray(8);
    int i = 0;

    if(apps->type == JSON_STRING) {
        osrfStringArrayAdd(arr, jsonObjectGetString(apps));

    } else {
        const jsonObject* app;
        while( (app = jsonObjectGetIndex(apps, i++)) )
            osrfStringArrayAdd(arr, jsonObjectGetString(app));
    }
    jsonObjectFree(apps);

    i = 0;
    const char* appname = NULL;
    while ((appname = osrfStringArrayGetString(arr, i++))) {

        if (!appname) {
            osrfLogWarning(OSRF_LOG_MARK, 
                "Invalid service name at index %d", i);
            continue;
        }

        char* lang = osrf_settings_host_value("/apps/%s/language", appname);

        // this is not a C service, skip it.
        if (!lang || strcasecmp(lang, "c")) continue;

        // caller requested a specific service, but not this one
        if (service && strcmp(service, appname))
            continue;

        // stop service(s)
        if (!strncmp(action, "stop", 4)) {
            stop_service(piddir, appname);
            continue;
        }

        pid_t pid;
        if ((pid = fork())) {
            // parent process forks the Listener, logs the PID to stdout, 
            // then goes about its business
            fprintf(stdout, 
                "* starting service pid=%ld %s\n", (long) pid, appname);
            continue;
        }

        // this is the top-level Listener process.  It's responsible
        // for managing all of the processes related to a given service.
        daemonize();

        char* libfile = osrf_settings_host_value(
            "/apps/%s/implementation", appname);

        if (!libfile) {
            osrfLogError(OSRF_LOG_MARK, 
                "Service %s has no implemention", appname);
            exit(1);
        }

        osrfLogInfo(OSRF_LOG_MARK, 
            "Launching application %s with implementation %s",
            appname, libfile);

        // write the PID of our newly detached process to the PID file
        // pid file name is /path/to/dir/<service>.pid
        char* pidfile_name = get_pid_file(piddir, appname);
        FILE* pidfile = fopen(pidfile_name, "w");
        if (pidfile) {
            osrfLogDebug(OSRF_LOG_MARK, 
                "Writing PID %ld for service %s", (long) getpid(), appname);
            fprintf(pidfile, "%ld\n", (long) getpid());
            fclose(pidfile);
        } else {
            osrfLogError(OSRF_LOG_MARK, 
                "Unable to open PID file '%s': %s", 
                    pidfile_name, strerror(errno));
            exit(1);
        }
        free(pidfile_name);

        if (osrfAppRegisterApplication(appname, libfile) == 0)
            osrf_prefork_run(appname);

        osrfLogInfo(OSRF_LOG_MARK, 
            "Prefork Server exiting for service %s and implementation %s\n", 
            appname, libfile);

        exit(0);

    } // service name loop

    // main process can now go away
    osrfStringArrayFree(arr);
    osrfConfigCleanup();
    osrf_settings_free_host_config(NULL);

    return 0;
}

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
