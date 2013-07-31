#include "opensrf/osrf_system.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
	@brief Run an OSRF server as defined by the command line and a config file.
	@param argc Number of command line arguments, plus one.
	@param argv Ragged array of command name plus command line arguments.
	@return 0 if successful, or 1 if failure.

	Command line parameters:
	- Full network name of the host where the process is running; or 'localhost' will do.
	- Name of the configuration file; normally '/openils/conf/opensrf_core.xml'.
	- Name of an aggregate within the configuration file, containing the relevant subset
	of configuration stuff.
*/
int main( int argc, char* argv[] ) {

    char* host    = NULL;
    char* config  = NULL;
    char* context = NULL;
    char* piddir  = NULL;
    char* action  = NULL;
    char* service = NULL;
    opterr = 0;

	/* values must be strdup'ed because init_proc_title / 
     * set_proc_title are evil and overwrite the argv memory */

    int c;
    while ((c = getopt(argc, argv, "h:c:x:p:a:s:")) != -1) {
        switch (c) {
            case 'h':
                host = strdup(optarg);
                break;
            case 'c':
                config = strdup(optarg);
                break;
            case 'x':
                context = strdup(optarg);
                break;
            case 'p':
                piddir = strdup(optarg);
                break;
            case 'a':
                action = strdup(optarg);
                break;
            case 's':
                service = strdup(optarg);
                break;
            default:
                continue;
        }
    }


    if (!(host && config && context && piddir && action)) {
		fprintf(stderr, "Usage: %s -h <host> -c <config> "
            "-x <config_context> -p <piddir>\n", argv[0]);
		return 1;
	}

    // prepare the proc title hack
    init_proc_title(argc, argv);

    // make sure the service name is valid
    if (service && strlen(service) == 0) {
        free(service);
        service = NULL;
    }

    int ret = osrf_system_service_ctrl(
        host, config, context, piddir, action, service);

	if (ret != 0) {
		osrfLogError(
			OSRF_LOG_MARK,
			"Server Loop returned an error condition, exiting with %d",
			ret
		);
	}

	free(host);
	free(config);
	free(context);
    free(piddir);
    free(action);
    if (service) free(service);

	return ret;
}


