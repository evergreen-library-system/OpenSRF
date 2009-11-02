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

/* a router maintains a list of server classes */
struct _osrfRouterStruct {

	osrfHash* classes;    /**< our list of server classes */
	char* domain;         /**< Domain name of Jabber server. */
	char* name;           /**< Router's username for the Jabber logon. */
	char* resource;       /**< Router's resource name for the Jabber logon. */
	char* password;       /**< Router's password for the Jabber logon. */
	int port;             /**< Jabber's port number. */
	sig_atomic_t stop;    /**< To be set by signal handler to interrupt main loop */

	osrfStringArray* trustedClients;
	osrfStringArray* trustedServers;

	transport_client* connection;
};

typedef struct _osrfRouterStruct osrfRouter;

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

/**
  Connects the given router to the network
  */
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

/**
  Handles connects, disconnects, etc.
  */
//int osrfRouterHandeStatusMessage( osrfRouter* router, transport_message* msg );

/**
  Handles REQUEST messages 
  */
//int osrfRouterHandleRequestMessage( osrfRouter* router, transport_message* msg );

#ifdef __cplusplus
}
#endif

#endif

