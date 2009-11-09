#ifndef OSRF_ROUTER_H
#define OSRF_ROUTER_H

/**
	@file 
	@brief Collection of routines for an OSRF router.

	The router receives messages from clients and passes each one to a listener for the
	targeted service.  Where there are multiple listeners for the same service, the router
	picks one on a round-robin basis.  If a message bounces because the listener has died,
	the router sends it to another listener for the same service, if one is available.

	The server's response to the client, if any, bypasses the router.  If the server needs to
	set up a stateful session with a client, it does so directly (well, via Jabber).  Only the
	initial message from the client passes through the router.

	Thus the router serves two main functions:
	- It spreads the load across multiple listeners for the same service.
	- It reroutes bounced messages to alternative listeners.

	It also responds to requests for information about the number of messages routed to
	different services and listeners.
*/

/*
	Prerequisite:

		string_array.h
*/
#ifdef __cplusplus
extern "C" {
#endif

struct osrfRouterStruct;
typedef struct osrfRouterStruct osrfRouter;

osrfRouter* osrfNewRouter( const char* domain, const char* name, const char* resource,
	const char* password, int port, osrfStringArray* trustedClients,
	osrfStringArray* trustedServers );

int osrfRouterConnect( osrfRouter* router );

void osrfRouterRun( osrfRouter* router );

void router_stop( osrfRouter* router );

void osrfRouterFree( osrfRouter* router );

#ifdef __cplusplus
}
#endif

#endif

