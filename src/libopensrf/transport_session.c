#include <opensrf/transport_session.h>

/**
	@file transport_session.c
	@brief Routines to manage a connection to a Jabber server.

	In all cases, a transport_session acts as a client with regard to Jabber.
*/

#define CONNECTING_1 1   /**< just starting the connection to Jabber */
#define CONNECTING_2 2   /**< XML stream opened but not yet logged in */


/* Note. these are growing buffers, so all that's necessary is a sane starting point */
#define JABBER_BODY_BUFSIZE    4096  /**< buffer size for message body */
#define JABBER_SUBJECT_BUFSIZE   64  /**< buffer size for message subject */
#define JABBER_THREAD_BUFSIZE    64  /**< buffer size for message thread */
#define JABBER_JID_BUFSIZE       64  /**< buffer size for various ids */
#define JABBER_STATUS_BUFSIZE    16  /**< buffer size for status code */

// ---------------------------------------------------------------------------------
// Callback for handling the startElement event.  Much of the jabber logic occurs
// in this and the characterHandler callbacks.
// Here we check for the various top level jabber elements: body, iq, etc.
// ---------------------------------------------------------------------------------
static void startElementHandler(
		void *session, const xmlChar *name, const xmlChar **atts);

// ---------------------------------------------------------------------------------
// Callback for handling the endElement event.  Updates the Jabber state machine
// to let us know the element is over.
// ---------------------------------------------------------------------------------
static void endElementHandler( void *session, const xmlChar *name);

// ---------------------------------------------------------------------------------
// This is where we extract XML text content.  In particular, this is useful for
// extracting Jabber message bodies.
// ---------------------------------------------------------------------------------
static void characterHandler(
		void *session, const xmlChar *ch, int len);

static void parseWarningHandler( void *session, const char* msg, ... );
static void parseErrorHandler( void *session, const char* msg, ... );

// ---------------------------------------------------------------------------------
// Tells the SAX parser which functions will be used as event callbacks
// ---------------------------------------------------------------------------------
static xmlSAXHandler SAXHandlerStruct = {
	NULL,                  /* internalSubset */
	NULL,                  /* isStandalone */
	NULL,                  /* hasInternalSubset */
	NULL,                  /* hasExternalSubset */
	NULL,                  /* resolveEntity */
	NULL,                  /* getEntity */
	NULL,                  /* entityDecl */
	NULL,                  /* notationDecl */
	NULL,                  /* attributeDecl */
	NULL,                  /* elementDecl */
	NULL,                  /* unparsedEntityDecl */
	NULL,                  /* setDocumentLocator */
	NULL,                  /* startDocument */
	NULL,                  /* endDocument */
	startElementHandler,   /* startElement */
	endElementHandler,     /* endElement */
	NULL,                  /* reference */
	characterHandler,      /* characters */
	NULL,                  /* ignorableWhitespace */
	NULL,                  /* processingInstruction */
	NULL,                  /* comment */
	parseWarningHandler,   /* xmlParserWarning */
	parseErrorHandler,     /* xmlParserError */
	NULL,                  /* xmlParserFatalError : unused */
	NULL,                  /* getParameterEntity */
	NULL,                  /* cdataBlock; */
	NULL,                  /* externalSubset; */
	1,
	NULL,
	NULL,                  /* startElementNs */
	NULL,                  /* endElementNs */
	NULL                   /* xmlStructuredErrorFunc */
};

// ---------------------------------------------------------------------------------
// Our SAX handler pointer.
// ---------------------------------------------------------------------------------
static const xmlSAXHandlerPtr SAXHandler = &SAXHandlerStruct;

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

static void grab_incoming(void* blob, socket_manager* mgr, int sockid, char* data, int parent);
static void reset_session_buffers( transport_session* session );
static const char* get_xml_attr( const xmlChar** atts, const char* attr_name );

/**
	@brief Allocate and initialize a transport_session.
	@param server Hostname or IP address where the Jabber server resides.
	@param port Port used for connecting to Jabber (0 if using UNIX domain socket).
	@param unix_path Name of Jabber's socket in file system (if using UNIX domain socket).
	@param user_data An opaque pointer stored on behalf of the calling code.
	@param component Boolean; true if we're a component.
	@return Pointer to a newly allocated transport_session.

	This function initializes memory but does not open any sockets or otherwise access
	the network.

	If @a port is greater than zero, we will use TCP to connect to Jabber, and ignore
	@a unix_path.  Otherwise we will open a UNIX domain socket using @a unix_path.

	The calling code is responsible for freeing the transport_session by calling
	session_free().
*/
transport_session* init_transport( const char* server,
	int port, const char* unix_path, void* user_data, int component ) {

	if( ! server )
		server = "";

	/* create the session struct */
	transport_session* session =
		(transport_session*) safe_malloc( sizeof(transport_session) );

	session->user_data = user_data;

	session->component = component;

	/* initialize the data buffers */
	session->body_buffer        = buffer_init( JABBER_BODY_BUFSIZE );
	session->subject_buffer     = buffer_init( JABBER_SUBJECT_BUFSIZE );
	session->thread_buffer      = buffer_init( JABBER_THREAD_BUFSIZE );
	session->from_buffer        = buffer_init( JABBER_JID_BUFSIZE );
	session->status_buffer      = buffer_init( JABBER_STATUS_BUFSIZE );
	session->recipient_buffer   = buffer_init( JABBER_JID_BUFSIZE );
	session->message_error_type = buffer_init( JABBER_JID_BUFSIZE );
	session->session_id         = buffer_init( 64 );

	session->message_error_code = 0;

	/* for OpenSRF extensions */
	session->router_to_buffer   = buffer_init( JABBER_JID_BUFSIZE );
	session->router_from_buffer = buffer_init( JABBER_JID_BUFSIZE );
	session->osrf_xid_buffer    = buffer_init( JABBER_JID_BUFSIZE );
	session->router_class_buffer    = buffer_init( JABBER_JID_BUFSIZE );
	session->router_command_buffer  = buffer_init( JABBER_JID_BUFSIZE );

	session->router_broadcast   = 0;

	/* initialize the jabber state machine */
	session->state_machine = (jabber_machine*) safe_malloc( sizeof(jabber_machine) );
	session->state_machine->connected        = 0;
	session->state_machine->connecting       = 0;
	session->state_machine->in_message       = 0;
	session->state_machine->in_message_body  = 0;
	session->state_machine->in_thread        = 0;
	session->state_machine->in_subject       = 0;
	session->state_machine->in_error         = 0;
	session->state_machine->in_message_error = 0;
	session->state_machine->in_iq            = 0;
	session->state_machine->in_presence      = 0;
	session->state_machine->in_status        = 0;

	/* initialize the sax push parser */
	session->parser_ctxt = xmlCreatePushParserCtxt(SAXHandler, session, "", 0, NULL);

	/* initialize the socket_manager structure */
	session->sock_mgr = (socket_manager*) safe_malloc( sizeof(socket_manager) );

	session->sock_mgr->data_received = &grab_incoming;
	session->sock_mgr->on_socket_closed = NULL;
	session->sock_mgr->socket = NULL;
	session->sock_mgr->blob = session;

	session->port = port;
	session->server = strdup(server);
	if(unix_path)
		session->unix_path = strdup(unix_path);
	else session->unix_path = NULL;

	session->sock_id = 0;
	session->message_callback = NULL;

	return session;
}

/**
	@brief Disconnect from Jabber, destroy a transport_session, and close its socket.
	@param session Pointer to the transport_session to be destroyed.
	@return 1 if successful, or 0 if not.

	The only error condition is a NULL pointer argument.
*/
int session_free( transport_session* session ) {
	if( ! session )
		return 0;
	else {
		session_disconnect( session );
		return session_discard( session );
	}
}

/**
	@brief Destroy a transport_session and close its socket, without disconnecting from Jabber.
	@param session Pointer to the transport_session to be destroyed.
	@return 1 if successful, or 0 if not.

	This function may be called from a child process in order to free resources associated
	with the parent's transport_session, but without sending a disconnect to Jabber (since
	that would disconnect the parent).

	The only error condition is a NULL pointer argument.
*/
int session_discard( transport_session* session ) {
	if( ! session )
		return 0;

	if(session->sock_mgr)
		socket_manager_free(session->sock_mgr);

	if( session->state_machine ) free( session->state_machine );
	if( session->parser_ctxt) {
		xmlFreeDoc( session->parser_ctxt->myDoc );
		xmlFreeParserCtxt(session->parser_ctxt);
	}

	xmlCleanupCharEncodingHandlers();
	xmlDictCleanup();
	xmlCleanupParser();

	buffer_free(session->body_buffer);
	buffer_free(session->subject_buffer);
	buffer_free(session->thread_buffer);
	buffer_free(session->from_buffer);
	buffer_free(session->recipient_buffer);
	buffer_free(session->status_buffer);
	buffer_free(session->message_error_type);
	buffer_free(session->router_to_buffer);
	buffer_free(session->router_from_buffer);
	buffer_free(session->osrf_xid_buffer);
	buffer_free(session->router_class_buffer);
	buffer_free(session->router_command_buffer);
	buffer_free(session->session_id);

	free(session->server);
	free(session->unix_path);

	free( session );
	return 1;
}

/**
	@brief Determine whether a transport_session is connected.
	@param session Pointer to the transport_session to be tested.
	@return 1 if connected, or 0 if not.
*/
int session_connected( transport_session* session ) {
	return session ? session->state_machine->connected : 0;
}

/**
	@brief Wait on the client socket connected to Jabber, and process any resulting input.
	@param session Pointer to the transport_session.
	@param timeout How seconds to wait before timing out (see notes).
	@return 0 if successful, or -1 if a timeout or other error occurs, or if the server
		closes the connection at the other end.

	If @a timeout is -1, wait indefinitely for input activity to appear.  If @a timeout is
	zero, don't wait at all.  If @a timeout is positive, wait that number of seconds
	before timing out.  If @a timeout has a negative value other than -1, the results are not
	well defined.

	Read all available input from the socket and pass it through grab_incoming() (a
	callback function previously installed in the socket_manager).

	There is no guarantee that we will get a complete message from a single call.  As a
	result, the calling code should call this function in a loop until it gets a complete
	message, or until an error occurs.
*/
int session_wait( transport_session* session, int timeout ) {
	if( ! session || ! session->sock_mgr ) {
		return 0;
	}

	int ret =  socket_wait( session->sock_mgr, timeout, session->sock_id );

	if( ret ) {
		osrfLogDebug(OSRF_LOG_MARK, "socket_wait returned error code %d", ret);
		session->state_machine->connected = 0;
	}
	return ret;
}

/**
	@brief Convert a transport_message to XML and send it to Jabber.
	@param session Pointer to the transport_session.
	@param msg Pointer to a transport_message enclosing the message.
	@return 0 if successful, or -1 upon error.
*/
int session_send_msg(
		transport_session* session, transport_message* msg ) {

	if( ! session ) { return -1; }

	if( ! session->state_machine->connected ) {
		osrfLogWarning(OSRF_LOG_MARK, "State machine is not connected in send_msg()");
		return -1;
	}

	message_prepare_xml( msg );
	return socket_send( session->sock_id, msg->msg_xml );

}


/**
	@brief Connect to the Jabber server as a client and open a Jabber session.
	@param session Pointer to a transport_session.
	@param username Jabber user name.
	@param password Jabber password.
	@param resource name of Jabber resource.
	@param connect_timeout Timeout interval, in seconds, for receiving data (see notes).
	@param auth_type An enum: either AUTH_PLAIN or AUTH_DIGEST (see notes).
	@return 1 if successful, or 0 upon error.

	If @a connect_timeout is -1, wait indefinitely for the Jabber server to respond.  If
	@a connect_timeout is zero, don't wait at all.  If @a timeout is positive, wait that
	number of seconds before timing out.  If @a connect_timeout has a negative value other
	than -1, the results are not well defined.

	The value of @a connect_timeout applies to each of two stages in the logon procedure.
	Hence the logon may take up to twice the amount of time indicated.

	If we connect as a Jabber component, we send the password as an SHA1 hash.  Otherwise
	we look at the @a auth_type.  If it's AUTH_PLAIN, we send the password as plaintext; if
	it's AUTH_DIGEST, we send it as a hash.

	At this writing, we only use AUTH_DIGEST.
*/
int session_connect( transport_session* session,
		const char* username, const char* password,
		const char* resource, int connect_timeout, enum TRANSPORT_AUTH_TYPE auth_type ) {

	// Sanity checks
	if( ! session ) {
		osrfLogWarning(OSRF_LOG_MARK, "session is null in session_connect()" );
		return 0;
	}

	if( session->sock_id != 0 ) {
		osrfLogWarning(OSRF_LOG_MARK, "transport session is already open, on socket %d",
			session->sock_id );
		return 0;
	}

	// Open a client socket connecting to the Jabber server
	if(session->port > 0) {   // use TCP
		session->sock_id = socket_open_tcp_client(
				session->sock_mgr, session->port, session->server );
		if( session->sock_id <= 0 ) {
			session->sock_id = 0;
			return 0;
		}
	} else if(session->unix_path != NULL) {  // use UNIX domain
		session->sock_id = socket_open_unix_client( session->sock_mgr, session->unix_path );
		if( session->sock_id <= 0 ) {
			session->sock_id = 0;
			return 0;
		}
	}
	else {
		osrfLogWarning( OSRF_LOG_MARK, "Can't open session: no port or unix path" );
		return 0;
	}

	const char* server = session->server;
	int size1 = 0;
	int size2 = 0;

	/*
	We establish the session in two stages.

	First we establish an XMPP stream with the Jabber server by sending an opening tag of
	stream:stream.  This is not a complete XML document.  We don't send the corresponding
	closing tag until we close the session.

	If the Jabber server responds by sending an opening stream:stream tag of its own, we can
	proceed to the second stage by sending a second stanza to log in.  This stanza is an XML
	element with the tag <handshake> (if we're a Jabber component) or <iq> (if we're not),
	enclosing the username, password, and resource.

	If all goes well, the Jabber server responds with a <handshake> or <iq> stanza of its own,
	and we're logged in.

	If authentication fails, the Jabber server returns a <stream:error> (if we used a <handshake>
	or an <iq> of type "error" (if we used an <iq>).
	*/
	if( session->component ) {

		/* the first Jabber connect stanza */
		char our_hostname[HOST_NAME_MAX + 1] = "";
		gethostname(our_hostname, sizeof(our_hostname) );
		our_hostname[HOST_NAME_MAX] = '\0';
		size1 = 150 + strlen( username ) + strlen( our_hostname );
		char stanza1[ size1 ];
		snprintf( stanza1, sizeof(stanza1),
				"<stream:stream version='1.0' xmlns:stream='http://etherx.jabber.org/streams' "
				"xmlns='jabber:component:accept' to='%s' from='%s' xml:lang='en'>",
				username, our_hostname );

		/* send the first stanze */
		session->state_machine->connecting = CONNECTING_1;

		if( socket_send( session->sock_id, stanza1 ) ) {
			osrfLogWarning(OSRF_LOG_MARK, "error sending");
			socket_disconnect( session->sock_mgr, session->sock_id );
			session->sock_id = 0;
			return 0;
		}

		/* wait for reply */
		socket_wait(session->sock_mgr, connect_timeout, session->sock_id);

		/* server acknowledges our existence, now see if we can login */
		if( session->state_machine->connecting == CONNECTING_2 ) {

			int ss = buffer_length( session->session_id ) + strlen( password ) + 5;
			char hashstuff[ss];
			snprintf( hashstuff, sizeof(hashstuff), "%s%s",
					OSRF_BUFFER_C_STR( session->session_id ), password );

			char* hash = shahash( hashstuff );
			size2 = 100 + strlen( hash );
			char stanza2[ size2 ];
			snprintf( stanza2, sizeof(stanza2), "<handshake>%s</handshake>", hash );

			if( socket_send( session->sock_id, stanza2 )  ) {
				osrfLogWarning(OSRF_LOG_MARK, "error sending");
				socket_disconnect( session->sock_mgr, session->sock_id );
				session->sock_id = 0;
				return 0;
			}
		}

	} else { /* we're not a component */

		/* the first Jabber connect stanza */
		size1 = 100 + strlen( server );
		char stanza1[ size1 ];
		snprintf( stanza1, sizeof(stanza1),
				"<stream:stream to='%s' xmlns='jabber:client' "
				"xmlns:stream='http://etherx.jabber.org/streams'>",
			server );

		/* send the first stanze */
		session->state_machine->connecting = CONNECTING_1;
		if( socket_send( session->sock_id, stanza1 ) ) {
			osrfLogWarning(OSRF_LOG_MARK, "error sending");
			socket_disconnect( session->sock_mgr, session->sock_id );
			session->sock_id = 0;
			return 0;
		}


		/* wait for reply */
		socket_wait( session->sock_mgr, connect_timeout, session->sock_id ); /* make the timeout smarter XXX */

		if( auth_type == AUTH_PLAIN ) {

			/* the second jabber connect stanza including login info*/
			size2 = 150 + strlen( username ) + strlen( password ) + strlen( resource );
			char stanza2[ size2 ];
			snprintf( stanza2, sizeof(stanza2),
					"<iq id='123456789' type='set'><query xmlns='jabber:iq:auth'>"
					"<username>%s</username><password>%s</password><resource>%s</resource></query></iq>",
					username, password, resource );

			/* server acknowledges our existence, now see if we can login */
			if( session->state_machine->connecting == CONNECTING_2 ) {
				if( socket_send( session->sock_id, stanza2 )  ) {
					osrfLogWarning(OSRF_LOG_MARK, "error sending");
					socket_disconnect( session->sock_mgr, session->sock_id );
					session->sock_id = 0;
					return 0;
				}
			}

		} else if( auth_type == AUTH_DIGEST ) {

			int ss = buffer_length( session->session_id ) + strlen( password ) + 5;
			char hashstuff[ss];
			snprintf( hashstuff, sizeof(hashstuff), "%s%s", OSRF_BUFFER_C_STR( session->session_id ), password );

			char* hash = shahash( hashstuff );

			/* the second jabber connect stanza including login info */
			size2 = 150 + strlen( username ) + strlen( hash ) + strlen(resource);
			char stanza2[ size2 ];
			snprintf( stanza2, sizeof(stanza2),
					"<iq id='123456789' type='set'><query xmlns='jabber:iq:auth'>"
					"<username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>",
					username, hash, resource );

			/* server acknowledges our existence, now see if we can login */
			if( session->state_machine->connecting == CONNECTING_2 ) {
				if( socket_send( session->sock_id, stanza2 )  ) {
					osrfLogWarning(OSRF_LOG_MARK, "error sending");
					socket_disconnect( session->sock_mgr, session->sock_id );
					session->sock_id = 0;
					return 0;
				}
			}

		} else {
			osrfLogWarning(OSRF_LOG_MARK, "Invalid auth_type parameter: %d",
					(int) auth_type );
			socket_disconnect( session->sock_mgr, session->sock_id );
			session->sock_id = 0;
			return 0;
		}

	} // not component


	/* wait for reply to login request */
	socket_wait( session->sock_mgr, connect_timeout, session->sock_id );

	if( session->state_machine->connected ) {
		/* yar! */
		return 1;
	} else {
		socket_disconnect( session->sock_mgr, session->sock_id );
		session->sock_id = 0;
		return 0;
	}
}

/**
	@brief Callback function: push a buffer of XML into an XML parser.
	@param blob Void pointer pointing to the transport_session.
	@param mgr Pointer to the socket_manager (not used).
	@param sockid Socket file descriptor (not used)
	@param data Pointer to a buffer of received data, as a nul-terminated string.
	@param parent Not applicable.

	The socket_manager calls this function when it reads a buffer's worth of data from
	the Jabber socket.  The XML parser calls other callback functions when it sees various
	features of the XML.
*/
static void grab_incoming(void* blob, socket_manager* mgr, int sockid, char* data, int parent) {
	transport_session* ses = (transport_session*) blob;
	if( ! ses ) { return; }
	xmlParseChunk(ses->parser_ctxt, data, strlen(data), 0);
}


/**
	@brief Respond to the beginning of an XML element.
	@param session Pointer to the transport_session, cast to a void pointer.
	@param name Name of the XML element.
	@param atts Pointer to a ragged array containing attributes and values.

	The XML parser calls this when it sees the beginning of an XML element.  We note what
	element it is by setting the corresponding switch in the state machine, and grab whatever
	attributes we expect to find.
*/
static void startElementHandler(
	void *session, const xmlChar *name, const xmlChar **atts) {

	transport_session* ses = (transport_session*) session;
	if( ! ses ) { return; }


	if( strcmp( (char*) name, "message" ) == 0 ) {
		ses->state_machine->in_message = 1;
		buffer_add( ses->from_buffer, get_xml_attr( atts, "from" ) );
		buffer_add( ses->recipient_buffer, get_xml_attr( atts, "to" ) );
		buffer_add( ses->router_from_buffer, get_xml_attr( atts, "router_from" ) );
		buffer_add( ses->osrf_xid_buffer, get_xml_attr( atts, "osrf_xid" ) );
		buffer_add( ses->router_to_buffer, get_xml_attr( atts, "router_to" ) );
		buffer_add( ses->router_class_buffer, get_xml_attr( atts, "router_class" ) );
		buffer_add( ses->router_command_buffer, get_xml_attr( atts, "router_command" ) );
		const char* broadcast = get_xml_attr( atts, "broadcast" );
		if( broadcast )
			ses->router_broadcast = atoi( broadcast );

		return;
	}

	if( ses->state_machine->in_message ) {

		if( strcmp( (char*) name, "body" ) == 0 ) {
			ses->state_machine->in_message_body = 1;
			return;
		}

		if( strcmp( (char*) name, "subject" ) == 0 ) {
			ses->state_machine->in_subject = 1;
			return;
		}

		if( strcmp( (char*) name, "thread" ) == 0 ) {
			ses->state_machine->in_thread = 1;
			return;
		}

	}

	if( strcmp( (char*) name, "presence" ) == 0 ) {
		ses->state_machine->in_presence = 1;
		buffer_add( ses->from_buffer, get_xml_attr( atts, "from" ) );
		buffer_add( ses->recipient_buffer, get_xml_attr( atts, "to" ) );
		return;
	}

	if( strcmp( (char*) name, "status" ) == 0 ) {
		ses->state_machine->in_status = 1;
		return;
	}


	if( strcmp( (char*) name, "stream:error" ) == 0 ) {
		ses->state_machine->in_error = 1;
		ses->state_machine->connected = 0;
		osrfLogWarning(  OSRF_LOG_MARK, "Received <stream:error> message from Jabber server" );
		return;
	}


	/* first server response from a connect attempt */
	if( strcmp( (char*) name, "stream:stream" ) == 0 ) {
		if( ses->state_machine->connecting == CONNECTING_1 ) {
			ses->state_machine->connecting = CONNECTING_2;
			buffer_add( ses->session_id, get_xml_attr(atts, "id") );
		}
		return;
	}

	if( strcmp( (char*) name, "handshake" ) == 0 ) {
		ses->state_machine->connected = 1;
		ses->state_machine->connecting = 0;
		return;
	}


	if( strcmp( (char*) name, "error" ) == 0 ) {
		ses->state_machine->in_message_error = 1;
		buffer_add( ses->message_error_type, get_xml_attr( atts, "type" ) );
		ses->message_error_code = atoi( get_xml_attr( atts, "code" ) );
		osrfLogInfo( OSRF_LOG_MARK, "Received <error> message with type %s and code %d",
			OSRF_BUFFER_C_STR( ses->message_error_type ), ses->message_error_code );
		return;
	}

	if( strcmp( (char*) name, "iq" ) == 0 ) {
		ses->state_machine->in_iq = 1;

		const char* type = get_xml_attr(atts, "type");

		if( strcmp( type, "result") == 0
				&& ses->state_machine->connecting == CONNECTING_2 ) {
			ses->state_machine->connected = 1;
			ses->state_machine->connecting = 0;
			return;
		}

		if( strcmp( type, "error") == 0 ) {
			osrfLogWarning( OSRF_LOG_MARK,  "Error connecting to jabber" );
			return;
		}
	}
}

/**
	@brief Return the value of a given XML attribute.
	@param atts Pointer to a NULL terminated array of strings.
	@param attr_name Name of the attribute you're looking for.
	@return The value of the attribute if found, or NULL if not.

	In the array to which @a atts points, the zeroth entry is an attribute name, and the
	one after that is its value.  Subsequent entries alternate between names and values.
	The last entry is NULL to terminate the list.
*/
static const char* get_xml_attr( const xmlChar** atts, const char* attr_name ) {
	int i;
	if (atts != NULL) {
		for(i = 0;(atts[i] != NULL);i++) {
			if( strcmp( (const char*) atts[i++], attr_name ) == 0 ) {
				if( atts[i] != NULL ) {
					return (const char*) atts[i];
				}
			}
		}
	}
	return NULL;
}


/**
	@brief React to the closing of an XML tag.
	@param session Pointer to a transport_session, cast to a void pointer.
	@param name Pointer to the name of the tag that is closing.

	See what kind of tag is closing, and respond accordingly.
*/
static void endElementHandler( void *session, const xmlChar *name) {
	transport_session* ses = (transport_session*) session;
	if( ! ses ) { return; }

	// Bypass a level of indirection, since we'll examine the machine repeatedly:
	jabber_machine* machine = ses->state_machine;

	if( machine->in_message && strcmp( (char*) name, "message" ) == 0 ) {

		/* pass off the message info the callback */
		if( ses->message_callback ) {

			transport_message* msg =  message_init(
				OSRF_BUFFER_C_STR( ses->body_buffer ),
				OSRF_BUFFER_C_STR( ses->subject_buffer ),
				OSRF_BUFFER_C_STR( ses->thread_buffer ),
				OSRF_BUFFER_C_STR( ses->recipient_buffer ),
				OSRF_BUFFER_C_STR( ses->from_buffer ) );

			message_set_router_info( msg,
				ses->router_from_buffer->buf,
				ses->router_to_buffer->buf,
				ses->router_class_buffer->buf,
				ses->router_command_buffer->buf,
				ses->router_broadcast );

			message_set_osrf_xid( msg, ses->osrf_xid_buffer->buf );

			if( ses->message_error_type->n_used > 0 ) {
				set_msg_error( msg, ses->message_error_type->buf, ses->message_error_code );
			}

			if( msg == NULL ) { return; }
			ses->message_callback( ses->user_data, msg );
		}

		machine->in_message = 0;
		reset_session_buffers( session );
		return;
	}

	if( machine->in_message_body && strcmp( (const char*) name, "body" ) == 0 ) {
		machine->in_message_body = 0;
		return;
	}

	if( machine->in_subject && strcmp( (const char*) name, "subject" ) == 0 ) {
		machine->in_subject = 0;
		return;
	}

	if( machine->in_thread && strcmp( (const char*) name, "thread" ) == 0 ) {
		machine->in_thread = 0;
		return;
	}

	if( machine->in_iq && strcmp( (const char*) name, "iq" ) == 0 ) {
		machine->in_iq = 0;
		if( ses->message_error_code > 0 ) {
			if( 401 == ses->message_error_code )
				osrfLogWarning( OSRF_LOG_MARK, "Error 401 in IQ packet: not authorized" );
			else
				osrfLogWarning( OSRF_LOG_MARK, "Error in IQ packet: code %d",
						ses->message_error_code );
		}
		reset_session_buffers( session );
		return;
	}

	if( machine->in_presence && strcmp( (const char*) name, "presence" ) == 0 ) {
		machine->in_presence = 0;
		/*
		if( ses->presence_callback ) {
			// call the callback with the status, etc.
		}
		*/
		reset_session_buffers( session );
		return;
	}

	if( machine->in_status && strcmp( (const char*) name, "status" ) == 0 ) {
		machine->in_status = 0;
		return;
	}

	if( machine->in_message_error && strcmp( (const char*) name, "error" ) == 0 ) {
		machine->in_message_error = 0;
		return;
	}

	if( machine->in_error && strcmp( (const char*) name, "stream:error" ) == 0 ) {
		machine->in_error = 0;
		return;
	}
}

/**
	@brief Clear all the buffers of a transport_session.
	@param ses Pointer to the transport_session whose buffers are to be cleared.
*/
static void reset_session_buffers( transport_session* ses ) {
	OSRF_BUFFER_RESET( ses->body_buffer );
	OSRF_BUFFER_RESET( ses->subject_buffer );
	OSRF_BUFFER_RESET( ses->thread_buffer );
	OSRF_BUFFER_RESET( ses->from_buffer );
	OSRF_BUFFER_RESET( ses->recipient_buffer );
	OSRF_BUFFER_RESET( ses->router_from_buffer );
	OSRF_BUFFER_RESET( ses->osrf_xid_buffer );
	OSRF_BUFFER_RESET( ses->router_to_buffer );
	OSRF_BUFFER_RESET( ses->router_class_buffer );
	OSRF_BUFFER_RESET( ses->router_command_buffer );
	OSRF_BUFFER_RESET( ses->message_error_type );
	OSRF_BUFFER_RESET( ses->session_id );
	OSRF_BUFFER_RESET( ses->status_buffer );
}

// ------------------------------------------------------------------
// takes data out of the body of the message and pushes it into
// the appropriate buffer
// ------------------------------------------------------------------
/**
	@brief Copy XML text (outside of tags) into the appropriate buffer.
	@param session Pointer to the transport_session.
	@param ch Pointer to the text to be copied.
	@param len How many characters to be copied.

	The XML parser calls this as a callback when it finds text outside of a tag,  We check
	the state machine to figure out what kind of text it is, and then append it to the
	corresponding buffer.
*/
static void characterHandler(
		void *session, const xmlChar *ch, int len) {

	const char* p = (const char*) ch;

	transport_session* ses = (transport_session*) session;
	if( ! ses ) { return; }

	jabber_machine* machine = ses->state_machine;

	/* set the various message parts */
	if( machine->in_message ) {

		if( machine->in_message_body ) {
			buffer_add_n( ses->body_buffer, p, len );
		}

		if( machine->in_subject ) {
			buffer_add_n( ses->subject_buffer, p, len );
		}

		if( machine->in_thread ) {
			buffer_add_n( ses->thread_buffer, p, len );
		}
	}

	/* set the presence status */
	if( machine->in_presence && ses->state_machine->in_status ) {
		buffer_add_n( ses->status_buffer, p, len );
	}

	if( machine->in_error ) {
		char msg[ len + 1 ];
		strncpy( msg, p, len );
		msg[ len ] = '\0';
		osrfLogWarning( OSRF_LOG_MARK,
			"Text of error message received from Jabber: %s", msg );
	}
}

/**
	@brief Log a warning from the XML parser.
	@param session Pointer to a transport_session, cast to a void pointer (not used).
	@param msg Pointer to a printf-style format string.  Subsequent messages, if any, are
		formatted and inserted into the expanded string.

	The XML parser calls this function when it wants to warn about something in the XML.
*/
static void  parseWarningHandler( void *session, const char* msg, ... ) {
	VA_LIST_TO_STRING(msg);
	osrfLogWarning( OSRF_LOG_MARK, VA_BUF );
}

/**
	@brief Log an error from the XML parser.
	@param session Pointer to a transport_session, cast to a void pointer (not used).
	@param msg Pointer to a printf-style format string.  Subsequent messages, if any, are
		formatted and inserted into the expanded string.

	The XML parser calls this function when it finds an error in the XML.
*/
static void  parseErrorHandler( void *session, const char* msg, ... ){
	VA_LIST_TO_STRING(msg);
	osrfLogError( OSRF_LOG_MARK, VA_BUF );
}

/**
	@brief Disconnect from Jabber, and close the socket.
	@param session Pointer to the transport_session to be disconnected.
	@return 0 in all cases.
*/
int session_disconnect( transport_session* session ) {
	if( session && session->sock_id != 0 ) {
		socket_send(session->sock_id, "</stream:stream>");
		socket_disconnect(session->sock_mgr, session->sock_id);
		session->sock_id = 0;
	}
	return 0;
}

