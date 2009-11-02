#ifndef OSRF_ROUTER_H
#define OSRF_ROUTER_H

#include <sys/select.h>
#include <signal.h>
#include <stdio.h>

#include "opensrf/utils.h"
#include "opensrf/log.h"
#include "opensrf/osrf_list.h"
#include "opensrf/osrf_hash.h"

#include "opensrf/string_array.h"
#include "opensrf/transport_client.h"
#include "opensrf/transport_message.h"

#include "opensrf/osrf_message.h"

#ifdef __cplusplus
extern "C" {
#endif

struct osrfRouterStruct;
typedef struct osrfRouterStruct osrfRouter;

/**
  Allocates a new router.  
  @param domain The jabber domain to connect to
  @param name The login name for the router
  @param resource The login resource for the router
  @param password The login password for the new router
  @param port The port to connect to the jabber server on
  @param trustedClients The array of client domains that we allow to send requests through us
  @param trustedServers The array of server domains that we allow to register, etc. with ust.
  @return The allocated router or NULL on memory error
  */
osrfRouter* osrfNewRouter( const char* domain, const char* name, const char* resource, 
	const char* password, int port, osrfStringArray* trustedClients,
	osrfStringArray* trustedServers );

int osrfRouterConnect( osrfRouter* router );

/**
  Waits for incoming data to route
  If this function returns, then the router's connection to the jabber server
  has failed.
  */
void osrfRouterRun( osrfRouter* router );

void router_stop( osrfRouter* router );

/**
  Frees a router
  */
void osrfRouterFree( osrfRouter* router );

#ifdef __cplusplus
}
#endif

#endif

