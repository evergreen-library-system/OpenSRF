#include <opensrf/transport_client.h>

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

	return client;
}


int client_connect( transport_client* client, 
		const char* username, const char* password, const char* resource, 
		int connect_timeout, enum TRANSPORT_AUTH_TYPE  auth_type ) {
	if(client == NULL) return 0; 
    client->xmpp_id = va_list_to_string("%s@%s/%s", username, client->host, resource);
	return session_connect( client->session, username, 
			password, resource, connect_timeout, auth_type );
}


int client_disconnect( transport_client* client ) {
	if( client == NULL ) { return 0; }
	return session_disconnect( client->session );
}

int client_connected( const transport_client* client ) {
	if(client == NULL) return 0;
	return client->session->state_machine->connected;
}

int client_send_message( transport_client* client, transport_message* msg ) {
	if(client == NULL) return 0;
	if( client->error ) return -1;
    msg->sender = strdup(client->xmpp_id); // free'd in message_free
	return session_send_msg( client->session, msg );
}


transport_message* client_recv( transport_client* client, int timeout ) {
	if( client == NULL ) { return NULL; }

	int error = 0;  /* boolean */

	if( NULL == client->msg_q_head ) {

		/* no messaage available?  try to get one */
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

// ---------------------------------------------------------------------------
// This is the message handler required by transport_session.  This handler
// takes an incoming message and adds it to the tail of a message queue.
// ---------------------------------------------------------------------------
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


int client_free( transport_client* client ){
	if(client == NULL) return 0; 

	session_free( client->session );
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

