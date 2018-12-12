#include <opensrf/osrf_app_session.h>
#include <opensrf/osrf_application.h>
#include <opensrf/osrf_json.h>
#include <opensrf/log.h>

#define MODULENAME "opensrf.cslow"

int osrfAppInitialize();
int osrfAppChildInit();
int osrfCSlowWait( osrfMethodContext* );


int osrfAppInitialize() {

	osrfAppRegisterMethod( 
			MODULENAME, 
			"opensrf.cslow.wait", 
			"osrfCSlowWait", 
			"Wait specified number of seconds, then return that number", 1, 0 );

	return 0;
}

int osrfAppChildInit() {
	return 0;
}

int osrfCSlowWait( osrfMethodContext* ctx ) {
	if( osrfMethodVerifyContext( ctx ) ) {
		osrfLogError( OSRF_LOG_MARK,  "Invalid method context" );
		return -1;
	}

	const jsonObject* x = jsonObjectGetIndex(ctx->params, 0);

	if( x ) {

		char* a = jsonObjectToSimpleString(x);

		if( a ) {

			unsigned int pause = atoi(a);
			sleep(pause);

			jsonObject* resp = jsonNewNumberObject(pause);
			osrfAppRespondComplete( ctx, resp );
			jsonObjectFree(resp);

			free(a);
			return 0;
		}
	}

	return -1;
}



