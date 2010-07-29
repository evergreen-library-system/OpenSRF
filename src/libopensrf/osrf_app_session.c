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
	/** Linkage pointers for a linked list.  We maintain a hash table of pending requests,
	    and each slot of the hash table is a doubly linked list. */
	osrfAppRequest* next;
	osrfAppRequest* prev;
};

static inline unsigned int request_id_hash( int req_id );
static osrfAppRequest* find_app_request( const osrfAppSession* session, int req_id );
static void add_app_request( osrfAppSession* session, osrfAppRequest* req );

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
	_osrf_app_request_free().
*/
static osrfAppRequest* _osrf_app_request_init(
		osrfAppSession* session, osrfMessage* msg ) {

	osrfAppRequest* req = safe_malloc(sizeof(osrfAppRequest));

	req->session        = session;
	req->request_id     = msg->thread_trace;
	req->complete       = 0;
	req->payload        = msg;
	req->result         = NULL;
	req->reset_timeout  = 0;
	req->next           = NULL;
	req->prev           = NULL;

	return req;
}


/**
	@brief Free an osrfAppRequest and everything it owns.
	@param req Pointer to an osrfAppRequest.
*/
static void _osrf_app_request_free( osrfAppRequest * req ) {
	if( req ) {
		if( req->payload )
			osrfMessageFree( req->payload );

		/* Free the messages in the result queue */
		osrfMessage* next_msg;
		while( req->result ) {
			next_msg = req->result->next;
			osrfMessageFree( req->result );
			req->result = next_msg;
		}

		free( req );
	}
}

/**
	@brief Append a new message to the list of responses to a request.
	@param req Pointer to the osrfAppRequest for the original REQUEST message.
	@param result Pointer to an osrfMessage received in response to the request.

	For each osrfAppRequest we maintain a linked list of response messages, and traverse
	it to find the end.
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
*/
void osrf_app_session_request_finish( osrfAppSession* session, int req_id ) {

	if( session ) {
		// Search the hash table for the request in question
		unsigned int index = request_id_hash( req_id );
		osrfAppRequest* old_req = session->request_hash[ index ];
		while( old_req ) {
			if( old_req->request_id == req_id )
				break;
			else
				old_req = old_req->next;
		}

		if( old_req ) {
			// Remove the request from the doubly linked list
			if( old_req->prev )
				old_req->prev->next = old_req->next;
			else
				session->request_hash[ index ] = old_req->next;

			if( old_req->next )
				old_req->next->prev = old_req->prev;

			_osrf_app_request_free( old_req );
		}
	}
}

/**
	@brief Derive a hash key from a request id.
	@param req_id The request id.
	@return The corresponding hash key; an index into request_hash[].

	If OSRF_REQUEST_HASH_SIZE is a power of two, then this calculation should
	reduce to a binary AND.
*/
static inline unsigned int request_id_hash( int req_id ) {
	return ((unsigned int) req_id ) % OSRF_REQUEST_HASH_SIZE;
}

/**
	@brief Search for an osrfAppRequest in the hash table, given a request id.
	@param session Pointer to the relevant osrfAppSession.
	@param req_id The request_id of the osrfAppRequest being sought.
	@return A pointer to the osrfAppRequest if found, or NULL if not.
*/
static osrfAppRequest* find_app_request( const osrfAppSession* session, int req_id ) {

	osrfAppRequest* req = session->request_hash[ request_id_hash( req_id) ];
	while( req ) {
		if( req->request_id == req_id )
			break;
		else
			req = req->next;
	}

	return req;
}

/**
	@brief Add an osrfAppRequest to the hash table of a given osrfAppSession.
	@param session Pointer to the session to which the request belongs.
	@param req Pointer to the osrfAppRequest to be stored.

	Find the right spot in the hash table; then add the request to the linked list at that
	spot.  We just add it to the head of the list, without trying to maintain any particular
	ordering.
*/
static void add_app_request( osrfAppSession* session, osrfAppRequest* req ) {
	if( session && req ) {
		unsigned int index = request_id_hash( req->request_id );
		req->next = session->request_hash[ index ];
		req->prev = NULL;
		session->request_hash[ index ] = req;
	}
}

/**
	@brief Request a reset of the timeout period for a request.
	@param session Pointer to the relevant osrfAppSession.
	@param req_id Request ID of the request whose timeout is to be reset.

	This happens when a client receives a STATUS message with a status code
	OSRF_STATUS_CONTINUE; in effect the server is asking for more time.

	The request to be reset is identified by the combination of session and request id.
*/
void osrf_app_session_request_reset_timeout( osrfAppSession* session, int req_id ) {
	if(session == NULL)
		return;
	osrfLogDebug( OSRF_LOG_MARK, "Resetting request timeout %d", req_id );
	osrfAppRequest* req = find_app_request( session, req_id );
	if( req )
		req->reset_timeout = 1;
}

/**
	@brief Fetch the next response message to a given previous request, subject to a timeout.
	@param req Pointer to the osrfAppRequest representing the request.
	@param timeout Maxmimum time to wait, in seconds.

	@return Pointer to the next osrfMessage for this request, if one is available, or if it
	becomes available before the end of the timeout; otherwise NULL;

	If there is already a message available in the input queue for this request, dequeue and
	return it immediately.  Otherwise wait up to timeout seconds until you either get an
	input message for the specified request, run out of time, or encounter an error.

	If the only message we receive for this request is a STATUS message with a status code
	OSRF_STATUS_COMPLETE, then return NULL.  That means that the server has nothing further
	to send in response to this request.

	You may also receive other messages for other requests, and other sessions.  These other
	messages will be wholly or partially processed behind the scenes while you wait for the
	one you want.
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

	// Wait repeatedly for input messages until you either receive one for the request
	// you're interested in, run out of time, or encounter an error.
	// Wait repeatedly because you may also receive messages for other requests, or for
	// other sessions, and process them behind the scenes. These are not the messages
	// you're looking for.
	while( remaining >= 0 ) {
		/* tell the session to wait for stuff */
		osrfLogDebug( OSRF_LOG_MARK,  "In app_request receive with remaining time [%d]",
				(int) remaining );


		osrf_app_session_queue_wait( req->session, 0, NULL );
		if(req->session->transport_error) {
			osrfLogError(OSRF_LOG_MARK, "Transport error in recv()");
			return NULL;
		}

		if( req->result != NULL ) { /* if we received any results for this request */
			/* dequeue the first message in the list */
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

		if( req->result != NULL ) { /* if we received any results for this request */
			/* dequeue the first message in the list */
			osrfLogDebug( OSRF_LOG_MARK,  "app_request_recv received a message, returning it");
			osrfMessage* ret_msg = req->result;
			req->result = ret_msg->next;
			if (ret_msg->sender_locale)
				osrf_app_session_set_locale(req->session, ret_msg->sender_locale);

			return ret_msg;
		}

		if( req->complete )
			return NULL;

		// Determine how much time is left
		if(req->reset_timeout) {
			// We got a reprieve.  This happens when a client receives a STATUS message
			// with a status code OSRF_STATUS_CONTINUE.  We restart the timer from the
			// beginning -- but only once.  We reset reset_timeout to zero. so that a
			// second attempted reprieve will allow, at most, only one more second.
			remaining = (time_t) timeout;
			req->reset_timeout = 0;
			osrfLogDebug( OSRF_LOG_MARK, "Received a timeout reset");
		} else {
			remaining -= (int) (time(NULL) - start);
		}
	}

	// Timeout exhausted; no messages for the request in question
	char* paramString = jsonObjectToJSON(req->payload->_params);
	osrfLogInfo( OSRF_LOG_MARK, "Returning NULL from app_request_recv after timeout: %s %s",
		req->payload->method_name, paramString);
	free(paramString);

	return NULL;
}

// --------------------------------------------------------------------------
// Session API
// --------------------------------------------------------------------------

/**
	@brief Install a copy of a locale string in a specified session.
	@param session Pointer to the osrfAppSession in which the locale is to be installed.
	@param locale The locale string to be copied and installed.
	@return A pointer to the installed copy of the locale string.
*/
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

/**
	@brief Create an osrfAppSession for a client.
	@param remote_service Name of the service to which to connect
	@return Pointer to the new osrfAppSession if successful, or NULL upon error.

	Allocate memory for an osrfAppSession, and initialize it as follows:

	- For talking with Jabber, grab an existing transport_client.  It must have been
	already set up by a prior call to osrfSystemBootstrapClientResc().
	- Build a Jabber ID for addressing the service.
	- Build a session ID based on a fine-grained timestamp and a process ID.  This ID is
	intended to be unique across the system, but uniqueness is not strictly guaranteed.
	- Initialize various other bits and scraps.
	- Add the session to the global session cache.

	Do @em not connect to the service at this point.
*/
osrfAppSession* osrfAppSessionClientInit( const char* remote_service ) {

	if (!remote_service) {
		osrfLogWarning( OSRF_LOG_MARK, "No remote service specified in osrfAppSessionClientInit");
		return NULL;
	}

	osrfAppSession* session = safe_malloc(sizeof(osrfAppSession));

	// Grab an existing transport_client for talking with Jabber
	session->transport_handle = osrfSystemGetTransportClient();
	if( session->transport_handle == NULL ) {
		osrfLogWarning( OSRF_LOG_MARK, "No transport client for service 'client'");
		free( session );
		return NULL;
	}

	// Get a list of domain names from the config settings;
	// ignore all but the first one in the list.
	osrfStringArray* arr = osrfNewStringArray(8);
	osrfConfigGetValueList(NULL, arr, "/domain");
	const char* domain = osrfStringArrayGetString(arr, 0);
	if (!domain) {
		osrfLogWarning( OSRF_LOG_MARK, "No domains specified in the OpenSRF config file");
		free( session );
		osrfStringArrayFree(arr);
		return NULL;
	}

	// Get a router name from the config settings.
	char* router_name = osrfConfigGetValue(NULL, "/router_name");
	if (!router_name) {
		osrfLogWarning( OSRF_LOG_MARK, "No router name specified in the OpenSRF config file");
		free( session );
		osrfStringArrayFree(arr);
		return NULL;
	}

	char target_buf[512];
	target_buf[ 0 ] = '\0';

	// Using the router name, domain, and service name,
	// build a Jabber ID for addressing the service.
	int len = snprintf( target_buf, sizeof(target_buf), "%s@%s/%s",
			router_name ? router_name : "(null)",
			domain ? domain : "(null)",
			remote_service ? remote_service : "(null)" );
	osrfStringArrayFree(arr);
	free(router_name);

	if( len >= sizeof( target_buf ) ) {
		osrfLogWarning( OSRF_LOG_MARK, "Buffer overflow for remote_id");
		free( session );
		return NULL;
	}

	session->remote_id = strdup(target_buf);
	session->orig_remote_id = strdup(session->remote_id);
	session->remote_service = strdup(remote_service);
	session->session_locale = NULL;
	session->transport_error = 0;
	session->panic = 0;
	session->outbuf = NULL;   // Not used by client

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

	// Initialize the hash table
	int i;
	for( i = 0; i < OSRF_REQUEST_HASH_SIZE; ++i )
		session->request_hash[ i ] = NULL;

	_osrf_app_session_push_session( session );
	return session;
}

/**
	@brief Create an osrfAppSession for a server.
	@param session_id The session ID.  In practice this comes from the thread member of
	the transport message from the client.
	@param our_app The name of the service being provided.
	@param remote_id Jabber ID of the client.
	@return Pointer to the newly created osrfAppSession if successful, or NULL upon failure.

	If there is already a session with the specified id, report an error.  Otherwise:

	- Allocate memory for an osrfAppSession.
	- For talking with Jabber, grab an existing transport_client.  It should have been
	already set up by a prior call to osrfSystemBootstrapClientResc().
	- Install a copy of the @a our_app string as remote_service.
	- Install copies of the @a remote_id string as remote_id and orig_remote_id.
	- Initialize various other bits and scraps.
	- Add the session to the global session cache.

	Do @em not respond to the client at this point.
*/
osrfAppSession* osrf_app_server_session_init(
		const char* session_id, const char* our_app, const char* remote_id ) {

	osrfLogDebug( OSRF_LOG_MARK, "Initing server session with session id %s, service %s,"
			" and remote_id %s", session_id, our_app, remote_id );

	osrfAppSession* session = osrf_app_session_find_session( session_id );
	if(session) {
		osrfLogWarning( OSRF_LOG_MARK, "App session already exists for session id %s",
				session_id );
		return NULL;
	}

	session = safe_malloc(sizeof(osrfAppSession));

	// Grab an existing transport_client for talking with Jabber
	session->transport_handle = osrfSystemGetTransportClient();
	if( session->transport_handle == NULL ) {
		osrfLogWarning( OSRF_LOG_MARK, "No transport client for service '%s'", our_app );
		free(session);
		return NULL;
	}

	// Decide from a config setting whether the session is stateless or not.  However
	// this determination is pointless because it will immediately be overruled according
	// to the compile-time macro ASSUME_STATELESS.
	int stateless = 0;
	char* statel = osrf_settings_host_value("/apps/%s/stateless", our_app );
	if( statel )
		stateless = atoi( statel );
	free(statel);

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
	session->transport_error = 0;

	// Initialize the hash table
	int i;
	for( i = 0; i < OSRF_REQUEST_HASH_SIZE; ++i )
		session->request_hash[ i ] = NULL;

	session->panic = 0;
	session->outbuf = buffer_init( 4096 );

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

	DEPRECATED.  Use osrfAppSessionSendRequest() instead.  It is identical except that it
	doesn't use the param_strings argument, which is redundant, confusing, and unused.

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

	osrfLogWarning( OSRF_LOG_MARK, "Function osrfAppSessionMakeRequest() is deprecasted; "
			"call osrfAppSessionSendRequest() instead" );
	return osrfAppSessionMakeLocaleRequest( session, params,
			method_name, protocol, param_strings, NULL );
}

/**
	@brief Create a REQUEST message, send it, and save it for future reference.
	@param session Pointer to the current session, which has the addressing information.
	@param params One way of specifying the parameters for the method.
	@param method_name The name of the method to be called.
	@param protocol Protocol.
	@return The request ID of the resulting REQUEST message, or -1 upon error.

	If @a params points to a JSON_ARRAY, then pass each element of the array as a separate
	parameter.  If @a params points to any other kind of jsonObject, pass it as a single
	parameter.

	This function is a thin wrapper for osrfAppSessionMakeLocaleRequest().
*/
int osrfAppSessionSendRequest( osrfAppSession* session, const jsonObject* params,
		const char* method_name, int protocol ) {

	return osrfAppSessionMakeLocaleRequest( session, params,
		 method_name, protocol, NULL, NULL );
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

	See the discussion of osrfAppSessionSendRequest(), which at this writing is the only
	place that calls this function, except for the similar but deprecated function
	osrfAppSessionMakeRequest().

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
	add_app_request( session, req );
	return req->request_id;
}

/**
	@brief Mark an osrfAppRequest (identified by session and ID) as complete.
	@param session Pointer to the osrfAppSession that owns the request.
	@param request_id Request ID of the osrfAppRequest.
*/
void osrf_app_session_set_complete( osrfAppSession* session, int request_id ) {
	if(session == NULL)
		return;

	osrfAppRequest* req = find_app_request( session, request_id );
	if(req)
		req->complete = 1;
}

/**
	@brief Determine whether a osrfAppRequest, identified by session and ID, is complete.
	@param session Pointer to the osrfAppSession that owns the request.
	@param request_id Request ID of the osrfAppRequest.
	@return Non-zero if the request is complete; zero if it isn't, or if it can't be found.
*/
int osrf_app_session_request_complete( const osrfAppSession* session, int request_id ) {
	if(session == NULL)
		return 0;

	osrfAppRequest* req = find_app_request( session, request_id );
	if(req)
		return req->complete;

	return 0;
}

/**
	@brief Reset the remote ID of a session to its original remote ID.
	@param session Pointer to the osrfAppSession to be reset.
*/
void osrf_app_session_reset_remote( osrfAppSession* session ){
	if( session==NULL )
		return;

	osrfLogDebug( OSRF_LOG_MARK,  "App Session [%s] [%s] resetting remote id to %s",
			session->remote_service, session->session_id, session->orig_remote_id );

	osrf_app_session_set_remote( session, session->orig_remote_id );
}

/**
	@brief Set a session's remote ID to a specified value.
	@param session Pointer to the osrfAppSession whose remote ID is to be set.
	@param remote_id Pointer to the new remote id.
*/
void osrf_app_session_set_remote( osrfAppSession* session, const char* remote_id ) {
	if( session == NULL || remote_id == NULL )
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
	@brief Append an osrfMessage to the list of responses to an osrfAppRequest.
	@param session Pointer to the osrfAppSession that owns the request.
	@param msg Pointer to the osrfMessage to be added.

	The thread_trace member of the osrfMessage is the request_id of the osrfAppRequest.
	Find the corresponding request in the session and append the osrfMessage to its list.
*/
void osrf_app_session_push_queue( osrfAppSession* session, osrfMessage* msg ) {
	if( session && msg ) {
		osrfAppRequest* req = find_app_request( session, msg->thread_trace );
		if( req )
			_osrf_app_request_push_queue( req, msg );
	}
}

/**
	@brief Connect to the remote service.
	@param session Pointer to the osrfAppSession for the service.
	@return 1 if successful, or 0 if not.

	If already connected, exit immediately, reporting success.  Otherwise, build a CONNECT
	message and send it to the service.  Wait for up to five seconds for an acknowledgement.

	The timeout value is currently hard-coded.  Perhaps it should be configurable.
*/
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

	// Address this message to the router
	osrf_app_session_reset_remote( session );
	session->state = OSRF_SESSION_CONNECTING;
	int ret = _osrf_app_session_send( session, con_msg );
	osrfMessageFree(con_msg);
	if(ret)
		return 0;

	time_t start = time(NULL);
	time_t remaining = (time_t) timeout;

	// Wait for the acknowledgement.  We look for it repeatedly because, under the covers,
	// we may receive and process messages other than the one we're looking for.
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

/**
	@brief Disconnect from the remote service.  No response is expected.
	@param session Pointer to the osrfAppSession to be disconnected.
	@return 1 in all cases.

	If we're already disconnected, return immediately without doing anything.  Likewise if
	we have a stateless session and we're in the process of connecting.  Otherwise, send a
	DISCONNECT message to the service.
*/
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

/**
	@brief Resend a request message, as specified by session and request id.
	@param session Pointer to the osrfAppSession.
	@param req_id Request ID for the request to be resent.
	@return Zero if successful, or if the specified request cannot be found; 1 if the
	request is already complete, or if the attempt to resend the message fails.

	The choice of return codes may seem seem capricious, but at this writing nothing
	pays any attention to the return code anyway.
*/
int osrf_app_session_request_resend( osrfAppSession* session, int req_id ) {
	osrfAppRequest* req = find_app_request( session, req_id );

	int rc;
	if(req == NULL) {
		rc = 0;
	} else if(!req->complete) {
		osrfLogDebug( OSRF_LOG_MARK, "Resending request [%d]", req->request_id );
		rc = _osrf_app_session_send( req->session, req->payload );
	} else {
		rc = 1;
	}

	return rc;
}

/**
	@brief Send one or more osrfMessages to the remote service or client.
	@param session Pointer to the osrfAppSession responsible for sending the message(s).
	@param msgs Pointer to an array of pointers to osrfMessages.
	@param size How many messages to send.
	@return 0 upon success, or -1 upon failure.
*/
static int osrfAppSessionSendBatch( osrfAppSession* session, osrfMessage* msgs[], int size ) {

	if( !(session && msgs && size > 0) ) return -1;
	int retval = 0;

	osrfMessage* msg = msgs[0];

	if(msg) {

		// First grab and process any input messages, for any app session.  This gives us
		// a chance to see any CONNECT or DISCONNECT messages that may have arrived.  We
		// may also see some unrelated messages, but we have to process those sooner or
		// later anyway, so we might as well do it now.
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
						return -1;
				}
			}
		}
	}

	// Translate the collection of osrfMessages into a JSON array
	char* string = osrfMessageSerializeBatch(msgs, size);

	// Send the JSON as the payload of a transport_message
	if( string ) {
		retval = osrfSendTransportPayload( session, string );
		free(string);
	}

	return retval;
}

/**
	@brief Wrap a given string in a transport message and send it.
	@param session Pointer to the osrfAppSession responsible for sending the message(s).
	@param payload A string to be sent via Jabber.
	@return 0 upon success, or -1 upon failure.

	In practice the payload is normally a JSON string, but this function assumes nothing
	about it.
*/
int osrfSendTransportPayload( osrfAppSession* session, const char* payload ) {
	transport_message* t_msg = message_init(
		payload, "", session->session_id, session->remote_id, NULL );
	message_set_osrf_xid( t_msg, osrfLogGetXid() );

	int retval = client_send_message( session->transport_handle, t_msg );
	if( retval )
		osrfLogError( OSRF_LOG_MARK, "client_send_message failed" );

	osrfLogInfo(OSRF_LOG_MARK, "[%s] sent %d bytes of data to %s",
		session->remote_service, strlen( payload ), t_msg->recipient );

	osrfLogDebug( OSRF_LOG_MARK, "Sent: %s", payload );

	message_free( t_msg );
	return retval;
}

/**
	@brief Send a single osrfMessage to the remote service or client.
	@param session Pointer to the osrfAppSession.
	@param msg Pointer to the osrfMessage to be sent.
	@return zero upon success, or 1 upon failure.

	A thin wrapper.  Create an array of one element, and pass it to osrfAppSessionSendBatch().
*/
static int _osrf_app_session_send( osrfAppSession* session, osrfMessage* msg ){
	if( !(session && msg) )
		return 1;
	osrfMessage* a[1];
	a[0] = msg;
	return  - osrfAppSessionSendBatch( session, a, 1 );
}


/**
	@brief Wait for any input messages to arrive, and process them as needed.
	@param session Pointer to the osrfAppSession whose transport_session we will use.
	@param timeout How many seconds to wait for the first input message.
	@param recvd Pointer to an boolean int.  If you receive at least one message, set the boolean
	to true; otherwise set it to false.
	@return 0 upon success (even if a timeout occurs), or -1 upon failure.

	A thin wrapper for osrf_stack_process().  The timeout applies only to the first
	message; process subsequent messages if they are available, but don't wait for them.

	The first parameter identifies an osrfApp session, but all we really use it for is to
	get a pointer to the transport_session.  Typically, a given process opens only a single
	transport_session (to talk to the Jabber server), and all app sessions in that process
	use the same transport_session.

	Hence this function indiscriminately waits for input messages for all osrfAppSessions
	tied to the same Jabber session, not just the one specified.

	Dispatch each message to the appropriate processing routine, depending on its type
	and contents, and on whether we're acting as a client or as a server for that message.
	For example, a response to a request may be appended to the input queue of the
	relevant request.  A server session receiving a REQUEST message may execute the
	requested method.  And so forth.
*/
int osrf_app_session_queue_wait( osrfAppSession* session, int timeout, int* recvd ){
	if(session == NULL) return 0;
	osrfLogDebug(OSRF_LOG_MARK, "AppSession in queue_wait with timeout %d", timeout );
	return osrf_stack_process(session->transport_handle, timeout, recvd);
}

/**
	@brief Shut down and destroy an osrfAppSession.
	@param session Pointer to the osrfAppSession to be destroyed.

	If this is a client session, send a DISCONNECT message.

	Remove the session from the global session cache.

	Free all associated resources, including any pending osrfAppRequests.
*/
void osrfAppSessionFree( osrfAppSession* session ){
	if(session == NULL) return;

	/* Disconnect */

	osrfLogDebug(OSRF_LOG_MARK,  "AppSession [%s] [%s] destroying self and deleting requests",
			session->remote_service, session->session_id );
	/* disconnect if we're a client */
	if(session->type == OSRF_SESSION_CLIENT
			&& session->state != OSRF_SESSION_DISCONNECTED ) {
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

	// Free the request hash
	int i;
	for( i = 0; i < OSRF_REQUEST_HASH_SIZE; ++i ) {
		osrfAppRequest* app = session->request_hash[ i ];
		while( app ) {
			osrfAppRequest* next = app->next;
			_osrf_app_request_free( app );
			app = next;
		}
	}

	if( session->outbuf )
		buffer_free( session->outbuf );

	free(session);
}

/**
	@brief Wait for a response to a given request, subject to a timeout.
	@param session Pointer to the osrfAppSession that owns the request.
	@param req_id Request ID for the request.
	@param timeout How many seconds to wait.
	@return A pointer to the received osrfMessage if one arrives; otherwise NULL.

	A thin wrapper.  Given a session and a request ID, look up the corresponding request
	and pass it to _osrf_app_request_recv().
*/
osrfMessage* osrfAppSessionRequestRecv(
		osrfAppSession* session, int req_id, int timeout ) {
	if(req_id < 0 || session == NULL)
		return NULL;
	osrfAppRequest* req = find_app_request( session, req_id );
	return _osrf_app_request_recv( req, timeout );
}

/**
	@brief In response to a specified request, send a payload of data to a client.
	@param ses Pointer to the osrfAppSession that owns the request.
	@param requestId Request ID of the osrfAppRequest.
	@param data Pointer to a jsonObject containing the data payload.
	@return 0 upon success, or -1 upon failure.

	Translate the jsonObject to a JSON string, and send it wrapped in a RESULT message.

	The only failure detected is if either of the two pointer parameters is NULL.
*/
int osrfAppRequestRespond( osrfAppSession* ses, int requestId, const jsonObject* data ) {
	if( !ses || ! data )
		return -1;

	osrfMessage* msg = osrf_message_init( RESULT, requestId, 1 );
	osrf_message_set_status_info( msg, NULL, "OK", OSRF_STATUS_OK );
	char* json = jsonObjectToJSON( data );

	osrf_message_set_result_content( msg, json );
	_osrf_app_session_send( ses, msg );

	free(json);
	osrfMessageFree( msg );

	return 0;
}


/**
	@brief Send one or two messages to a client in response to a specified request.
	@param ses Pointer to the osrfAppSession that owns the request.
	@param requestId Request ID of the osrfAppRequest.
	@param data Pointer to a jsonObject containing the data payload.
	@return  Zero in all cases.

	If the @a data parameter is not NULL, translate the jsonObject into a JSON string, and
	incorporate that string into a RESULT message as as the payload .  Also build a STATUS
	message indicating that the response is complete.  Send both messages bundled together
	in the same transport_message.

	If the @a data parameter is NULL, send only a STATUS message indicating that the response
	is complete.
*/
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

/**
	@brief Send a STATUS message, for a specified request, back to the client.
	@param ses Pointer to the osrfAppSession connected to the client.
	@param type A numeric code denoting the status.
	@param name A string naming the status.
	@param reqId The request ID of the request.
	@param message A brief message describing the status.
	@return 0 upon success, or -1 upon failure.

	The only detected failure is when the @a ses parameter is NULL.
*/
int osrfAppSessionStatus( osrfAppSession* ses, int type,
		const char* name, int reqId, const char* message ) {

	if(ses) {
		osrfMessage* msg = osrf_message_init( STATUS, reqId, 1);
		osrf_message_set_status_info( msg, name, message, type );
		_osrf_app_session_send( ses, msg );
		osrfMessageFree( msg );
		return 0;
	} else
		return -1;
}

/**
	@brief Free the global session cache.

	Note that the osrfHash that implements the global session cache does @em not have a
	callback function installed for freeing its cargo.  As a result, any remaining
	osrfAppSessions are leaked, along with all the osrfAppRequests and osrfMessages they
	own.
*/
void osrfAppSessionCleanup( void ) {
	osrfHashFree(osrfAppSessionCache);
	osrfAppSessionCache = NULL;
}

/**
	@brief Arrange for immediate termination of the process.
	@param ses Pointer to the current osrfAppSession.

	Typical use case: a server drone loses its database connection, thereby becoming useless.
	It terminates so that it will not receive further requests, being unable to service them.
*/
void osrfAppSessionPanic( osrfAppSession* ses ) {
	if( ses )
		ses->panic = 1;
}
