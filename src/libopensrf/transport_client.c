#include <opensrf/transport_client.h>

/**
	@file transport_client.c
	@brief Collection of routines for sending and receiving single messages over Jabber.

	These functions form an API built on top of the transport_session API.  They serve
	two main purposes:
	- They remember a Jabber ID to use when sending messages.
	- They maintain a queue of input messages that the calling code can get one at a time.
*/

static void client_message_handler( void* client, transport_message* msg );

//int main( int argc, char** argv );

/*
int main( int argc, char** argv ) {

	transport_message* recv;
	transport_message* send;

	transport_client* client = client_init( "spacely.georgialibraries.org", 5222 );

	// try to connect, allow 15 second connect timeout
	if( client_connect( client, "admin", "asdfjkjk", "system", 15 ) ) {
		printf("Connected...\n");
	} else {
		printf( "NOT Connected...\n" ); exit(99);
	}

	while( (recv = client_recv( client, -1 )) ) {

		if( recv->body ) {
			int len = strlen(recv->body);
			char buf[len + 20];
			osrf_clearbuf( buf, 0, sizeof(buf));
			sprintf( buf, "Echoing...%s", recv->body );
			send = message_init( buf, "Echoing Stuff", "12345", recv->sender, "" );
		} else {
			send = message_init( " * ECHOING * ", "Echoing Stuff", "12345", recv->sender, "" );
		}

		if( send == NULL ) { printf("something's wrong"); }
		client_send_message( client, send );

		message_free( send );
		message_free( recv );
	}

	printf( "ended recv loop\n" );

	return 0;

}
*/


/**
	@brief Allocate and initialize a transport_client.
	@param server Domain name where the Jabber server resides.
	@param port Port used for connecting to Jabber (0 if using UNIX domain socket).
	@param unix_path Name of Jabber's socket in file system (if using UNIX domain socket).
	@param component Boolean; true if we're a Jabber component.
	@return A pointer to a newly created transport_client.

	Create a transport_client with a transport_session and an empty message queue (but don't
	open a connection yet).  Install a callback function in the transport_session to enqueue
	incoming messages.

	The calling code is responsible for freeing the transport_client by calling client_free().
*/
transport_client* client_init( const char* server, int port, const char* unix_path, int component ) {

	if(server == NULL) return NULL;

	/* build and clear the client object */
	transport_client* client = safe_malloc( sizeof( transport_client) );

	/* start with an empty message queue */
	client->msg_q_head = NULL;
	client->msg_q_tail = NULL;

	/* build the session */
	client->session = init_transport( server, port, unix_path, client, component );

	client->session->message_callback = client_message_handler;
	client->error = 0;
	client->host = strdup(server);
	client->xmpp_id = NULL;

	return client;
}


/**
	@brief Open a Jabber session for a transport_client.
	@param client Pointer to the transport_client.
	@param username Jabber user name.
	@param password Password for the Jabber logon.
	@param resource Resource name for the Jabber logon.
	@param connect_timeout How many seconds to wait for the connection to open.
	@param auth_type An enum: either AUTH_PLAIN or AUTH_DIGEST (see notes).
	@return 1 if successful, or 0 upon error.

	Besides opening the Jabber session, create a Jabber ID for future use.

	If @a connect_timeout is -1, wait indefinitely for the Jabber server to respond.  If
	@a connect_timeout is zero, don't wait at all.  If @a timeout is positive, wait that
	number of seconds before timing out.  If @a connect_timeout has a negative value other
	than -1, the results are not well defined.

	The value of @a connect_timeout applies to each of two stages in the logon procedure.
	Hence the logon may take up to twice the amount of time indicated.

	If we connect as a Jabber component, we send the password as an SHA1 hash.  Otherwise
	we look at the @a auth_type.  If it's AUTH_PLAIN, we send the password as plaintext; if
	it's AUTH_DIGEST, we send it as a hash.
 */
int client_connect( transport_client* client,
		const char* username, const char* password, const char* resource,
		int connect_timeout, enum TRANSPORT_AUTH_TYPE  auth_type ) {
	if( client == NULL )
		return 0;

	// Create and store a Jabber ID
	if( client->xmpp_id )
		free( client->xmpp_id );
	client->xmpp_id = va_list_to_string( "%s@%s/%s", username, client->host, resource );

	// Open a transport_session
	return session_connect( client->session, username,
			password, resource, connect_timeout, auth_type );
}

/**
	@brief Disconnect from the Jabber session.
	@param client Pointer to the transport_client.
	@return 0 in all cases.

	If there are any messages still in the queue, they stay there; i.e. we don't free them here.
*/
int client_disconnect( transport_client* client ) {
	if( client == NULL ) { return 0; }
	return session_disconnect( client->session );
}

/**
	@brief Report whether a transport_client is connected.
	@param client Pointer to the transport_client.
	@return Boolean: 1 if connected, or 0 if not.
*/
int client_connected( const transport_client* client ) {
	if(client == NULL) return 0;
	return session_connected( client->session );
}

/**
	@brief Send a transport message to the current destination.
	@param client Pointer to a transport_client.
	@param msg Pointer to the transport_message to be sent.
	@return 0 if successful, or -1 if not.

	Translate the transport_message into XML and send it to Jabber, using the previously
	stored Jabber ID for the sender.
*/
int client_send_message( transport_client* client, transport_message* msg ) {
	if( client == NULL || client->error )
		return -1;
	if( msg->sender )
		free( msg->sender );
	msg->sender = strdup(client->xmpp_id);
	return session_send_msg( client->session, msg );
}

/**
	@brief Fetch an input message, if one is available.
	@param client Pointer to a transport_client.
	@param timeout How long to wait for a message to arrive, in seconds (see remarks).
	@return A pointer to a transport_message if successful, or NULL if not.

	If there is a message already in the queue, return it immediately.  Otherwise read any
	available messages from the transport_session (subject to a timeout), and return the
	first one.

	If the value of @a timeout is -1, then there is no time limit -- wait indefinitely until a
	message arrives (or we error out for other reasons).  If the value of @a timeout is zero,
	don't wait at all.

	The calling code is responsible for freeing the transport_message by calling message_free().
*/
transport_message* client_recv( transport_client* client, int timeout ) {
	if( client == NULL ) { return NULL; }

	int error = 0;  /* boolean */

	if( NULL == client->msg_q_head ) {

		// No message available on the queue?  Try to get a fresh one.

		// When we call session_wait(), it reads a socket for new messages.  When it finds
		// one, it enqueues it by calling the callback function client_message_handler(),
		// which we installed in the transport_session when we created the transport_client.

		// Since a single call to session_wait() may not result in the receipt of a complete
		// message. we call it repeatedly until we get either a message or an error.

		// Alternatively, a single call to session_wait() may result in the receipt of
		// multiple messages.  That's why we have to enqueue them.

		// The timeout applies to the receipt of a complete message.  For a sufficiently
		// short timeout, a sufficiently long message, and a sufficiently slow connection,
		// we could timeout on the first message even though we're still receiving data.
		
		// Likewise we could time out while still receiving the second or subsequent message,
		// return the first message, and resume receiving messages later.

		if( timeout == -1 ) {  /* wait potentially forever for data to arrive */

			int x;
			do {
				if( (x = session_wait( client->session, -1 )) ) {
					osrfLogDebug(OSRF_LOG_MARK, "session_wait returned failure code %d\n", x);
					error = 1;
					break;
				}
			} while( client->msg_q_head == NULL );

		} else {    /* loop up to 'timeout' seconds waiting for data to arrive  */

			/* This loop assumes that a time_t is denominated in seconds -- not */
			/* guaranteed by Standard C, but a fair bet for Linux or UNIX       */

			time_t start = time(NULL);
			time_t remaining = (time_t) timeout;

			int wait_ret;
			do {
				if( (wait_ret = session_wait( client->session, (int) remaining)) ) {
					error = 1;
					osrfLogDebug(OSRF_LOG_MARK,
						"session_wait returned failure code %d: setting error=1\n", wait_ret);
					break;
				}

				remaining -= time(NULL) - start;
			} while( NULL == client->msg_q_head && remaining > 0 );
		}
	}

	transport_message* msg = NULL;

	if( error )
		client->error = 1;
	else if( client->msg_q_head != NULL ) {
		/* got message(s); dequeue the oldest one */
		msg = client->msg_q_head;
		client->msg_q_head = msg->next;
		msg->next = NULL;  /* shouldn't be necessary; nullify for good hygiene */
		if( NULL == client->msg_q_head )
			client->msg_q_tail = NULL;
	}

	return msg;
}

/**
	@brief Enqueue a newly received transport_message.
	@param client A pointer to a transport_client, cast to a void pointer.
	@param msg A new transport message.

	Add a newly arrived input message to the tail of the queue.

	This is a callback function.  The transport_session parses the XML coming in through a
	socket, accumulating various bits and pieces.  When it sees the end of a message stanza,
	it packages the bits and pieces into a transport_message that it passes to this function,
	which enqueues the message for processing.
*/
static void client_message_handler( void* client, transport_message* msg ){

	if(client == NULL) return;
	if(msg == NULL) return;

	transport_client* cli = (transport_client*) client;

	/* add the new message to the tail of the queue */
	if( NULL == cli->msg_q_head )
		cli->msg_q_tail = cli->msg_q_head = msg;
	else {
		cli->msg_q_tail->next = msg;
		cli->msg_q_tail = msg;
	}
	msg->next = NULL;
}


/**
	@brief Free a transport_client, along with all resources it owns.
	@param client Pointer to the transport_client to be freed.
	@return 1 if successful, or 0 if not.  The only error condition is if @a client is NULL.
*/
int client_free( transport_client* client ) {
	if(client == NULL)
		return 0;
	session_free( client->session );
	client->session = NULL;
	return client_discard( client );
}

/**
	@brief Free a transport_client's resources, but without disconnecting.
	@param client Pointer to the transport_client to be freed.
	@return 1 if successful, or 0 if not.  The only error condition is if @a client is NULL.

	A child process may call this in order to free the resources associated with the parent's
	transport_client, but without disconnecting from Jabber, since disconnecting would
	disconnect the parent as well.
 */
int client_discard( transport_client* client ) {
	if(client == NULL)
		return 0;
	
	transport_message* current = client->msg_q_head;
	transport_message* next;

	/* deallocate the list of messages */
	while( current != NULL ) {
		next = current->next;
		message_free( current );
		current = next;
	}

	free(client->host);
	free(client->xmpp_id);
	free( client );
	return 1;
}

int client_sock_fd( transport_client* client )
{
	if( !client )
		return 0;
	else
		return client->session->sock_id;
}
