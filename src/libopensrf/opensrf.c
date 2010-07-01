#include "opensrf/osrf_system.h"

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

	if( argc < 4 ) {
		fprintf(stderr, "Usage: %s <host> <bootstrap_config> <config_context>\n", argv[0]);
		return 1;
	}

	/* these must be strdup'ed because init_proc_title / set_proc_title
		are evil and overwrite the argv memory */
	char* host      = strdup( argv[1] );
	char* config    = strdup( argv[2] );
	char* context   = strdup( argv[3] );

	if( argv[4] )
		osrfSystemSetPidFile( argv[4] );

	init_proc_title( argc, argv );
	set_proc_title( "OpenSRF System-C" );

	int ret = osrfSystemBootstrap( host, config, context );

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

	return ret;
}


