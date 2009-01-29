#include "osrf_router.h"
#include <opensrf/osrfConfig.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>
#include <signal.h>

static osrfRouter* router = NULL;

void routerSignalHandler( int signo ) {
	osrfLogWarning( OSRF_LOG_MARK, "Received signal [%d], cleaning up...", signo );

    /* for now, just forcibly exit.  This is not a friendly way to clean up, but
     * there is a bug in osrfRouterFree() (in particular with cleaning up sockets),
     * that can cause the router process to stick around.  If we do this, we 
     * are guaranteed to exit.  
     */
    _exit(0);

	osrfConfigCleanup();
	osrfRouterFree(router);
	router = NULL;

	// Exit by re-raising the signal so that the parent
	// process can detect it
	
	signal( signo, SIG_DFL );
	raise( signo );
}

static int setupRouter(jsonObject* configChunk);


int main( int argc, char* argv[] ) {

	if( argc < 3 ) {
		osrfLogError( OSRF_LOG_MARK,
			"Usage: %s <path_to_config_file> <config_context>", argv[0] );
		exit( EXIT_FAILURE );
	}

	const char* config_file = argv[1];
	const char* context = argv[2];

	/* Get a set of router definitions from a config file */
	
	osrfConfig* cfg = osrfConfigInit(config_file, context);
	if( NULL == cfg ) {
		osrfLogError( OSRF_LOG_MARK, "Router can't load config file %s", config_file );
		exit( EXIT_FAILURE );
	}
	
	osrfConfigSetDefaultConfig(cfg);
    jsonObject* configInfo = osrfConfigGetValueObject(NULL, "/router");
	
	if( configInfo->size < 1 || NULL == jsonObjectGetIndex( configInfo, 1 ) ) {
		osrfLogError( OSRF_LOG_MARK, "No routers defined in config file %s, context \"%s\"",
			config_file, context );
		exit( EXIT_FAILURE );
	}
	
	/* We're done with the command line now, */
	/* so we can safely overlay it */
	
	init_proc_title( argc, argv );
	set_proc_title( "OpenSRF Router" );

	/* Spawn child process(es) */
	
    int i;
    for(i = 0; i < configInfo->size; i++) {
        jsonObject* configChunk = jsonObjectGetIndex(configInfo, i);
        if(fork() == 0) { /* create a new child to run this router instance */
            setupRouter(configChunk);
			break;  /* We're a child; don't spawn any more children here */
		}
    }

	return EXIT_SUCCESS;
}

int setupRouter(jsonObject* configChunk) {

    if(!jsonObjectGetKey(configChunk, "transport"))
        return 0; /* these are not the configs you're looking for */

	char* server = jsonObjectGetString(jsonObjectFindPath(configChunk, "/transport/server"));
	char* port = jsonObjectGetString(jsonObjectFindPath(configChunk, "/transport/port"));
	char* username = jsonObjectGetString(jsonObjectFindPath(configChunk, "/transport/username"));
	char* password = jsonObjectGetString(jsonObjectFindPath(configChunk, "/transport/password"));
	char* resource = jsonObjectGetString(jsonObjectFindPath(configChunk, "/transport/resource"));

	char* level = jsonObjectGetString(jsonObjectFindPath(configChunk, "/loglevel"));
	char* log_file = jsonObjectGetString(jsonObjectFindPath(configChunk, "/logfile"));
	char* facility = jsonObjectGetString(jsonObjectFindPath(configChunk, "/syslog"));

	int llevel = 1;
	if(level) llevel = atoi(level);

	if(!log_file) { fprintf(stderr, "Log file name not specified\n"); return -1; }

	if(!strcmp(log_file, "syslog")) {
		osrfLogInit( OSRF_LOG_TYPE_SYSLOG, "router", llevel );
		osrfLogSetSyslogFacility(osrfLogFacilityToInt(facility));

	} else {
		osrfLogInit( OSRF_LOG_TYPE_FILE, "router", llevel );
		osrfLogSetFile( log_file );
	}

	free(facility);
	free(level);
	free(log_file);

	osrfLogInfo(  OSRF_LOG_MARK, "Router connecting as: server: %s port: %s "
			"user: %s resource: %s", server, port, username, resource );

	int iport = 0;
	if(port)	iport = atoi( port );

	osrfStringArray* tclients = osrfNewStringArray(4);
	osrfStringArray* tservers = osrfNewStringArray(4);

    jsonObject* tclientsList = jsonObjectFindPath(configChunk, "/trusted_domains/client");
    jsonObject* tserversList = jsonObjectFindPath(configChunk, "/trusted_domains/server");

	int i;

    if(tserversList->type == JSON_ARRAY) {
	    for( i = 0; i != tserversList->size; i++ ) {
            char* serverDomain = jsonObjectGetString(jsonObjectGetIndex(tserversList, i));
		    osrfLogInfo( OSRF_LOG_MARK,  "Router adding trusted server: %s", serverDomain);
            osrfStringArrayAdd(tservers, serverDomain);
        }
    } else {
        char* serverDomain = jsonObjectGetString(tserversList);
        osrfLogInfo( OSRF_LOG_MARK,  "Router adding trusted server: %s", serverDomain);
        osrfStringArrayAdd(tservers, serverDomain);
    }

    if(tclientsList->type == JSON_ARRAY) {
	    for( i = 0; i != tclientsList->size; i++ ) {
            char* clientDomain = jsonObjectGetString(jsonObjectGetIndex(tclientsList, i));
		    osrfLogInfo( OSRF_LOG_MARK,  "Router adding trusted client: %s", clientDomain);
            osrfStringArrayAdd(tclients, clientDomain);
        }
    } else {
        char* clientDomain = jsonObjectGetString(tclientsList);
        osrfLogInfo( OSRF_LOG_MARK,  "Router adding trusted client: %s", clientDomain);
        osrfStringArrayAdd(tclients, clientDomain);
    }


	if( tclients->size == 0 || tservers->size == 0 ) {
		osrfLogError( OSRF_LOG_MARK, "We need trusted servers and trusted client to run the router...");
		osrfStringArrayFree( tservers );
		osrfStringArrayFree( tclients );
		return -1;
	}

	router = osrfNewRouter( server,
			username, resource, password, iport, tclients, tservers );
	
	signal(SIGHUP,routerSignalHandler);
	signal(SIGINT,routerSignalHandler);
	signal(SIGTERM,routerSignalHandler);

	if( (osrfRouterConnect(router)) != 0 ) {
		fprintf(stderr, "Unable to connect router to jabber server %s... exiting\n", server );
		osrfRouterFree(router);
		return -1;
	}

	daemonize();
	osrfRouterRun( router );

	// Shouldn't get here, since osrfRouterRun()
	// should go into an infinite loop

	osrfRouterFree(router);
	router = NULL;
	
	return -1;
}


