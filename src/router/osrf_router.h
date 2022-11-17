#ifndef OSRF_ROUTER_H
#define OSRF_ROUTER_H

#include "opensrf/string_array.h"

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

typedef struct RouterStruct Router;

Router* osrfNewRouter(
    const char* domain, 
    const char* username,
    const char* password, 
    int port, 
    osrfStringArray* trusted_clients, 
    osrfStringArray* trusted_servers
);

int osrfRouterConnect(Router* router);

void osrfRouterRun(Router* router);

void osrfRouterStop(Router* router);

void osrfRouterFree(Router* router);

#ifdef __cplusplus
}
#endif

#endif

