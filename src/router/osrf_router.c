#include "osrf_router.h"

/**
	@file osrf_router.c
	@brief Implementation of osrfRouter.

	The router opens multiple Jabber sessions for the same username and domain, one for
	each server class.  The Jabber IDs for these sessions are distinguished by the use of
	the class names as Jabber resource names.

	For each server class there may be multiple server nodes.
*/

/* a router maintains a list of server classes */

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
	osrfHashIterator* class_itr;  /**< For traversing the list of classes */
	char* domain;         /**< Domain name of Jabber server. */
	char* name;           /**< Router's username for the Jabber logon. */
	char* resource;       /**< Router's resource name for the Jabber logon. */
	char* password;       /**< Router's password for the Jabber logon. */
	int port;             /**< Jabber's port number. */
	sig_atomic_t stop;    /**< To be set by signal handler to interrupt main loop */

	/** Array of client domains that we allow to send requests through us. */
	osrfStringArray* trustedClients;
	/** Array of server domains that we allow to register, etc. with us. */
	osrfStringArray* trustedServers;

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
static void osrfRouterHandleCommand( osrfRouter* router, transport_message* msg );
static int osrfRouterClassHandleMessage( osrfRouter* router,
		osrfRouterClass* rclass, transport_message* msg );
static void osrfRouterRemoveClass( osrfRouter* router, const char* classname );
static int osrfRouterClassRemoveNode( osrfRouter* router, const char* classname,
		const char* remoteId );
static void osrfRouterClassFree( char* classname, void* rclass );
static void osrfRouterNodeFree( char* remoteId, void* node );
static osrfRouterClass* osrfRouterFindClass( osrfRouter* router, const char* classname );
static osrfRouterNode* osrfRouterClassFindNode( osrfRouterClass* rclass,
		const char* remoteId );
static int _osrfRouterFillFDSet( osrfRouter* router, fd_set* set );
static void osrfRouterHandleIncoming( osrfRouter* router );
static int osrfRouterClassHandleIncoming( osrfRouter* router,
		const char* classname,  osrfRouterClass* class );
static transport_message* osrfRouterClassHandleBounce( osrfRouter* router,
		const char* classname, osrfRouterClass* rclass, transport_message* msg );
static void osrfRouterHandleAppRequest( osrfRouter* router, transport_message* msg );
static int osrfRouterRespondConnect( osrfRouter* router, transport_message* msg,
		osrfMessage* omsg );
static int osrfRouterProcessAppRequest( osrfRouter* router, transport_message* msg,
		osrfMessage* omsg );
static int osrfRouterHandleAppResponse( osrfRouter* router,
		transport_message* msg, osrfMessage* omsg, const jsonObject* response );
static int osrfRouterHandleMethodNFound( osrfRouter* router, transport_message* msg,
		osrfMessage* omsg );

#define ROUTER_REGISTER "register"
#define ROUTER_UNREGISTER "unregister"

#define ROUTER_REQUEST_CLASS_LIST "opensrf.router.info.class.list"
#define ROUTER_REQUEST_STATS_NODE_FULL "opensrf.router.info.stats.class.node.all"
#define ROUTER_REQUEST_STATS_CLASS_FULL "opensrf.router.info.stats.class.all"
#define ROUTER_REQUEST_STATS_CLASS "opensrf.router.info.stats.class"
#define ROUTER_REQUEST_STATS_CLASS_SUMMARY "opensrf.router.info.stats.class.summary"

/**
	@brief Stop the otherwise infinite main loop of the router.
	@param router Pointer to the osrfRouter to be stopped.

	To be called by a signal handler.
*/
void router_stop( osrfRouter* router )
{
	if( router )
		router->stop = 1;
}

/**
	@brief Allocate and initialize a new osrfRouter.
	@param domain Domain name of Jabber server
	@param name Router's username for the Jabber logon.
	@param resource Router's resource name for the Jabber logon.
	@param password Router's password for the Jabber logon.
	@param port Jabber's port number.
	@param trustedClients Array of client domains that we allow to send requests through us.
	@param trustedServers Array of server domains that we allow to register, etc. with us.
	@return Pointer to the newly allocated osrfRouter.

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

	// Prepare to connect to Jabber, as a non-component, over TCP (not UNIX domain).
	router->connection = client_init( domain, port, NULL, 0 );

	return router;
}

/**
	@brief Connect to Jabber.
	@param router Pointer to the osrfRouter to connect to Jabber
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
	@return 

	On each iteration: wait for incoming messages to arrive on any of our sockets -- i.e.
	either the top level socket belong to the router or any of the lower level sockets
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
		int numhandled = 0;

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
			numhandled++;
			osrfRouterHandleIncoming( router );
		}

		/* now check each of the connected classes and see if they have data to route */
		while( numhandled < selectret ) {

			osrfRouterClass* class;
			osrfHashIterator* itr = router->class_itr;  // remove a layer of indirection
			osrfHashIteratorReset( itr );

			while( (class = osrfHashIteratorNext(itr)) ) {

				const char* classname = osrfHashIteratorKey(itr);

				if( classname ) {

					osrfLogDebug( OSRF_LOG_MARK, "Checking %s for activity...", classname );

					int sockfd = client_sock_fd( class->connection );
					if(FD_ISSET( sockfd, &set )) {
						osrfLogDebug( OSRF_LOG_MARK, "Socket is active: %d", sockfd );
						numhandled++;
						osrfRouterClassHandleIncoming( router, classname, class );
					}
				}
			} // end while
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

			/* if the sender is not on a trusted domain, drop the message */
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
	@brief Handle incoming requests to a router class.
	@param router Pointer to the osrfRouter.
	@param classname Class name.
	@param class Pointer to an osrfRouterClass.

	Make sure sender is a trusted client.
*/
static int osrfRouterClassHandleIncoming( osrfRouter* router, const char* classname,
		osrfRouterClass* class ) {
	if(!(router && class)) return -1;

	transport_message* msg;
	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleIncoming()");

	while( (msg = client_recv( class->connection, 0 )) ) {

		osrfLogSetXid(msg->osrf_xid);

		if( msg->sender ) {

			osrfLogDebug(OSRF_LOG_MARK,
				"osrfRouterClassHandleIncoming(): investigating message from %s", msg->sender);

			/* if the client is not from a trusted domain, drop the message */
			int len = strlen(msg->sender) + 1;
			char domain[len];
			memset(domain, 0, sizeof(domain));
			jid_get_domain( msg->sender, domain, len - 1 );

			if(osrfStringArrayContains( router->trustedClients, domain)) {

				transport_message* bouncedMessage = NULL;
				if( msg->is_error )  {

					/* handle bounced message */
					if( !(bouncedMessage = osrfRouterClassHandleBounce( router, classname,
							class, msg )) )
						return -1; /* we have no one to send the requested message to */

					message_free( msg );
					msg = bouncedMessage;
				}
				osrfRouterClassHandleMessage( router, class, msg );

			} else {
				osrfLogWarning( OSRF_LOG_MARK, 
						"Received client message from untrusted client domain %s", domain );
			}
		}

		osrfLogClearXid();
		message_free( msg );
	}

	return 0;
}

/**
	@brief Handle a top level router command.
	@param router Pointer to the osrfRouter.
	@param msg Pointer to the transport_message to be handled.

	Currently supported commands:
	- "register" -- Add a server class and/or a server node to our lists.
	- "unregister" -- Remove a server class (and any associated nodes) from our list.
*/
static void osrfRouterHandleCommand( osrfRouter* router, transport_message* msg ) {
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
	@brief Adds an osrfRouterClass to a router, and open a connection for it.
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
	@param rclass Pointer to the osrfRouterClass to add the node to.
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

/* copy off the lastMessage, remove the offending node, send error if it's tht last node
	? return NULL if it's the last node ?
*/

/* handles case where router node is not longer reachable.  copies over the
	data from the last sent message and returns a newly crafted suitable for treating
	as a newly inconing message.  Removes the dead node and If there are no more
	nodes to send the new message to, returns NULL.
*/
static transport_message* osrfRouterClassHandleBounce( osrfRouter* router,
		const char* classname, osrfRouterClass* rclass, transport_message* msg ) {

	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleBounce()");

	osrfLogInfo( OSRF_LOG_MARK, "Received network layer error message from %s", msg->sender );
	osrfRouterNode* node = osrfRouterClassFindNode( rclass, msg->sender );
	transport_message* lastSent = NULL;

	if( node && osrfHashGetCount(rclass->nodes) == 1 ) { /* the last node is dead */

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

		return NULL;

	} else {

		if( node ) {
			if( node->lastMessage ) {
				osrfLogDebug( OSRF_LOG_MARK, "Cloning lastMessage so next node can send it");
				lastSent = message_init( node->lastMessage->body,
					node->lastMessage->subject, node->lastMessage->thread, "", node->lastMessage->router_from );
				message_set_router_info( lastSent, node->lastMessage->router_from, NULL, NULL, NULL, 0 );
			message_set_osrf_xid( lastSent, node->lastMessage->osrf_xid );
			}
		} else {

			osrfLogInfo(OSRF_LOG_MARK, "network error occurred after we removed the class.. ignoring");
			return NULL;
		}
	}

	/* remove the dead node */
	osrfRouterClassRemoveNode( router, classname, msg->sender);
	return lastSent;
}


/*
	Handles class level requests
	If we get a regular message, we send it to the next node in the list of nodes
	if we get an error, it's a bounce back from a previous attempt.  We take the
	body and thread from the last sent on the node that had the bounced message
	and propogate them on to the new message being sent
	@return 0 on success
*/
static int osrfRouterClassHandleMessage(
		osrfRouter* router, osrfRouterClass* rclass, transport_message* msg ) {
	if(!(router && rclass && msg)) return -1;

	osrfLogDebug( OSRF_LOG_MARK, "osrfRouterClassHandleMessage()");

	osrfRouterNode* node = osrfHashIteratorNext( rclass->itr );
	if(!node) {
		osrfHashIteratorReset(rclass->itr);
		node = osrfHashIteratorNext( rclass->itr );
	}

	if(node) {

		transport_message* new_msg= message_init( msg->body,
				msg->subject, msg->thread, node->remoteId, msg->sender );
		message_set_router_info( new_msg, msg->sender, NULL, NULL, NULL, 0 );
		message_set_osrf_xid( new_msg, msg->osrf_xid );

		osrfLogInfo( OSRF_LOG_MARK,  "Routing message:\nfrom: [%s]\nto: [%s]",
				new_msg->router_from, new_msg->recipient );

		message_free( node->lastMessage );
		node->lastMessage = new_msg;

		if ( client_send_message( rclass->connection, new_msg ) == 0 )
			node->count++;

		else {
			message_prepare_xml(new_msg);
			osrfLogWarning( OSRF_LOG_MARK, "Error sending message from %s to %s\n%s",
					new_msg->sender, new_msg->recipient, new_msg->msg_xml );
		}

	}

	return 0;
}


/**
	@brief Remove a given osrfRouterClass from an osrfRouter
	@param router Pointer to the osrfRouter.
	@param classname The name of the class to be removed.

	A callback function, installed in the osrfHash, frees the osrfRouterClass and any
	associated nodes.
*/
static void osrfRouterRemoveClass( osrfRouter* router, const char* classname ) {
	if( router && router->classes && classname ) {
		osrfLogInfo( OSRF_LOG_MARK, "Removing router class %s", classname );
		osrfHashRemove( router->classes, classname );
	}
}


/*
	Removes the given node from the class.  Also, if this is that last node in the set,
	removes the class from the router
	@return 0 on successful removal with no class removal
	@return 1 on successful remove with class removal
	@return -1 error on removal
*/
static int osrfRouterClassRemoveNode(
		osrfRouter* router, const char* classname, const char* remoteId ) {

	if(!(router && router->classes && classname && remoteId)) return 0;

	osrfLogInfo( OSRF_LOG_MARK, "Removing router node %s", remoteId );

	osrfRouterClass* class = osrfRouterFindClass( router, classname );

	if( class ) {

		osrfHashRemove( class->nodes, remoteId );
		if( osrfHashGetCount(class->nodes) == 0 ) {
			osrfRouterRemoveClass( router, classname );
			return 1;
		}

		return 0;
	}

	return -1;
}


/**
	@brief Free a router class object.
	@param classname Class name.
	@param c Pointer to the osrfRouterClass, cast to a void pointer.

	This function is designated to the osrfHash as a callback.
*/
static void osrfRouterClassFree( char* classname, void* c ) {
	if(!(classname && c)) return;
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
	@param remoteId Name of router (not used).
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

	client_free( router->connection );
	free(router);
}



/*
	Finds the class associated with the given class name in the list of classes
*/
static osrfRouterClass* osrfRouterFindClass( osrfRouter* router, const char* classname ) {
	if(!( router && router->classes && classname )) return NULL;
	return (osrfRouterClass*) osrfHashGet( router->classes, classname );
}


/*
	Finds the router node within this class with the given remote id
*/
static osrfRouterNode* osrfRouterClassFindNode( osrfRouterClass* rclass,
		const char* remoteId ) {
	if(!(rclass && remoteId))  return NULL;
	return (osrfRouterNode*) osrfHashGet( rclass->nodes, remoteId );
}


/*
	Clears and populates the provided fd_set* with file descriptors
	from the router's top level connection as well as each of the
	router class connections
	@return The largest file descriptor found in the filling process
*/
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
	osrfHashIterator* itr = osrfNewHashIterator(router->classes);

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

	osrfHashIteratorFree(itr);
	return maxfd;
}

/*
	handles messages that don't have a 'router_command' set.  They are assumed to
	be app request messages
*/
static void osrfRouterHandleAppRequest( osrfRouter* router, transport_message* msg ) {

	int T = 32;
	osrfMessage* arr[T];
	memset(arr, 0, sizeof(arr));

	int num_msgs = osrf_message_deserialize( msg->body, arr, T );
	osrfMessage* omsg = NULL;

	int i;
	for( i = 0; i != num_msgs; i++ ) {

		if( !(omsg = arr[i]) ) continue;

		switch( omsg->m_type ) {

			case CONNECT:
				osrfRouterRespondConnect( router, msg, omsg );
				break;

			case REQUEST:
				osrfRouterProcessAppRequest( router, msg, omsg );
				break;

			default: break;
		}

		osrfMessageFree( omsg );
	}

	return;
}

static int osrfRouterRespondConnect( osrfRouter* router, transport_message* msg,
		osrfMessage* omsg ) {
	if(!(router && msg && omsg)) return -1;

	osrfMessage* success = osrf_message_init( STATUS, omsg->thread_trace, omsg->protocol );

	osrfLogDebug( OSRF_LOG_MARK, "router received a CONNECT message from %s", msg->sender );

	osrf_message_set_status_info(
		success, "osrfConnectStatus", "Connection Successful", OSRF_STATUS_OK );

	char* data = osrf_message_serialize(success);

	transport_message* return_m = message_init(
		data, "", msg->thread, msg->sender, "" );

	client_send_message(router->connection, return_m);

	free(data);
	osrfMessageFree(success);
	message_free(return_m);

	return 0;
}



static int osrfRouterProcessAppRequest( osrfRouter* router, transport_message* msg,
		osrfMessage* omsg ) {

	if(!(router && msg && omsg && omsg->method_name)) return -1;

	osrfLogInfo( OSRF_LOG_MARK, "Router received app request: %s", omsg->method_name );

	jsonObject* jresponse = NULL;
	if(!strcmp( omsg->method_name, ROUTER_REQUEST_CLASS_LIST )) {

		int i;
		jresponse = jsonNewObjectType(JSON_ARRAY);

		osrfStringArray* keys = osrfHashKeys( router->classes );
		for( i = 0; i != keys->size; i++ )
			jsonObjectPush( jresponse, jsonNewObject(osrfStringArrayGetString( keys, i )) );
		osrfStringArrayFree(keys);


	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS_SUMMARY )) {

		osrfRouterClass* class;
		osrfRouterNode* node;
		int count = 0;

		char* classname = jsonObjectToSimpleString( jsonObjectGetIndex( omsg->_params, 0 ) );

		if (!classname)
			return -1;

		class = osrfHashGet(router->classes, classname);
		free(classname);

		osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
		while( (node = osrfHashIteratorNext(node_itr)) ) {
			count += node->count;
			//jsonObjectSetKey( class_res, node->remoteId, jsonNewNumberObject( (double) node->count ) );
		}
		osrfHashIteratorFree(node_itr);

		jresponse = jsonNewNumberObject( (double) count );

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS )) {

		osrfRouterClass* class;
		osrfRouterNode* node;

		char* classname = jsonObjectToSimpleString( jsonObjectGetIndex( omsg->_params, 0 ) );

		if (!classname)
			return -1;

		jresponse = jsonNewObjectType(JSON_HASH);
		class = osrfHashGet(router->classes, classname);
		free(classname);

		osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
		while( (node = osrfHashIteratorNext(node_itr)) ) {
			jsonObjectSetKey( jresponse, node->remoteId,
					jsonNewNumberObject( (double) node->count ) );
		}
		osrfHashIteratorFree(node_itr);

	} else if(!strcmp( omsg->method_name, ROUTER_REQUEST_STATS_CLASS_FULL )) {

		osrfRouterClass* class;
		osrfRouterNode* node;
		jresponse = jsonNewObjectType(JSON_HASH);

		osrfHashIterator* class_itr = osrfNewHashIterator(router->classes);
		while( (class = osrfHashIteratorNext(class_itr)) ) {

			jsonObject* class_res = jsonNewObjectType(JSON_HASH);
			const char* classname = osrfHashIteratorKey(class_itr);

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

		osrfRouterClass* class;
		osrfRouterNode* node;
		int count;
		jresponse = jsonNewObjectType(JSON_HASH);

		osrfHashIterator* class_itr = osrfNewHashIterator(router->classes);
		while( (class = osrfHashIteratorNext(class_itr)) ) {

			count = 0;
			const char* classname = osrfHashIteratorKey(class_itr);

			osrfHashIterator* node_itr = osrfNewHashIterator(class->nodes);
			while( (node = osrfHashIteratorNext(node_itr)) ) {
				count += node->count;
			}
			osrfHashIteratorFree(node_itr);

			jsonObjectSetKey( jresponse, classname, jsonNewNumberObject( (double) count ) );
		}

		osrfHashIteratorFree(class_itr);

	} else {

		return osrfRouterHandleMethodNFound( router, msg, omsg );
	}


	osrfRouterHandleAppResponse( router, msg, omsg, jresponse );
	jsonObjectFree(jresponse);

	return 0;

}


static int osrfRouterHandleMethodNFound(
		osrfRouter* router, transport_message* msg, osrfMessage* omsg ) {

	osrfMessage* err = osrf_message_init( STATUS, omsg->thread_trace, 1);
		osrf_message_set_status_info( err,
				"osrfMethodException", "Router method not found", OSRF_STATUS_NOTFOUND );

		char* data =  osrf_message_serialize(err);

		transport_message* tresponse = message_init(
				data, "", msg->thread, msg->sender, msg->recipient );

		client_send_message(router->connection, tresponse );

		free(data);
		osrfMessageFree( err );
		message_free(tresponse);
		return 0;
}


static int osrfRouterHandleAppResponse( osrfRouter* router,
	transport_message* msg, osrfMessage* omsg, const jsonObject* response ) {

	if( response ) { /* send the response message */

		osrfMessage* oresponse = osrf_message_init(
				RESULT, omsg->thread_trace, omsg->protocol );

		char* json = jsonObjectToJSON(response);
		osrf_message_set_result_content( oresponse, json);

		char* data =  osrf_message_serialize(oresponse);
		osrfLogDebug( OSRF_LOG_MARK,  "Responding to client app request with data: \n%s\n", data );

		transport_message* tresponse = message_init(
				data, "", msg->thread, msg->sender, msg->recipient );

		client_send_message(router->connection, tresponse );

		osrfMessageFree(oresponse);
		message_free(tresponse);
		free(json);
		free(data);
	}

	/* now send the 'request complete' message */
	osrfMessage* status = osrf_message_init( STATUS, omsg->thread_trace, 1);
	osrf_message_set_status_info( status, "osrfConnectStatus", "Request Complete",
			OSRF_STATUS_COMPLETE );

	char* statusdata = osrf_message_serialize(status);

	transport_message* sresponse = message_init(
			statusdata, "", msg->thread, msg->sender, msg->recipient );
	client_send_message(router->connection, sresponse );

	free(statusdata);
	osrfMessageFree(status);
	message_free(sresponse);

	return 0;
}




