/**
	@file osrf_app_session.c
	@brief Implementation of osrfAppSession.
*/

#include <time.h>
#include "opensrf/osrf_app_session.h"
#include "opensrf/osrf_stack.h"

struct osrf_app_request_struct {
	/** The controlling session. */
	struct osrf_app_session_struct* session;

	/** Request id.  It is the same as the thread_trace of the REQUEST message
		for which it was created.
	*/
	int request_id;
	/** True if we have received a 'request complete' message from our request. */
	int complete;
	/** The original REQUEST message payload. */
	osrfMessage* payload;
	/** Linked list of responses to the request. */
	osrfMessage* result;

	/** Boolean; if true, then a call that is waiting on a response will reset the
	timeout and set this variable back to false. */
	int reset_timeout;
};
typedef struct osrf_app_request_struct osrfAppRequest;

/* Send the given message */
static int _osrf_app_session_send( osrfAppSession*, osrfMessage* msg );

static int osrfAppSessionMakeLocaleRequest(
		osrfAppSession* session, const jsonObject* params, const char* method_name,
		int protocol, osrfStringArray* param_strings, char* locale );

/** @brief The global session cache.

	Key: session_id.  Data: osrfAppSession.
*/
static osrfHash* osrfAppSessionCache = NULL;

// --------------------------------------------------------------------------
// Request API
// --------------------------------------------------------------------------

/**
	@brief Create a new osrfAppRequest.
	@param session Pointer to the osrfAppSession that will own the new osrfAppRequest.
	@param msg Pointer to the osrfMessage representing the request.
	@return Pointer to the new osrfAppRequest.

	The calling code is responsible for freeing the osrfAppRequest by calling
	_osrf_app_request_free().  In practice this normally happens via a callback installed
	in an osrfList.
*/
static osrfAppRequest* _osrf_app_request_init(
		osrfAppSession* session, osrfMessage* msg ) {

	osrfAppRequest* req =
		(osrfAppRequest*) safe_malloc(sizeof(osrfAppRequest));

	req->session        = session;
	req->request_id     = msg->thread_trace;
	req->complete       = 0;
	req->payload        = msg;
	req->result         = NULL;
	req->reset_timeout  = 0;

	return req;
}


/**
	@brief Free an osrfAppRequest and everything it owns.
	@param req Pointer to an osrfAppRequest, cast to a void pointer.

	This function is installed as a callback in the osrfList request_queue, a member
	of osrfAppSession.
*/
static void _osrf_app_request_free( void * req ){
	if( req ) {
		osrfAppRequest* r = (osrfAppRequest*) req;
		if( r->payload )
			osrfMessageFree( r->payload );

		/* Free the messages in the result queue */
		osrfMessage* next_msg;
		while( r->result ) {
			next_msg = r->result->next;
			osrfMessageFree( r->result );
			r->result = next_msg;
		}

		free( r );
	}
}

/**
	@brief Append a new message to the list of responses to a request.
	@param req Pointer to the osrfAppRequest for the original REQUEST message.
	@param result Pointer to an osrfMessage received in response to the request.

	We maintain a linked list of response messages, and traverse it to find the end.
*/
static void _osrf_app_request_push_queue( osrfAppRequest* req, osrfMessage* result ){
	if(req == NULL || result == NULL)
		return;

	osrfLogDebug( OSRF_LOG_MARK, "App Session pushing request [%d] onto request queue",
			result->thread_trace );
	if(req->result == NULL) {
		req->result = result;   // Add the first node

	} else {

		// Find the last node in the list, and append the new node to it
		osrfMessage* ptr = req->result;
		osrfMessage* ptr2 = req->result->next;
		while( ptr2 ) {
			ptr = ptr2;
			ptr2 = ptr2->next;
		}
		ptr->next = result;
	}
}

/**
	@brief Remove an osrfAppRequest (identified by request_id) from an osrfAppSession.
	@param session Pointer to the osrfAppSession that owns the osrfAppRequest.
	@param req_id request_id of the osrfAppRequest to be removed.

	The osrfAppRequest itself is freed by a callback installed in the osrfList where it resides.
*/
void osrf_app_session_request_finish(
		osrfAppSession* session, int req_id ){

	if( session ) {
		osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, req_id );
		if(req)
			osrfListRemove( req->session->request_queue, req->request_id );
	}
}


/**
	@brief Set the timeout for a request to one second.
	@param session Pointer to the relevant osrfAppSession.
	@param req_id Request ID of the request whose timeout is to be reset.

	The request to be reset is identified by the combination of session and request id.
*/
void osrf_app_session_request_reset_timeout( osrfAppSession* session, int req_id ) {
	if(session == NULL)
		return;
	osrfLogDebug( OSRF_LOG_MARK, "Resetting request timeout %d", req_id );
	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, req_id );
	if( req )
		req->reset_timeout = 1;
}

/**
	Checke the receive queue for messages.  If any are found, the first
	is popped off and returned.  Otherwise, this method will wait at most timeout
	seconds for a message to appear in the receive queue.  Once it arrives it is returned.
	If no messages arrive in the timeout provided, null is returned.
*/
/**
	@brief Return the next response message for a given request, subject to a timeout.
	@param req Pointer to the osrfAppRequest representing the request.
	@param timeout Maxmimum time to wait, in seconds.

	@return 

	If the input queue for this request is not empty, dequeue the next message and return it.
*/
static osrfMessage* _osrf_app_request_recv( osrfAppRequest* req, int timeout ) {

	if(req == NULL) return NULL;

	if( req->result != NULL ) {
		/* Dequeue the next message in the list */
		osrfMessage* tmp_msg = req->result;
		req->result = req->result->next;
		return tmp_msg;
	}

	time_t start = time(NULL);
	time_t remaining = (time_t) timeout;

	while( remaining >= 0 ) {
		/* tell the session to wait for stuff */
		osrfLogDebug( OSRF_LOG_MARK,  "In app_request receive with remaining time [%d]",
				(int) remaining );

		osrf_app_session_queue_wait( req->session, 0, NULL );
		if(req->session->transport_error) {
			osrfLogError(OSRF_LOG_MARK, "Transport error in recv()");
			return NULL;
		}

		if( req->result != NULL ) { /* if we received anything */
			/* pop off the first message in the list */
			osrfLogDebug( OSRF_LOG_MARK, "app_request_recv received a message, returning it" );
			osrfMessage* ret_msg = req->result;
			req->result = ret_msg->next;
			if (ret_msg->sender_locale)
				osrf_app_session_set_locale(req->session, ret_msg->sender_locale);

			return ret_msg;
		}

		if( req->complete )
			return NULL;

		osrf_app_session_queue_wait( req->session, (int) remaining, NULL );

		if(req->session->transport_error) {
			osrfLogError(OSRF_LOG_MARK, "Transport error in recv()");
			return NULL;
		}

		if( req->result != NULL ) { /* if we received anything */
			/* pop off the first message in the list */
			osrfLogDebug( OSRF_LOG_MARK,  "app_request_recv received a message, returning it");
			osrfMessage* ret_msg = req->result;
			req->result = ret_msg->next;
			if (ret_msg->sender_locale)
				osrf_app_session_set_locale(req->session, ret_msg->sender_locale);

			return ret_msg;
		}
		if( req->complete )
			return NULL;

		if(req->reset_timeout) {
			remaining = (time_t) timeout;
			req->reset_timeout = 0;
			osrfLogDebug( OSRF_LOG_MARK, "Received a timeout reset");
		} else {
			remaining -= (int) (time(NULL) - start);
		}
	}

	char* paramString = jsonObjectToJSON(req->payload->_params);
	osrfLogInfo( OSRF_LOG_MARK, "Returning NULL from app_request_recv after timeout: %s %s",
		req->payload->method_name, paramString);
	free(paramString);

	return NULL;
}

/** Resend this requests original request message */
static int _osrf_app_request_resend( osrfAppRequest* req ) {
	if(req == NULL) return 0;
	if(!req->complete) {
		osrfLogDebug( OSRF_LOG_MARK,  "Resending request [%d]", req->request_id );
		return _osrf_app_session_send( req->session, req->payload );
	}
	return 1;
}


// --------------------------------------------------------------------------
// Session API
// --------------------------------------------------------------------------

/** Install a locale for the session */
char* osrf_app_session_set_locale( osrfAppSession* session, const char* locale ) {
	if (!session || !locale)
		return NULL;

	if(session->session_locale) {
		if( strlen(session->session_locale) >= strlen(locale) ) {
			/* There's room available; just copy */
			strcpy(session->session_locale, locale);
		} else {
			free(session->session_locale);
			session->session_locale = strdup( locale );
		}
	} else {
		session->session_locale = strdup( locale );
	}

	return session->session_locale;
}

/**
	@brief Find the osrfAppSession for a given session id.
	@param session_id The session id to look for.
	@return Pointer to the corresponding osrfAppSession if found, or NULL if not.

	Search the global session cache for the specified session id.
*/
osrfAppSession* osrf_app_session_find_session( const char* session_id ) {
	if(session_id)
		return osrfHashGet( osrfAppSessionCache, session_id );
	return NULL;
}


/**
	@brief Add a session to the global session cache, keyed by session id.
	@param session Pointer to the osrfAppSession to be added.

	If a cache doesn't exist yet, create one.  It's an osrfHash using session ids for the
	key and osrfAppSessions for the data.
*/
static void _osrf_app_session_push_session( osrfAppSession* session ) {
	if( session ) {
		if( osrfAppSessionCache == NULL )
			osrfAppSessionCache = osrfNewHash();
		if( osrfHashGet( osrfAppSessionCache, session->session_id ) )
			return;   // A session with this id is already in the cache.  Shouldn't happen.
		osrfHashSet( osrfAppSessionCache, session, session->session_id );
	}
}

/** Allocates and initializes a new app_session */

osrfAppSession* osrfAppSessionClientInit( const char* remote_service ) {

	if (!remote_service) {
		osrfLogWarning( OSRF_LOG_MARK, "No remote service specified in osrfAppSessionClientInit");
		return NULL;
	}

	osrfAppSession* session = safe_malloc(sizeof(osrfAppSession));

	session->transport_handle = osrfSystemGetTransportClient();
	if( session->transport_handle == NULL ) {
		osrfLogWarning( OSRF_LOG_MARK, "No transport client for service 'client'");
		free( session );
		return NULL;
	}

	osrfStringArray* arr = osrfNewStringArray(8);
	osrfConfigGetValueList(NULL, arr, "/domain");
	const char* domain = osrfStringArrayGetString(arr, 0);

	if (!domain) {
		osrfLogWarning( OSRF_LOG_MARK, "No domains specified in the OpenSRF config file");
		free( session );
		osrfStringArrayFree(arr);
		return NULL;
	}

	char* router_name = osrfConfigGetValue(NULL, "/router_name");
	if (!router_name) {
		osrfLogWarning( OSRF_LOG_MARK, "No router name specified in the OpenSRF config file");
		free( session );
		osrfStringArrayFree(arr);
		return NULL;
	}

	char target_buf[512];
	target_buf[ 0 ] = '\0';

	int len = snprintf( target_buf, sizeof(target_buf), "%s@%s/%s",
			router_name ? router_name : "(null)",
			domain ? domain : "(null)",
			remote_service ? remote_service : "(null)" );
	osrfStringArrayFree(arr);
	//free(domain);
	free(router_name);

	if( len >= sizeof( target_buf ) ) {
		osrfLogWarning( OSRF_LOG_MARK, "Buffer overflow for remote_id");
		free( session );
		return NULL;
	}

	session->request_queue = osrfNewList();
	session->request_queue->freeItem = &_osrf_app_request_free;
	session->remote_id = strdup(target_buf);
	session->orig_remote_id = strdup(session->remote_id);
	session->remote_service = strdup(remote_service);
	session->session_locale = NULL;
	session->transport_error = 0;

	#ifdef ASSUME_STATELESS
	session->stateless = 1;
	osrfLogDebug( OSRF_LOG_MARK, "%s session is stateless", remote_service );
	#else
	session->stateless = 0;
	osrfLogDebug( OSRF_LOG_MARK, "%s session is NOT stateless", remote_service );
	#endif

	/* build a chunky, random session id */
	char id[256];

	snprintf(id, sizeof(id), "%f.%d%ld", get_timestamp_millis(), (int)time(NULL), (long) getpid());
	session->session_id = strdup(id);
	osrfLogDebug( OSRF_LOG_MARK,  "Building a new client session with id [%s] [%s]",
			session->remote_service, session->session_id );

	session->thread_trace = 0;
	session->state = OSRF_SESSION_DISCONNECTED;
	session->type = OSRF_SESSION_CLIENT;

	session->userData = NULL;
	session->userDataFree = NULL;

	_osrf_app_session_push_session( session );
	return session;
}

osrfAppSession* osrf_app_server_session_init(
		const char* session_id, const char* our_app, const char* remote_id ) {

	osrfLogDebug( OSRF_LOG_MARK, "Initing server session with session id %s, service %s,"
			" and remote_id %s", session_id, our_app, remote_id );

	osrfAppSession* session = osrf_app_session_find_session( session_id );
	if(session) return session;

	session = safe_malloc(sizeof(osrfAppSession));

	session->transport_handle = osrfSystemGetTransportClient();
	if( session->transport_handle == NULL ) {
		osrfLogWarning( OSRF_LOG_MARK, "No transport client for service '%s'", our_app );
		free(session);
		return NULL;
	}

	int stateless = 0;
	char* statel = osrf_settings_host_value("/apps/%s/stateless", our_app );
	if(statel) stateless = atoi(statel);
	free(statel);


	session->request_queue = osrfNewList();
	session->request_queue->freeItem = &_osrf_app_request_free;
	session->remote_id = strdup(remote_id);
	session->orig_remote_id = strdup(remote_id);
	session->session_id = strdup(session_id);
	session->remote_service = strdup(our_app);
	session->stateless = stateless;

	#ifdef ASSUME_STATELESS
	session->stateless = 1;
	#else
	session->stateless = 0;
	#endif

	session->thread_trace = 0;
	session->state = OSRF_SESSION_DISCONNECTED;
	session->type = OSRF_SESSION_SERVER;
	session->session_locale = NULL;

	session->userData = NULL;
	session->userDataFree = NULL;

	_osrf_app_session_push_session( session );
	return session;

}

/**
	@brief Create a REQUEST message, send it, and save it for future reference.
	@param session Pointer to the current session, which has the addressing information.
	@param params One way of specifying the parameters for the method.
	@param method_name The name of the method to be called.
	@param protocol Protocol.
	@param param_strings Another way of specifying the parameters for the method.
	@return The request ID of the resulting REQUEST message, or -1 upon error.

	If @a params is non-NULL, use it to specify the parameters to the method.  Otherwise
	use @a param_strings.

	If @a params points to a JSON_ARRAY, then pass each element of the array as a separate
	parameter.  If @a params points to any other kind of jsonObject, pass it as a single
	parameter.

	If @a params is NULL, and @a param_strings is not NULL, then each pointer in the
	osrfStringArray must point to a JSON string encoding a parameter.  Pass them.

	At this writing, all calls to this function use @a params to pass parameters, rather than
	@a param_strings.

	This function is a thin wrapper for osrfAppSessionMakeLocaleRequest().
*/
int osrfAppSessionMakeRequest(
		osrfAppSession* session, const jsonObject* params,
		const char* method_name, int protocol, osrfStringArray* param_strings ) {

	return osrfAppSessionMakeLocaleRequest( session, params,
			method_name, protocol, param_strings, NULL );
}

/**
	@brief Create a REQUEST message, send it, and save it for future reference.
	@param session Pointer to the current session, which has the addressing information.
	@param params One way of specifying the parameters for the method.
	@param method_name The name of the method to be called.
	@param protocol Protocol.
	@param param_strings Another way of specifying the parameters for the method.
	@param locale Pointer to a locale string.
	@return The request ID of the resulting REQUEST message, or -1 upon error.

	See the discussion of osrfAppSessionMakeRequest(), which at this writing is the only
	place that calls this function.

	At this writing, the @a param_strings and @a locale parameters are always NULL.
*/
static int osrfAppSessionMakeLocaleRequest(
		osrfAppSession* session, const jsonObject* params, const char* method_name,
		int protocol, osrfStringArray* param_strings, char* locale ) {

	if(session == NULL) return -1;

	osrfLogMkXid();

	osrfMessage* req_msg = osrf_message_init( REQUEST, ++(session->thread_trace), protocol );
	osrf_message_set_method(req_msg, method_name);

	if (locale) {
		osrf_message_set_locale(req_msg, locale);
	} else if (session->session_locale) {
		osrf_message_set_locale(req_msg, session->session_locale);
	}

	if(params) {
		osrf_message_set_params(req_msg, params);

	} else {

		if(param_strings) {
			int i;
			for(i = 0; i!= param_strings->size ; i++ ) {
				osrf_message_add_param(req_msg,
					osrfStringArrayGetString(param_strings,i));
			}
		}
	}

	osrfAppRequest* req = _osrf_app_request_init( session, req_msg );
	if(_osrf_app_session_send( session, req_msg ) ) {
		osrfLogWarning( OSRF_LOG_MARK,  "Error sending request message [%d]",
				session->thread_trace );
		_osrf_app_request_free(req);
		return -1;
	}

	osrfLogDebug( OSRF_LOG_MARK,  "Pushing [%d] onto request queue for session [%s] [%s]",
			req->request_id, session->remote_service, session->session_id );
	osrfListSet( session->request_queue, req, req->request_id );
	return req->request_id;
}

void osrf_app_session_set_complete( osrfAppSession* session, int request_id ) {
	if(session == NULL)
		return;

	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, request_id );
	if(req) req->complete = 1;
}

int osrf_app_session_request_complete( const osrfAppSession* session, int request_id ) {
	if(session == NULL)
		return 0;
	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, request_id );
	if(req)
		return req->complete;
	return 0;
}


/** Resets the remote connection id to that of the original*/
void osrf_app_session_reset_remote( osrfAppSession* session ){
	if( session==NULL )
		return;

	osrfLogDebug( OSRF_LOG_MARK,  "App Session [%s] [%s] resetting remote id to %s",
			session->remote_service, session->session_id, session->orig_remote_id );

	osrf_app_session_set_remote( session, session->orig_remote_id );
}

void osrf_app_session_set_remote( osrfAppSession* session, const char* remote_id ) {
	if(session == NULL)
		return;

	if( session->remote_id ) {
		if( strlen(session->remote_id) >= strlen(remote_id) ) {
			// There's enough room; just copy it
			strcpy(session->remote_id, remote_id);
		} else {
			free(session->remote_id );
			session->remote_id = strdup( remote_id );
		}
	} else
		session->remote_id = strdup( remote_id );
}

/**
	pushes the given message into the result list of the app_request
	with the given request_id
*/
int osrf_app_session_push_queue(
		osrfAppSession* session, osrfMessage* msg ){
	if(session == NULL || msg == NULL) return 0;

	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, msg->thread_trace );
	if(req == NULL) return 0;
	_osrf_app_request_push_queue( req, msg );

	return 0;
}

/** Attempts to connect to the remote service */
int osrfAppSessionConnect( osrfAppSession* session ) {

	if(session == NULL)
		return 0;

	if(session->state == OSRF_SESSION_CONNECTED) {
		return 1;
	}

	int timeout = 5; /* XXX CONFIG VALUE */

	osrfLogDebug( OSRF_LOG_MARK,  "AppSession connecting to %s", session->remote_id );

	/* defaulting to protocol 1 for now */
	osrfMessage* con_msg = osrf_message_init( CONNECT, session->thread_trace, 1 );
	osrf_app_session_reset_remote( session );
	session->state = OSRF_SESSION_CONNECTING;
	int ret = _osrf_app_session_send( session, con_msg );
	osrfMessageFree(con_msg);
	if(ret)
		return 0;

	time_t start = time(NULL);
	time_t remaining = (time_t) timeout;

	while( session->state != OSRF_SESSION_CONNECTED && remaining >= 0 ) {
		osrf_app_session_queue_wait( session, remaining, NULL );
		if(session->transport_error) {
			osrfLogError(OSRF_LOG_MARK, "cannot communicate with %s", session->remote_service);
			return 0;
		}
		remaining -= (int) (time(NULL) - start);
	}

	if(session->state == OSRF_SESSION_CONNECTED)
		osrfLogDebug( OSRF_LOG_MARK, " * Connected Successfully to %s", session->remote_service );

	if(session->state != OSRF_SESSION_CONNECTED)
		return 0;

	return 1;
}



/** Disconnects from the remote service */
int osrf_app_session_disconnect( osrfAppSession* session){
	if(session == NULL)
		return 1;

	if(session->state == OSRF_SESSION_DISCONNECTED)
		return 1;

	if(session->stateless && session->state != OSRF_SESSION_CONNECTED) {
		osrfLogDebug( OSRF_LOG_MARK,
				"Exiting disconnect on stateless session %s",
				session->session_id);
		return 1;
	}

	osrfLogDebug(OSRF_LOG_MARK,  "AppSession disconnecting from %s", session->remote_id );

	osrfMessage* dis_msg = osrf_message_init( DISCONNECT, session->thread_trace, 1 );
	_osrf_app_session_send( session, dis_msg );
	session->state = OSRF_SESSION_DISCONNECTED;

	osrfMessageFree( dis_msg );
	osrf_app_session_reset_remote( session );
	return 1;
}

int osrf_app_session_request_resend( osrfAppSession* session, int req_id ) {
	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, req_id );
	return _osrf_app_request_resend( req );
}


static int osrfAppSessionSendBatch( osrfAppSession* session, osrfMessage* msgs[], int size ) {

	if( !(session && msgs && size > 0) ) return 0;
	int retval = 0;

	osrfMessage* msg = msgs[0];

	if(msg) {

		osrf_app_session_queue_wait( session, 0, NULL );

		if(session->state != OSRF_SESSION_CONNECTED)  {

			if(session->stateless) { /* stateless session always send to the root listener */
				osrf_app_session_reset_remote(session);

			} else {

				/* do an auto-connect if necessary */
				if( ! session->stateless &&
					(msg->m_type != CONNECT) &&
					(msg->m_type != DISCONNECT) &&
					(session->state != OSRF_SESSION_CONNECTED) ) {

					if(!osrfAppSessionConnect( session ))
						return 0;
				}
			}
		}
	}

	char* string = osrfMessageSerializeBatch(msgs, size);

	if( string ) {

		transport_message* t_msg = message_init(
				string, "", session->session_id, session->remote_id, NULL );
		message_set_osrf_xid( t_msg, osrfLogGetXid() );

		retval = client_send_message( session->transport_handle, t_msg );

		if( retval ) osrfLogError(OSRF_LOG_MARK, "client_send_message failed");

		osrfLogInfo(OSRF_LOG_MARK, "[%s] sent %d bytes of data to %s",
			session->remote_service, strlen(string), t_msg->recipient );

		osrfLogDebug(OSRF_LOG_MARK, "Sent: %s", string );

		free(string);
		message_free( t_msg );
	}

	return retval;
}



static int _osrf_app_session_send( osrfAppSession* session, osrfMessage* msg ){
	if( !(session && msg) ) return 0;
	osrfMessage* a[1];
	a[0] = msg;
	return osrfAppSessionSendBatch( session, a, 1 );
}


/**
	Waits up to 'timeout' seconds for some data to arrive.
	Any data that arrives will be processed according to its
	payload and message type.  This method will return after
	any data has arrived.
*/
int osrf_app_session_queue_wait( osrfAppSession* session, int timeout, int* recvd ){
	if(session == NULL) return 0;
	osrfLogDebug(OSRF_LOG_MARK, "AppSession in queue_wait with timeout %d", timeout );
	return osrf_stack_process(session->transport_handle, timeout, recvd);
}

/** Disconnects (if client) and removes the given session from the global session cache
	! This frees all attached app_requests !
*/
void osrfAppSessionFree( osrfAppSession* session ){
	if(session == NULL) return;

	/* Disconnect */

	osrfLogDebug(OSRF_LOG_MARK,  "AppSession [%s] [%s] destroying self and deleting requests",
			session->remote_service, session->session_id );
	if(session->type == OSRF_SESSION_CLIENT
			&& session->state != OSRF_SESSION_DISCONNECTED ) { /* disconnect if we're a client */
		osrfMessage* dis_msg = osrf_message_init( DISCONNECT, session->thread_trace, 1 );
		_osrf_app_session_send( session, dis_msg );
		osrfMessageFree(dis_msg);
	}

	/* Remove self from the global session cache */

	osrfHashRemove( osrfAppSessionCache, session->session_id );

	/* Free the memory */

	if( session->userDataFree && session->userData )
		session->userDataFree(session->userData);

	if(session->session_locale)
		free(session->session_locale);

	free(session->remote_id);
	free(session->orig_remote_id);
	free(session->session_id);
	free(session->remote_service);
	osrfListFree(session->request_queue);
	free(session);
}

osrfMessage* osrfAppSessionRequestRecv(
		osrfAppSession* session, int req_id, int timeout ) {
	if(req_id < 0 || session == NULL)
		return NULL;
	osrfAppRequest* req = OSRF_LIST_GET_INDEX( session->request_queue, req_id );
	return _osrf_app_request_recv( req, timeout );
}



int osrfAppRequestRespond( osrfAppSession* ses, int requestId, const jsonObject* data ) {
	if(!ses || ! data ) return -1;

	osrfMessage* msg = osrf_message_init( RESULT, requestId, 1 );
	osrf_message_set_status_info( msg, NULL, "OK", OSRF_STATUS_OK );
	char* json = jsonObjectToJSON( data );

	osrf_message_set_result_content( msg, json );
	_osrf_app_session_send( ses, msg );

	free(json);
	osrfMessageFree( msg );

	return 0;
}


int osrfAppRequestRespondComplete(
		osrfAppSession* ses, int requestId, const jsonObject* data ) {

	osrfMessage* status = osrf_message_init( STATUS, requestId, 1);
	osrf_message_set_status_info( status, "osrfConnectStatus", "Request Complete",
			OSRF_STATUS_COMPLETE );

	if (data) {
		osrfMessage* payload = osrf_message_init( RESULT, requestId, 1 );
		osrf_message_set_status_info( payload, NULL, "OK", OSRF_STATUS_OK );

		char* json = jsonObjectToJSON( data );
		osrf_message_set_result_content( payload, json );
		free(json);

		osrfMessage* ms[2];
		ms[0] = payload;
		ms[1] = status;

		osrfAppSessionSendBatch( ses, ms, 2 );

		osrfMessageFree( payload );
	} else {
		osrfAppSessionSendBatch( ses, &status, 1 );
	}

	osrfMessageFree( status );

	return 0;
}

int osrfAppSessionStatus( osrfAppSession* ses, int type,
		const char* name, int reqId, const char* message ) {

	if(ses) {
		osrfMessage* msg = osrf_message_init( STATUS, reqId, 1);
		osrf_message_set_status_info( msg, name, message, type );
		_osrf_app_session_send( ses, msg );
		osrfMessageFree( msg );
		return 0;
	}
	return -1;
}

/**
	@brief Free the global session cache.
	
	Note that the osrfHash that implements the global session cache does @em not have a
	callback function installed for freeing its cargo.  As a result, any outstanding
	osrfAppSessions are leaked, along with all the osrfAppRequests and osrfMessages they
	own.
*/
void osrfAppSessionCleanup( void ) {
	osrfHashFree(osrfAppSessionCache);
	osrfAppSessionCache = NULL;
}





