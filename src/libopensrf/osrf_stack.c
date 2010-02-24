#include <opensrf/osrf_stack.h>
#include <opensrf/osrf_application.h>

/**
	@file osrf_stack.c
	@brief Routines to receive and process input osrfMessages.
*/

/* the max number of oilsMessage blobs present in any one root packet */
#define OSRF_MAX_MSGS_PER_PACKET 256
// -----------------------------------------------------------------------------

static void _do_client( osrfAppSession*, osrfMessage* );
static void _do_server( osrfAppSession*, osrfMessage* );

/**
	@brief Read and process available transport_messages for a transport_client.
	@param client Pointer to the transport_client whose socket is to be read.
	@param timeout How many seconds to wait for the first message.
	@param msg_received A pointer through which to report whether a message was received.
	@return 0 upon success (even if a timeout occurs), or -1 upon failure.

	Read and process all available transport_messages from the socket of the specified
	transport_client.  Pass each one through osrf_stack_transport().

	The timeout applies only to the first message.  Any subsequent messages must be
	available immediately.  Don't wait for them, even if the timeout has not expired.  In
	theory, a sufficiently large backlog of input messages could keep you working past the
	nominal expiration of the timeout.

	The @a msg_received parameter points to an int owned by the calling code and used as
	a boolean.  Set it to true if you receive at least one transport_message, or to false
	if you don't.  A timeout is not treated as an error; it just means you must set that
	boolean to false.
*/
int osrf_stack_process( transport_client* client, int timeout, int* msg_received ) {
	if( !client ) return -1;
	transport_message* msg = NULL;
	if(msg_received) *msg_received = 0;

	// Loop through the available input messages
	while( (msg = client_recv( client, timeout )) ) {
		if(msg_received) *msg_received = 1;
		osrfLogDebug( OSRF_LOG_MARK, "Received message from transport code from %s", msg->sender );
		osrf_stack_transport_handler( msg, NULL );
		timeout = 0;
	}

	if( client->error ) {
		osrfLogWarning(OSRF_LOG_MARK, "transport_client had trouble reading from the socket..");
		return -1;
	}

	if( ! client_connected( client ) ) return -1;

	return 0;
}

// -----------------------------------------------------------------------------
// Entry point into the stack
// -----------------------------------------------------------------------------
/**
	@brief Unpack a transport_message into one or more osrfMessages, and process each one.
	@param msg Pointer to the transport_message to be unpacked and processed.
	@param my_service Application name (optional).
	@return Pointer to an osrfAppSession -- either a pre-existing one or a new one.

	Look for an existing osrfAppSession with which the message is associated.  Such a session
	may already exist if, for example, you're a client waiting for a response from some other
	application, or if you're a server that has opened a stateful session with a client.

	If you can't find an existing session for the current message, and the @a my_service
	parameter has provided an application name, then you're presumably a server receiving
	something from a new client.  Create an application server session to own the new message.

	Barring various errors and malformations, extract one or more osrfMessages from the
	transport_message.  Pass each one to the appropriate routine for processing, depending
	on whether you're acting as a client or as a server.
*/
struct osrf_app_session_struct* osrf_stack_transport_handler( transport_message* msg,
		const char* my_service ) {

	if(!msg) return NULL;

	osrfLogSetXid(msg->osrf_xid);

	osrfLogDebug( OSRF_LOG_MARK,  "Transport handler received new message \nfrom %s "
			"to %s with body \n\n%s\n", msg->sender, msg->recipient, msg->body );

	if( msg->is_error && ! msg->thread ) {
		osrfLogWarning( OSRF_LOG_MARK,
				"!! Received jabber layer error for %s ... exiting\n", msg->sender );
		message_free( msg );
		return NULL;
	}

	if(! msg->thread  && ! msg->is_error ) {
		osrfLogWarning( OSRF_LOG_MARK,
				"Received a non-error message with no thread trace... dropping");
		message_free( msg );
		return NULL;
	}

	osrfAppSession* session = osrf_app_session_find_session( msg->thread );

	if( !session && my_service )
		session = osrf_app_server_session_init( msg->thread, my_service, msg->sender);

	if( !session ) {
		message_free( msg );
		return NULL;
	}

	if(!msg->is_error)
		osrfLogDebug( OSRF_LOG_MARK, "Session [%s] found or built", session->session_id );

	osrf_app_session_set_remote( session, msg->sender );
	osrfMessage* arr[OSRF_MAX_MSGS_PER_PACKET];

	/* Convert the message body into one or more osrfMessages */
	int num_msgs = osrf_message_deserialize(msg->body, arr, OSRF_MAX_MSGS_PER_PACKET);

	osrfLogDebug( OSRF_LOG_MARK, "We received %d messages from %s", num_msgs, msg->sender );

	double starttime = get_timestamp_millis();

	int i;
	for( i = 0; i < num_msgs; i++ ) {

		/* if we've received a jabber layer error message (probably talking to
			someone who no longer exists) and we're not talking to the original
			remote id for this server, consider it a redirect and pass it up */
		if(msg->is_error) {
			osrfLogWarning( OSRF_LOG_MARK,  " !!! Received Jabber layer error message" );

			if( strcmp( session->remote_id, session->orig_remote_id ) ) {
				osrfLogWarning( OSRF_LOG_MARK, "Treating jabber error as redirect for tt [%d] "
					"and session [%s]", arr[i]->thread_trace, session->session_id );

				arr[i]->m_type = STATUS;
				arr[i]->status_code = OSRF_STATUS_REDIRECTED;

			} else {
				osrfLogWarning( OSRF_LOG_MARK, " * Jabber Error is for top level remote "
					" id [%s], no one to send my message to!  Cutting request short...",
					session->remote_id );
				session->transport_error = 1;
				break;
			}
		}

		if( session->type == OSRF_SESSION_CLIENT )
			_do_client( session, arr[i] );
		else
			_do_server( session, arr[i] );
	}

	double duration = get_timestamp_millis() - starttime;
	osrfLogInfo(OSRF_LOG_MARK, "Message processing duration %f", duration);

	message_free( msg );
	osrfLogDebug( OSRF_LOG_MARK, "after msg delete");

	return session;
}

/**
	@brief Acting as a client, process an incoming osrfMessage.
	@param session Pointer to the osrfAppSession to which the message pertains.
	@param msg Pointer to the osrfMessage.

	What we do with the message depends on the combination of message type and status code:
	- If it's a RESULT message, add it to the message queue of the appropriate app session,
	to be handled later.
	- If it's a STATUS message, handle it according to its status code and return NULL --
	unless it has an unexpected status code, in which case add it to the message queue of
	the appropriate app session, to be handled later.
*/
static void _do_client( osrfAppSession* session, osrfMessage* msg ) {
	if(session == NULL || msg == NULL)
		return;

	if( msg->m_type == STATUS ) {

		switch( msg->status_code ) {

			case OSRF_STATUS_OK:
				// This combination of message type and status code comes
				// only from the router, in response to a CONNECT message.
				osrfLogDebug( OSRF_LOG_MARK, "We connected successfully");
				session->state = OSRF_SESSION_CONNECTED;
				osrfLogDebug( OSRF_LOG_MARK,  "State: %x => %s => %d", session,
						session->session_id, session->state );
				osrfMessageFree(msg);
				break;

			case OSRF_STATUS_COMPLETE:
				osrf_app_session_set_complete( session, msg->thread_trace );
				osrfMessageFree(msg);
				break;

			case OSRF_STATUS_CONTINUE:
				osrf_app_session_request_reset_timeout( session, msg->thread_trace );
				osrfMessageFree(msg);
				break;

			case OSRF_STATUS_REDIRECTED:
				osrf_app_session_reset_remote( session );
				session->state = OSRF_SESSION_DISCONNECTED;
				osrf_app_session_request_resend( session, msg->thread_trace );
				osrfMessageFree(msg);
				break;

			case OSRF_STATUS_EXPFAILED:
				osrf_app_session_reset_remote( session );
				session->state = OSRF_SESSION_DISCONNECTED;
				osrfMessageFree(msg);
				break;

			case OSRF_STATUS_TIMEOUT:
				osrf_app_session_reset_remote( session );
				session->state = OSRF_SESSION_DISCONNECTED;
				osrf_app_session_request_resend( session, msg->thread_trace );
				osrfMessageFree(msg);
				break;

			default:
			{
				/* Replace the old message with a new one */
				osrfMessage* new_msg = osrf_message_init( 
						RESULT, msg->thread_trace, msg->protocol );
				osrf_message_set_status_info( new_msg,
						msg->status_name, msg->status_text, msg->status_code );
				osrfLogWarning( OSRF_LOG_MARK, "The stack doesn't know what to do with "
						"the provided message code: %d, name %s. Passing UP.",
						msg->status_code, msg->status_name );
				new_msg->is_exception = 1;
				osrf_app_session_set_complete( session, msg->thread_trace );
				osrfLogDebug( OSRF_LOG_MARK, 
						"passing client message %d / session %s to app handler",
						msg->thread_trace, session->session_id );
				osrfMessageFree(msg);
				// Enqueue the new message to be processed later
				osrf_app_session_push_queue( session, new_msg );
				break;
			} // end default
		} // end switch

	} else if( msg->m_type == RESULT ) {
		osrfLogDebug( OSRF_LOG_MARK, "passing client message %d / session %s to app handler",
					  msg->thread_trace, session->session_id );
		// Enqueue the RESULT message to be processed later
		osrf_app_session_push_queue( session, msg );

	}

	return;
}

/**
	@brief Acting as a server, process an incoming osrfMessage.
	@param session Pointer to the osrfAppSession to which the message pertains.
	@param msg Pointer to the osrfMessage.

	Branch on the message type.  In particular, if it's a REQUEST, call the requested method.
*/
static void _do_server( osrfAppSession* session, osrfMessage* msg ) {

	if(session == NULL || msg == NULL) return;

	osrfLogDebug( OSRF_LOG_MARK, "Server received message of type %d", msg->m_type );

	switch( msg->m_type ) {

		case STATUS:
			break;

		case DISCONNECT:
			/* session will be freed by the forker */
			osrfLogDebug(OSRF_LOG_MARK, "Client sent explicit disconnect");
			session->state = OSRF_SESSION_DISCONNECTED;
			break;

		case CONNECT:
			osrfAppSessionStatus( session, OSRF_STATUS_OK,
					"osrfConnectStatus", msg->thread_trace, "Connection Successful" );
			session->state = OSRF_SESSION_CONNECTED;
			break;

		case REQUEST:
			osrfLogDebug( OSRF_LOG_MARK, "server passing message %d to application handler "
					"for session %s", msg->thread_trace, session->session_id );

			osrfAppRunMethod( session->remote_service, msg->method_name,
				session, msg->thread_trace, msg->_params );

			break;

		default:
			osrfLogWarning( OSRF_LOG_MARK,
					"Server cannot handle message of type %d", msg->m_type );
			session->state = OSRF_SESSION_DISCONNECTED;
			break;
	}

	osrfMessageFree(msg);
	return;
}
