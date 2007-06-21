#include <opensrf/osrf_system.h>
//#include <opensrf/osrf_hash.h>
//#include <opensrf/osrf_list.h>

int main( int argc, char* argv[] ) {

	if( argc < 4 ) {
		fprintf(stderr, "Usage: %s <host> <bootstrap_config> <config_context>\n", argv[0]);
		return 1;
	}

	fprintf(stderr, "Loading OpenSRF host %s with bootstrap config %s "
			"and config context %s\n", argv[1], argv[2], argv[3] );

	/* these must be strdup'ed because init_proc_title / set_proc_title 
		are evil and overwrite the argv memory */
	char* host		= strdup( argv[1] );
	char* config	= strdup( argv[2] );
	char* context	= strdup( argv[3] );

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


