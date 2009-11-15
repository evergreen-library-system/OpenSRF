#include <sys/select.h>
#include <signal.h>
#include "opensrf/utils.h"
#include "opensrf/log.h"
#include "opensrf/osrf_list.h"
#include "opensrf/string_array.h"
#include "opensrf/osrf_hash.h"
#include "osrf_router.h"
#include "opensrf/transport_client.h"
#include "opensrf/transport_message.h"
#include "opensrf/osrf_message.h"

/**
	@file osrf_router.c
	@brief Implementation of osrfRouter.

	The router opens multiple Jabber sessions for the same username and domain, one for
	each server class.  The Jabber IDs for these sessions are distinguished by the use of
	the class names as Jabber resource names.

	For each server class there may be multiple server nodes.  Each node corresponds to a
	listener process for a service.
*/

/**
	@brief Collection of server classes, with connection parameters for Jabber.
 */
struct osrfRouterStruct {

	/** 
		@brief Hash store of server classes.
	
		For each entry, the key is the class name, and the corresponding datum is an
		osrfRouterClass.
	*/
	osrfHash* classes;
	osrfHashIterator* class_itr;  /**< For traversing the list of classes. */
	char* domain;         /**< Domain name of Jabber server. */
	char* name;           /**< Router's username for the Jabber logon. */
	char* resource;       /**< Router's resource name for the Jabber logon. */
	char* password;       /**< Router's password for the Jabber logon. */
	int port;             /**< Jabber's port number. */
	sig_atomic_t stop;    /**< To be set by signal handler to interrupt main loop. */

	/** Array of client domains that we allow to send requests through us. */
	osrfStringArray* trustedClients;
	/** Array of server domains that we allow to register, etc. with us. */
	osrfStringArray* trustedServers;
	/** List of osrfMessages to be returned from osrfMessageDeserialize() */
	osrfList* message_list;

	transport_client* connection;
};

/**
	@brief Maintains a set of server nodes belonging to the same class.
*/
struct _osrfRouterClassStruct {
	osrfRouter* router;         /**< The osrfRouter that owns this osrfRouterClass. */
	osrfHashIterator* itr;      /**< Iterator for set of osrfRouterNodes. */
	/**
		@brief Hash store of server nodes.

		The key of each entry is a node name, and the associated data is an osrfRouterNode.
		We install a callback for freeing the entries.
	*/
	osrfHash* nodes;
	/** The transport_client used for communicating with this server. */
	transport_client* connection;
};
typedef struct _osrfRouterClassStruct osrfRouterClass;

/**
	@brief Represents a link to a single server's inbound connection.
*/
struct _osrfRouterNodeStruct {
	char* remoteId;     /**< Send message to me via this login. */
	int count;          /**< How many message have been sent to this node. */
	transport_message* lastMessage;
};
typedef struct _osrfRouterNodeStruct osrfRouterNode;

static osrfRouterClass* osrfRouterAddClass( osrfRouter* router, const char* classname );
static void osrfRouterClassAddNode( osrfRouterClass* rclass, const char* remoteId );
static void osrfRouterHandleCommand( osrfRouter* router, const transport_message* msg );
static void osrfRouterClassHandleMessage( osrfRouter* router,
		osrfRouterClass* rclass, const transport_message* msg );
static void osrfRouterRemoveClass( osrfRouter* router, const char* classname );
static void osrfRouterClassRemoveNode( osrfRouter* router, const char* classname,
		const char* remoteId );
static void osrfRouterClassFree( char* classname, void* rclass );
static void osrfRouterNodeFree( char* remoteId, void* node );
static osrfRouterClass* osrfRouterFindClass( osrfRouter* router, const char* classname );
static osrfRouterNode* osrfRouterClassFindNode( osrfRouterClass* rclass,
		const char* remoteId );
static int _osrfRouterFillFDSet( osrfRouter* router, fd_set* set );
static void osrfRouterHandleIncoming( osrfRouter* router );
static void osrfRouterClassHandleIncoming( osrfRouter* router,
		const char* classname,  osrfRouterClass* class );
static transport_message* osrfRouterClassHandleBounce( osrfRouter* router,
		const char* classname, osrfRouterClass* rclass, const transport_message* msg );
static void osrfRouterHandleAppRequest( osrfRouter* router, const transport_message* msg );
static void osrfRouterRespondConnect( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg );
static void osrfRouterProcessAppRequest( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg );
static void osrfRouterSendAppResponse( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg, const jsonObject* response );
static void osrfRouterHandleMethodNFound( osrfRouter* router,
		const transport_message* msg, const osrfMessage* omsg );

#define ROUTER_REGISTER "register"
#define ROUTER_UNREGISTER "unregister"

#define ROUTER_REQUEST_CLASS_LIST "opensrf.router.info.class.list"
#define ROUTER_REQUEST_STATS_NODE_FULL "opensrf.router.info.stats.class.node.all"
#define ROUTER_REQUEST_STATS_CLASS_FULL "opensrf.router.info.stats.class.all"
#define ROUTER_REQUEST_STATS_CLASS "opensrf.router.info.stats.class"
#define ROUTER_REQUEST_STATS_CLASS_SUMMARY "opensrf.router.info.stats.class.summary"

/**
	@brief Stop the otherwise endless main loop of the router.
	@param router Pointer to the osrfRouter to be stopped.

	To be called by a signal handler.  We don't stop the loop immediately; we just set
	a switch that the main loop checks on each iteration.
*/
void router_stop( osrfRouter* router )
{
	if( router )
		router->stop = 1;
}

/**
	@brief Allocate and initialize a new osrfRouter.
	@param domain Domain name of Jabber server.
	@param name Router's username for the Jabber logon.
	@param resource Router's resource name for the Jabber logon.
	@param password Router's password for the Jabber logon.
	@param port Jabber's port number.
	@param trustedClients Array of client domains that we allow to send requests through us.
	@param trustedServers Array of server domains that we allow to register, etc. with us.
	@return Pointer to the newly allocated osrfRouter, or NULL upon error.

	Don't connect to Jabber yet.  We'll do that later, upon a call to osrfRouterConnect().

	The calling code is responsible for freeing the osrfRouter by calling osrfRouterFree().
*/
osrfRouter* osrfNewRouter(
		const char* domain, const char* name,
		const char* resource, const char* password, int port,
		osrfStringArray* trustedClients, osrfStringArray* trustedServers ) {

	if(!( domain && name && resource && password && port && trustedClients && trustedServers ))
		return NULL;

	osrfRouter* router     = safe_malloc(sizeof(osrfRouter));
	router->domain         = strdup(domain);
	router->name           = strdup(name);
	router->password       = strdup(password);
	router->resource       = strdup(resource);
	router->port           = port;
	router->stop           = 0;

	router->trustedClients = trustedClients;
	router->trustedServers = trustedServers;


	router->classes = osrfNewHash();
	osrfHashSetCallback(router->classes, &osrfRouterClassFree);
	router->class_itr = osrfNewHashIterator( router->classes );
	router->message_list = NULL;   // We'll allocate one later

	// Prepare to connect to Jabber, as a non-component, over TCP (not UNIX domain).
	router->connection = client_init( domain, port, NULL, 0 );

	return router;
}

/**
	@brief Connect to Jabber.
	@param router Pointer to the osrfRouter to connect to Jabber.
	@return 0 if successful, or -1 on error.

	Allow up to 10 seconds for the logon to succeed.

	We connect over TCP (not over a UNIX domain), as a non-component.
*/
int osrfRouterConnect( osrfRouter* router ) {
	if(!router) return -1;
	int ret = client_connect( router->connection, router->name,
			router->password, router->resource, 10, AUTH_DIGEST );
	if( ret == 0 ) return -1;
	return 0;
}

/**
	@brief Enter endless loop to receive and respond to input.
	@param router Pointer to the osrfRouter that's looping.

	On each iteration: wait for incoming messages to arrive on any of our sockets -- i.e.
	either the top level socket belonging to the router or any of the lower level sockets
	belonging to the classes.  React to the incoming activity as needed.

	We don't exit the loop until we receive a signal to stop, or until we encounter an error.
*/
void osrfRouterRun( osrfRouter* router ) {
	if(!(router && router->classes)) return;

	int routerfd = client_sock_fd( router->connection );
	int selectret = 0;

	// Loop until a signal handler sets router->stop
	while( ! router->stop ) {

		fd_set set;
		int maxfd = _osrfRouterFillFDSet( router, &set );

		// Wait indefinitely for an incoming message
		if( (selectret = select(maxfd + 1, &set, NULL, NULL, NULL)) < 0 ) {
			if( EINTR == errno ) {
				if( router->stop ) {
					osrfLogWarning( OSRF_LOG_MARK, "Top level select call interrupted by signal" );
					break;
				}
				else
					continue;    // Irrelevant signal; ignore it
			} else {
				osrfLogWarning( OSRF_LOG_MARK, "Top level select call failed with errno %d: %s",
						errno, strerror( errno ) );
				break;
			}
		}

		/* see if there is a top level router message */
		if( FD_ISSET(routerfd, &set) ) {
			osrfLogDebug( OSRF_LOG_MARK, "Top router socket is active: %d", routerfd );
			osrfRouterHandleIncoming( router );
		}

		/* Check each of the connected classes and see if they have data to route */
		osrfRouterClass* class;
		osrfHashIterator* itr = router->class_itr;  // remove a layer of indirection
		osrfHashIteratorReset( itr );

		while( (class = osrfHashIteratorNext(itr)) ) {   // for each class

			const char* classname = osrfHashIteratorKey(itr);

			if( classname ) {

				osrfLogDebug( OSRF_LOG_MARK, "Checking %s for activity...", classname );

				int sockfd = client_sock_fd( class->connection );
				if(FD_ISSET( sockfd, &set )) {
					osrfLogDebug( OSRF_LOG_MARK, "Socket is active: %d", sockfd );
					osrfRouterClassHandleIncoming( router, classname, class );
				}
			}
		} // end while
	} // end while
}


/**
	@brief Handle incoming requests to the router.
	@param router Pointer to the osrfRouter.

	Read all available input messages from the top-level transport_client.  For each one:
	if the domain of the sender's Jabber id is on the list of approved domains, pass the
	message to osrfRouterHandleCommand() (if there's a message) or osrfRouterHandleAppRequest()
	(if there isn't).  If the domain is @em not on the approved list, log a warning and
	discard the message.
*/
static void osrfRouterHandleIncoming( osrfRouter* router ) {
	if(!router) return;

	transport_message* msg = NULL;

	while( (msg = client_recv( router->connection, 0 )) ) {  // for each message

		if( msg->sender ) {

			osrfLogDebug(OSRF_LOG_MARK,
				"osrfRouterHandleIncoming(): investigating message from %s", msg->sender);

			/* if the server is not on a trusted domain, drop the message */
			int len = strlen(msg->sender) + 1;
			char domain[len];
			jid_get_domain( msg->sender, domain, len - 1 );

			if(osrfStringArrayContains( router->trustedServers, domain)) {

				// If there's a command, obey it.  Otherwise, treat
				// the message as an app session level request.
				if( msg->router_command && *msg->router_command )
					osrfRouterHandleCommand( router, msg );
				else
					osrfRouterHandleAppRequest( router, msg );
			}
			else
				osrfLogWarning( OSRF_LOG_MARK, 
						"Received message from un-trusted server domain %s", msg->sender);
		}

		message_free(msg);
	}
}

/**
	@brief Handle all available incoming messages for a router class.
	@param router Pointer to the osrfRouter.
	@param classname Class name.
	@param class Pointer to the osrfRouterClass.

	For each message: if the sender is on a trusted domain, process the message.
*/
static void osrfRouterClassHandleIncoming( osrfRouter* router, const char* classname,
		osrfRouterClass* class ) {
	if(!(router && class)) return;

	transport_message* msg;
	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleIncoming()");

	// For each incoming message for this class:
	while( (msg = client_recv( class->connection, 0 )) ) {

		// Save the transaction id so that we can incorporate it
		// into any relevant messages
		osrfLogSetXid(msg->osrf_xid);

		if( msg->sender ) {

			osrfLogDebug(OSRF_LOG_MARK,
				"osrfRouterClassHandleIncoming(): investigating message from %s", msg->sender);

			/* if the client is not from a trusted domain, drop the message */
			int len = strlen(msg->sender) + 1;
			char domain[len];
			jid_get_domain( msg->sender, domain, len - 1 );

			if(osrfStringArrayContains( router->trustedClients, domain )) {

				if( msg->is_error )  {

					// A previous message bounced.  Try to send a clone of it to a
					// different node of the same class.
					transport_message* bouncedMessage = osrfRouterClassHandleBounce(
							router, classname, class, msg );
					/* handle bounced message */
					if( !bouncedMessage ) {
						/* we have no one to send the requested message to */
						message_free( msg );
						osrfLogClearXid();
						continue;
					}
					osrfRouterClassHandleMessage( router, class, bouncedMessage );
					message_free( bouncedMessage );
				} else
					osrfRouterClassHandleMessage( router, class, msg );

			} else {
				osrfLogWarning( OSRF_LOG_MARK, 
						"Received client message from untrusted client domain %s", domain );
			}
		}

		message_free( msg );
		osrfLogClearXid();  // We're done with this transaction id
	}
}

/**
	@brief Handle a top level router command.
	@param router Pointer to the osrfRouter.
	@param msg Pointer to the transport_message to be handled.

	Currently supported commands:
	- "register" -- Add a server class and/or a server node to our lists.
	- "unregister" -- Remove a node from a class, and the class as well if no nodes are
	left for it.
*/
static void osrfRouterHandleCommand( osrfRouter* router, const transport_message* msg ) {
	if(!(router && msg && msg->router_class)) return;

	if( !strcmp( msg->router_command, ROUTER_REGISTER ) ) {

		osrfLogInfo( OSRF_LOG_MARK, "Registering class %s", msg->router_class );

		// Add the server class to the list, if it isn't already there
		osrfRouterClass* class = osrfRouterFindClass( router, msg->router_class );
		if(!class)
			class = osrfRouterAddClass( router, msg->router_class );

		// Add the node to the osrfRouterClass's list, if it isn't already there
		if(class && ! osrfRouterClassFindNode( class, msg->sender ) )
			osrfRouterClassAddNode( class, msg->sender );

	} else if( !strcmp( msg->router_command, ROUTER_UNREGISTER ) ) {

		if( msg->router_class && *msg->router_class ) {
			osrfLogInfo( OSRF_LOG_MARK, "Unregistering router class %s", msg->router_class );
			osrfRouterClassRemoveNode( router, msg->router_class, msg->sender );
		}
	}
}


/**
	@brief Add an osrfRouterClass to a router, and open a connection for it.
	@param router Pointer to the osrfRouter.
	@param classname The name of the class this node handles.
	@return A pointer to the new osrfRouterClass, or NULL upon error.

	Open a Jabber session to be used for this server class.  The Jabber ID incorporates the
	class name as the resource name.
*/
static osrfRouterClass* osrfRouterAddClass( osrfRouter* router, const char* classname ) {
	if(!(router && router->classes && classname)) return NULL;

	osrfRouterClass* class = safe_malloc(sizeof(osrfRouterClass));
	class->nodes = osrfNewHash();
	class->itr = osrfNewHashIterator(class->nodes);
	osrfHashSetCallback(class->nodes, &osrfRouterNodeFree);
	class->router = router;

	class->connection = client_init( router->domain, router->port, NULL, 0 );

	if(!client_connect( class->connection, router->name,
			router->password, classname, 10, AUTH_DIGEST ) ) {
		// Cast away the constness of classname.  Though ugly, this
		// cast is benign because osrfRouterClassFree doesn't actually
		// write through the pointer.  We can't readily change its
		// signature because it is used for a function pointer, and
		// we would have to change other signatures the same way.
		osrfRouterClassFree( (char *) classname, class );
		return NULL;
	}

	osrfHashSet( router->classes, class, classname );
	return class;
}


/**
	@brief Add a new server node to an osrfRouterClass.
	@param rclass Pointer to the osrfRouterClass to which we are to add the node.
	@param remoteId The remote login of the osrfRouterNode.
*/
static void osrfRouterClassAddNode( osrfRouterClass* rclass, const char* remoteId ) {
	if(!(rclass && rclass->nodes && remoteId)) return;

	osrfLogInfo( OSRF_LOG_MARK, "Adding router node for remote id %s", remoteId );

	osrfRouterNode* node = safe_malloc(sizeof(osrfRouterNode));
	node->count = 0;
	node->lastMessage = NULL;
	node->remoteId = strdup(remoteId);

	osrfHashSet( rclass->nodes, node, remoteId );
}

/**
	@brief Handle an input message representing a Jabber error stanza.
	@param router Pointer to the current osrfRouter.
	@param classname Name of the class to which the error stanza was sent.
	@param rclass Pointer to the osrfRouterClass to which the error stanza was sent.
	@param msg Pointer to the transport_message representing the error stanza.
	@return Pointer to a newly allocated transport_message; or NULL (see remarks).

	The presumption is that the relevant node is dead.  If another node is available for
	the same class, then remove the dead one, create a clone of the message to be sent
	elsewhere, and return a pointer to it.  If there is no other node for the same class,
	send a cancel message back to the sender, remove both the node and the class it belongs
	to, and return NULL.  If we can't even do that because the entire class is dead, log
	a message to that effect and return NULL.
*/
static transport_message* osrfRouterClassHandleBounce( osrfRouter* router,
		const char* classname, osrfRouterClass* rclass, const transport_message* msg ) {

	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleBounce()");

	osrfLogInfo( OSRF_LOG_MARK, "Received network layer error message from %s", msg->sender );
	osrfRouterNode* node = osrfRouterClassFindNode( rclass, msg->sender );
	if( ! node ) {
		osrfLogInfo( OSRF_LOG_MARK,
			"network error occurred after we removed the class.. ignoring");
		return NULL;
	}

	if( osrfHashGetCount(rclass->nodes) == 1 ) { /* the last node is dead */

		if( node->lastMessage ) {
			osrfLogWarning( OSRF_LOG_MARK,
					"We lost the last node in the class, responding with error and removing...");

			transport_message* error = message_init(
				node->lastMessage->body, node->lastMessage->subject,
				node->lastMessage->thread, node->lastMessage->router_from,
				node->lastMessage->recipient );
			message_set_osrf_xid(error, node->lastMessage->osrf_xid);
			set_msg_error( error, "cancel", 501 );

			/* send the error message back to the original sender */
			client_send_message( rclass->connection, error );
			message_free( error );
		}

		/* remove the dead node */
		osrfRouterClassRemoveNode( router, classname, msg->sender);
		return NULL;

	} else {

		transport_message* lastSent = NULL;

		if( node->lastMessage ) {
			osrfLogDebug( OSRF_LOG_MARK, "Cloning lastMessage so next node can send it");
			lastSent = message_init( node->lastMessage->body,
				node->lastMessage->subject, node->lastMessage->thread, "",
				node->lastMessage->router_from );
			message_set_router_info( lastSent, node->lastMessage->router_from,
				NULL, NULL, NULL, 0 );
			message_set_osrf_xid( lastSent, node->lastMessage->osrf_xid );
		}

		/* remove the dead node */
		osrfRouterClassRemoveNode( router, classname, msg->sender);
		return lastSent;
	}
}


/**
	@brief Forward a class-level message to a listener for the corresponding service.
	@param router Pointer to the current osrfRouter.
	@param rclass Pointer to the class to which the message is directed.
	@param msg Pointer to the message to be forwarded.

	Pick a node for the specified class, and forward the message to it.

	We use an iterator, stored with the class, to maintain a position in the class's list
	of nodes.  Advance the iterator to pick the next node, and if we reach the end, go
	back to the beginning of the list.
*/
static void osrfRouterClassHandleMessage(
		osrfRouter* router, osrfRouterClass* rclass, const transport_message* msg ) {
	if(!(router && rclass && msg)) return;

	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleMessage()");

	// Pick a node, in a round-robin.
	osrfRouterNode* node = osrfHashIteratorNext( rclass->itr );
	if(!node) {   // wrap around to the beginning of the list
		osrfHashIteratorReset(rclass->itr);
		node = osrfHashIteratorNext( rclass->itr );
	}

	if(node) {  // should always be true -- no class without a node

		// Build a transport message
		transport_message* new_msg = message_init( msg->body,
				msg->subject, msg->thread, node->remoteId, msg->sender );
		message_set_router_info( new_msg, msg->sender, NULL, NULL, NULL, 0 );
		message_set_osrf_xid( new_msg, msg->osrf_xid );

		osrfLogInfo( OSRF_LOG_MARK,  "Routing message:\nfrom: [%s]\nto: [%s]",
				new_msg->router_from, new_msg->recipient );

		// Save it for possible future reference
		message_free( node->lastMessage );
		node->lastMessage = new_msg;

		// Send it
		if ( client_send_message( rclass->connection, new_msg ) == 0 )
			node->count++;

		else {
			message_prepare_xml(new_msg);
			osrfLogWarning( OSRF_LOG_MARK, "Error sending message from %s to %s\n%s",
					new_msg->sender, new_msg->recipient, new_msg->msg_xml );
		}
		// We don't free new_msg here because we saved it as node->lastMessage.
	}
}


/**
	@brief Remove a given osrfRouterClass from an osrfRouter
	@param router Pointer to the osrfRouter.
	@param classname The name of the class to be removed.

	Delete an osrfRouterClass from the router's list of classes.  Indirectly (via a callback
	function installed in the osrfHash), free the osrfRouterClass and any associated nodes.
*/
static void osrfRouterRemoveClass( osrfRouter* router, const char* classname ) {
	if( router && router->classes && classname ) {
		osrfLogInfo( OSRF_LOG_MARK, "Removing router class %s", classname );
		osrfHashRemove( router->classes, classname );
	}
}


/**
	@brief Remove a node from a class.  If the class thereby becomes empty, remove it as well.
	@param router Pointer to the current osrfRouter.
	@param classname Class name.
	@param remoteId Identifier for the node to be removed.
*/
static void osrfRouterClassRemoveNode(
		osrfRouter* router, const char* classname, const char* remoteId ) {

	if(!(router && router->classes && classname && remoteId))  // sanity check
		return;

	osrfLogInfo( OSRF_LOG_MARK, "Removing router node %s", remoteId );

	osrfRouterClass* class = osrfRouterFindClass( router, classname );
	if( class ) {
		osrfHashRemove( class->nodes, remoteId );
		if( osrfHashGetCount(class->nodes) == 0 ) {
			osrfRouterRemoveClass( router, classname );
		}
	}
}


/**
	@brief Free a router class object.
	@param classname Class name (not used).
	@param c Pointer to the osrfRouterClass, cast to a void pointer.

	This function is invoked as a callback when we remove an osrfRouterClass from the
	router's list of classes.
*/
static void osrfRouterClassFree( char* classname, void* c ) {
	if( !c )
		return;
	osrfRouterClass* rclass = (osrfRouterClass*) c;
	client_disconnect( rclass->connection );
	client_free( rclass->connection );

	osrfHashIteratorReset( rclass->itr );
	osrfRouterNode* node;

	while( (node = osrfHashIteratorNext(rclass->itr)) )
		osrfHashRemove( rclass->nodes, node->remoteId );

	osrfHashIteratorFree(rclass->itr);
	osrfHashFree(rclass->nodes);

	free(rclass);
}

/**
	@brief Free an osrfRouterNode.
	@param remoteId Jabber ID of node (not used).
	@param n Pointer to the osrfRouterNode to be freed, cast to a void pointer.

	This is a callback installed in an osrfHash (the nodes member of an osrfRouterClass).
*/
static void osrfRouterNodeFree( char* remoteId, void* n ) {
	if(!n) return;
	osrfRouterNode* node = (osrfRouterNode*) n;
	free(node->remoteId);
	message_free(node->lastMessage);
	free(node);
}


/**
	@brief Free an osrfRouter and everything it owns.
	@param router Pointer to the osrfRouter to be freed.

	The osrfRouterClasses and osrfRouterNodes are freed by callback functions installed in
	the osrfHashes.
*/
void osrfRouterFree( osrfRouter* router ) {
	if(!router) return;

	osrfHashIteratorFree( router->class_itr);
	osrfHashFree(router->classes);
	free(router->domain);
	free(router->name);
	free(router->resource);
	free(router->password);

	osrfStringArrayFree( router->trustedClients );
	osrfStringArrayFree( router->trustedServers );
	osrfListFree( router->message_list );

	client_free( router->connection );
	free(router);
}


/**
	@brief Given a class name, find the corresponding osrfRouterClass.
	@param router Pointer to the osrfRouter that owns the osrfRouterClass.
	@param classname Name of the class.
	@return Pointer to a matching osrfRouterClass if found, or NULL if not.
*/
static osrfRouterClass* osrfRouterFindClass( osrfRouter* router, const char* classname ) {
	if(!( router && router->classes && classname )) return NULL;
	return (osrfRouterClass*) osrfHashGet( router->classes, classname );
}


/**
	@brief Find a given node for a given class.
	@param rclass Pointer to the osrfRouterClass in which to search.
	@param remoteId Jabber ID of the node for which to search.
	@return Pointer to the matching osrfRouterNode, if found; otherwise NULL.
*/
static osrfRouterNode* osrfRouterClassFindNode( osrfRouterClass* rclass,
		const char* remoteId ) {
	if(!(rclass && remoteId))  return NULL;
	return (osrfRouterNode*) osrfHashGet( rclass->nodes, remoteId );
}


/**
	@brief Fill an fd_set with all the sockets owned by the osrfRouter.
	@param router Pointer to the osrfRouter whose sockets are to be used.
	@param set Pointer to the fd_set that is to be filled.
	@return The largest file descriptor loaded into the fd_set; or -1 upon error.

	There's one socket for the osrfRouter as a whole, and one for each osrfRouterClass
	that belongs to it.  We load them all.
*/
static int _osrfRouterFillFDSet( osrfRouter* router, fd_set* set ) {
	if(!(router && router->classes && set)) return -1;

	FD_ZERO(set);
	int maxfd = client_sock_fd( router->connection );
	FD_SET(maxfd, set);

	int sockid;

	osrfRouterClass* class = NULL;
	osrfHashIterator* itr = router->class_itr;
	osrfHashIteratorReset( itr );

	while( (class = osrfHashIteratorNext(itr)) ) {
		const char* classname = osrfHashIteratorKey(itr);

		if( classname && (class = osrfRouterFindClass( router, classname )) ) {
			sockid = client_sock_fd( class->connection );

			if( osrfUtilsCheckFileDescriptor( sockid ) ) {

				osrfLogWarning(OSRF_LOG_MARK,
					"Removing router class '%s' because of a bad top-level file descriptor [%d]",
					classname, sockid );
				osrfRouterRemoveClass( router, classname );

			} else {
				if( sockid > maxfd ) maxfd = sockid;
				FD_SET(sockid, set);
			}
		}
	}

	return maxfd;
}

/**
	@brief Handler a router-level message that isn't a command; presumed to be an app request.
	@param router Pointer to the current osrfRouter.
	@param msg Pointer to the incoming message.

	The body of the transport_message is a JSON string, specifically an JSON array.
	Translate the JSON into a series of osrfMessages, one for each element of the JSON
	array.  Process each osrfMessage in turn.  Each message is either a CONNECT or a
	REQUEST.
*/
static void osrfRouterHandleAppRequest( osrfRouter* router, const transport_message* msg ) {

	// Translate the JSON into a list of osrfMessages
	router->message_list = osrfMessageDeserialize( msg->body, router->message_list );
	osrfMessage* omsg = NULL;

	// Process each osrfMessage
	int i;
	for( i = 0; i < router->message_list->size; ++i ) {

		omsg = osrfListGetIndex( router->message_list, i );
		if( omsg ) {

			switch( omsg->m_type ) {

				case CONNECT:
					osrfRouterRespondConnect( router, msg, omsg );
					break;

				case REQUEST:
					osrfRouterProcessAppRequest( router, msg, omsg );
					break;

				default:
					break;
			}

			osrfMessageFree( omsg );
		}
	}

	return;
}

/**
	@brief Respond to a CONNECT message.
	@param router Pointer to the current osrfRouter.
	@param msg Pointer to the transport_message that the osrfMessage came from.
	@param omsg Pointer to the osrfMessage to be processed.

	An application is trying to connect to the router.  Reply with a STATUS message
	signifying success.

	"CONNECT" is a bit of a misnomer.  We don't establish a stateful session; i.e. we
	don't retain any memory of this message.  We just confirm that the router is alive,
	and that the client has a good address for it.
*/
static void osrfRouterRespondConnect( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg ) {
	if(!(router && msg && omsg))
		return;

	osrfLogDebug( OSRF_LOG_MARK, "router received a CONNECT message from %s", msg->sender );

	// Build a success message
	osrfMessage* success = osrf_message_init( STATUS, omsg->thread_trace, omsg->protocol );
	osrf_message_set_status_info(
		success, "osrfConnectStatus", "Connection Successful", OSRF_STATUS_OK );

	// Translate the success message into JSON,
	// and then package the JSON in a transport message
	char* data = osrf_message_serialize(success);
	osrfMessageFree( success );
	transport_message* return_msg = message_init(
		data,           // message payload, translated from osrfMessage
		"",             // no subject
		msg->thread,    // same thread
		msg->sender,    // destination (client's Jabber ID)
		""              // don't send our address; client already has it
	);
	free( data );

	client_send_message( router->connection, return_msg );
	message_free( return_msg );
}


/**
	@brief Respond to a REQUEST message.
	@param router Pointer to the current osrfRouter.
	@param msg Pointer to the transport_message that the osrfMessage came from.
	@param omsg Pointer to the osrfMessage to be processed.

	Respond to an information request from an application.  Most types of request involve
	one or more counts of messages successfully routed.

	Request types currently supported:
	- "opensrf.router.info.class.list" -- list of class names.
	- "opensrf.router.info.stats.class.summary" -- total count for a specified class.
	- "opensrf.router.info.stats.class" -- count for every node of a specified class.
	- "opensrf.router.info.stats.class.all" -- count for every node of every class.
	- "opensrf.router.info.stats.class.node.all" -- total count for every class.
*/
static void osrfRouterProcessAppRequest( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg ) {

	if(!(router && msg && omsg && omsg->method_name))
		return;

	osrfLogInfo( OSRF_LOG_MARK, "Router received app request: %s", omsg->method_name );

	// Branch on the request type.  Build a jsonObject as an answer to the request.
	jsonObject* jresponse = NULL;
	if(!strcmp( omsg->method_name, ROUTER_REQUEST_CLASS_LIST )) {

		// Prepare an array of class names.
		int i;
		jresponse = jsonNewObjectType(JSON_ARRAY);

		osrfStringArray* keys = osrfHashKeys( router->classes );
		for( i = 0; i != keys->size; i++ )
			jsonObjectPush( jresponse, jsonNewObject(osrfStringArrayGetString( keys, i )) );
		osrfStringArrayFree(keys);

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS_SUMMARY )) {

		// Prepare a count of all the messages successfully routed for a given class.
		int count = 0;

		// class name is the first parameter
		const char* classname = jsonObjectGetString( jsonObjectGetIndex( omsg->_params, 0 ) );
		if (!classname)
			return;

		osrfRouterClass* class = osrfHashGet(router->classes, classname);

		// For each node: add the count to the total.
		osrfRouterNode* node;
		osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
		while( (node = osrfHashIteratorNext(node_itr)) ) {
			count += node->count;
			// jsonObjectSetKey( class_res, node->remoteId, 
			//       jsonNewNumberObject( (double) node->count ) );
		}
		osrfHashIteratorFree(node_itr);

		jresponse = jsonNewNumberObject( (double) count );

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS )) {

		// Prepare a hash for a given class.  Key: the remoteId of a node.  Datum: the
		// number of messages successfully routed for that node.

		// class name is the first parameter
		const char* classname = jsonObjectGetString( jsonObjectGetIndex( omsg->_params, 0 ) );
		if (!classname)
			return;

		jresponse = jsonNewObjectType(JSON_HASH);
		osrfRouterClass* class = osrfHashGet(router->classes, classname);

		// For each node: get the count and store it in the hash.
		osrfRouterNode* node;
		osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
		while( (node = osrfHashIteratorNext(node_itr)) ) {
			jsonObjectSetKey( jresponse, node->remoteId,
					jsonNewNumberObject( (double) node->count ) );
		}
		osrfHashIteratorFree(node_itr);

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS_FULL )) {

		// Prepare a hash of hashes, giving the message counts for each node for each class.

		osrfRouterClass* class;
		osrfRouterNode* node;
		jresponse = jsonNewObjectType(JSON_HASH);  // Key: class name.

		// Traverse the list of classes.
		osrfHashIterator* class_itr = osrfNewHashIterator(router->classes);
		while( (class = osrfHashIteratorNext(class_itr)) ) {

			jsonObject* class_res = jsonNewObjectType(JSON_HASH);  // Key: remoteId of node.
			const char* classname = osrfHashIteratorKey(class_itr);

			// Traverse the list of nodes for the current class.
			osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
			while( (node = osrfHashIteratorNext(node_itr)) ) {
				jsonObjectSetKey( class_res, node->remoteId,
						jsonNewNumberObject( (double) node->count ) );
			}
			osrfHashIteratorFree(node_itr);

			jsonObjectSetKey( jresponse, classname, class_res );
		}

		osrfHashIteratorFree(class_itr);

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_NODE_FULL )) {

		// Prepare a hash. Key: class name.  Datum: total number of successfully routed
		// messages routed for nodes of that class.

		osrfRouterClass* class;
		osrfRouterNode* node;
		jresponse = jsonNewObjectType(JSON_HASH);

		osrfHashIterator* class_itr = osrfNewHashIterator(router->classes);
		while( (class = osrfHashIteratorNext(class_itr)) ) {  // For each class

			int count = 0;
			const char* classname = osrfHashIteratorKey(class_itr);

			osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
			while( (node = osrfHashIteratorNext(node_itr)) ) {  // For each node
				count += node->count;
			}
			osrfHashIteratorFree(node_itr);

			jsonObjectSetKey( jresponse, classname, jsonNewNumberObject( (double) count ) );
		}

		osrfHashIteratorFree(class_itr);

	} else {  // None of the above

		osrfRouterHandleMethodNFound( router, msg, omsg );
		return;
	}

	// Send the result back to the requester.
	osrfRouterSendAppResponse( router, msg, omsg, jresponse );
	jsonObjectFree(jresponse);
}


/**
	@brief Respond to an invalid REQUEST message.
	@param router Pointer to the current osrfRouter.
	@param msg Pointer to the transport_message that contained the REQUEST message.
	@param omsg Pointer to the osrfMessage that contained the REQUEST.
*/
static void osrfRouterHandleMethodNFound( osrfRouter* router,
		const transport_message* msg, const osrfMessage* omsg ) {

	// Create an exception message
	osrfMessage* err = osrf_message_init( STATUS, omsg->thread_trace, 1 );
	osrf_message_set_status_info( err,
			"osrfMethodException", "Router method not found", OSRF_STATUS_NOTFOUND );

	// Translate it into JSON
	char* data =  osrf_message_serialize(err);
	osrfMessageFree( err );

	// Wrap the JSON up in a transport_message
	transport_message* tresponse = message_init(
			data, "", msg->thread, msg->sender, msg->recipient );
	free(data);

	// Send it
	client_send_message( router->connection, tresponse );
	message_free( tresponse );
}


/**
	@brief Send a response to a router REQUEST message.
	@param router Pointer to the current osrfRouter.
	@param msg Pointer to the transport_message that contained the REQUEST message.
	@param omsg Pointer to the osrfMessage that contained the REQUEST.
	@param response Pointer to the jsonObject that constitutes the response.
*/
static void osrfRouterSendAppResponse( osrfRouter* router, const transport_message* msg,
		const osrfMessage* omsg, const jsonObject* response ) {

	if( response ) { /* send the response message */

		// Turn the jsonObject into JSON, and load it into an osrfMessage
		osrfMessage* oresponse = osrf_message_init(
				RESULT, omsg->thread_trace, omsg->protocol );

		char* json = jsonObjectToJSON(response);
		osrf_message_set_result_content( oresponse, json );
		free(json);

		// Package the osrfMessage into a transport_message, and send it
		char* data =  osrf_message_serialize(oresponse);
		osrfMessageFree(oresponse);
		osrfLogDebug( OSRF_LOG_MARK,  "Responding to client app request with data: \n%s\n", data );

		transport_message* tresponse = message_init(
				data, "", msg->thread, msg->sender, msg->recipient );
		free(data);

		client_send_message(router->connection, tresponse );
		message_free(tresponse);
	}

	/* now send the 'request complete' message */
	osrfMessage* status = osrf_message_init( STATUS, omsg->thread_trace, 1);
	osrf_message_set_status_info( status, "osrfConnectStatus", "Request Complete",
			OSRF_STATUS_COMPLETE );
	char* statusdata = osrf_message_serialize(status);
	osrfMessageFree(status);

	transport_message* sresponse = message_init(
			statusdata, "", msg->thread, msg->sender, msg->recipient );
	free(statusdata);

	client_send_message(router->connection, sresponse );
	message_free(sresponse);
}
