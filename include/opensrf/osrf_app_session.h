#ifndef _OSRF_APP_SESSION
#define _OSRF_APP_SESSION

#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_system.h>
#include <opensrf/string_array.h>
#include <opensrf/osrfConfig.h>
#include <opensrf/osrf_hash.h>
#include <opensrf/osrf_list.h>

#include <opensrf/osrf_json.h>



#define	DEF_RECV_TIMEOUT 6 /* receive timeout */
#define	DEF_QUEUE_SIZE	

enum OSRF_SESSION_STATE { OSRF_SESSION_CONNECTING, OSRF_SESSION_CONNECTED, OSRF_SESSION_DISCONNECTED };
enum OSRF_SESSION_TYPE { OSRF_SESSION_SERVER, OSRF_SESSION_CLIENT };

/* entry point for data into the stack.  gets set in osrf_stack.c */
int (*osrf_stack_entry_point) (transport_client* client, int timeout, int* recvd );

struct osrf_app_request_struct {
	/** Our controlling session */
	struct osrf_app_session_struct* session;

	/** our "id" */
	int request_id;
	/** True if we have received a 'request complete' message from our request */
	int complete;
	/** Our original request payload */
	osrf_message* payload; 
	/** List of responses to our request */
	osrf_message* result;

	/* if set to true, then a call that is waiting on a response, will reset the 
		timeout and set this variable back to false */
	int reset_timeout;
};
typedef struct osrf_app_request_struct osrf_app_request;
typedef struct osrf_app_request_struct osrfAppRequest;

struct osrf_app_session_struct {

	/** Our messag passing object */
	transport_client* transport_handle;
	/** Cache of active app_request objects */

	//osrfAppRequest* request_queue;

	osrfList* request_queue;

	/** The original remote id of the remote service we're talking to */
	char* orig_remote_id;
	/** The current remote id of the remote service we're talking to */
	char* remote_id;

	/** Who we're talking to if we're a client.  
		what app we're serving if we're a server */
	char* remote_service;

	/** The current request thread_trace */
	int thread_trace;
	/** Our ID */
	char* session_id;

	/* true if this session does not require connect messages */
	int stateless;

	/** The connect state */
	enum OSRF_SESSION_STATE state;

	/** SERVER or CLIENT */
	enum OSRF_SESSION_TYPE type;

	/** the current locale for this session **/
	char* session_locale;

	/* let the user use the session to store their own session data */
	void* userData;

	void (*userDataFree) (void*);
};
typedef struct osrf_app_session_struct osrf_app_session;
typedef struct osrf_app_session_struct osrfAppSession;



// -------------------------------------------------------------------------- 
// PUBLIC API ***
// -------------------------------------------------------------------------- 

/** Allocates a initializes a new app_session */
osrfAppSession* osrfAppSessionClientInit( const char* remote_service );
osrfAppSession* osrf_app_client_session_init( const char* remote_service );

/** Allocates and initializes a new server session.  The global session cache
  * is checked to see if this session already exists, if so, it's returned 
  */
osrfAppSession* osrf_app_server_session_init(
		const char* session_id, const char* our_app, const char* remote_id );

/** sets the default locale for a session **/
char* osrf_app_session_set_locale( osrfAppSession*, const char* );

/** returns a session from the global session hash */
osrfAppSession* osrf_app_session_find_session( const char* session_id );

/** Builds a new app_request object with the given payload andn returns
  * the id of the request.  This id is then used to perform work on the
  * requeset.
  */
int osrfAppSessionMakeRequest(
		osrfAppSession* session, const jsonObject* params,
		const char* method_name, int protocol, string_array* param_strings);

int osrf_app_session_make_req( 
		osrfAppSession* session, const jsonObject* params,
		const char* method_name, int protocol, string_array* param_strings);

/** Sets the given request to complete state */
void osrf_app_session_set_complete( osrfAppSession* session, int request_id );

/** Returns true if the given request is complete */
int osrf_app_session_request_complete( const osrfAppSession* session, int request_id );

/** Does a recv call on the given request */
osrf_message* osrfAppSessionRequestRecv(
		osrfAppSession* session, int request_id, int timeout );
osrf_message* osrf_app_session_request_recv( 
		osrfAppSession* session, int request_id, int timeout );

/** Removes the request from the request set and frees the reqest */
void osrf_app_session_request_finish( osrfAppSession* session, int request_id );

/** Resends the orginal request with the given request id */
int osrf_app_session_request_resend( osrfAppSession*, int request_id );

/** Resets the remote connection target to that of the original*/
void osrf_app_session_reset_remote( osrfAppSession* );

/** Sets the remote target to 'remote_id' */
void osrf_app_session_set_remote( osrfAppSession* session, const char* remote_id );

/** pushes the given message into the result list of the app_request
  * whose request_id matches the messages thread_trace 
  */
int osrf_app_session_push_queue( osrfAppSession*, osrf_message* msg );

/** Attempts to connect to the remote service. Returns 1 on successful 
  * connection, 0 otherwise.
  */
int osrf_app_session_connect( osrfAppSession* );
int osrfAppSessionConnect( osrfAppSession* );

/** Sends a disconnect message to the remote service.  No response is expected */
int osrf_app_session_disconnect( osrfAppSession* );

/**  Waits up to 'timeout' seconds for some data to arrive.
  * Any data that arrives will be processed according to its
  * payload and message type.  This method will return after
  * any data has arrived.
  */
int osrf_app_session_queue_wait( osrfAppSession*, int timeout, int* recvd );

/** Disconnects (if client), frees any attached app_reuqests, removes the session from the 
  * global session cache and frees the session.  Needless to say, only call this when the
  * session is completey done.
  */
void osrf_app_session_destroy ( osrfAppSession* );
void osrfAppSessionFree( osrfAppSession* );

/* tells the request to reset it's wait timeout */
void osrf_app_session_request_reset_timeout( osrfAppSession* session, int req_id );

int osrfAppRequestRespond( osrfAppSession* ses, int requestId, const jsonObject* data );
int osrfAppRequestRespondComplete(
		osrfAppSession* ses, int requestId, const jsonObject* data ); 

int osrfAppSessionStatus( osrfAppSession* ses, int type,
		const char* name, int reqId, const char* message );

void osrfAppSessionCleanup();



#endif
