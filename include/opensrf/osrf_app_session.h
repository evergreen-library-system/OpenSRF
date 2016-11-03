/**
	@file osrf_app_session.h
	@brief Header for osrfAppSession.
*/

#ifndef OSRF_APP_SESSION_H
#define OSRF_APP_SESSION_H

#include "opensrf/transport_client.h"
#include "opensrf/osrf_message.h"
#include "opensrf/osrf_system.h"
#include "opensrf/string_array.h"
#include "opensrf/osrfConfig.h"
#include "opensrf/osrf_hash.h"
#include "opensrf/osrf_list.h"
#include "opensrf/osrf_json.h"

#ifdef __cplusplus
extern "C" {
#endif

enum OSRF_SESSION_STATE {
	OSRF_SESSION_CONNECTING,
	OSRF_SESSION_CONNECTED,
	OSRF_SESSION_DISCONNECTED
};

enum OSRF_SESSION_TYPE {
	OSRF_SESSION_SERVER,
	OSRF_SESSION_CLIENT
};

struct osrf_app_request_struct;
typedef struct osrf_app_request_struct osrfAppRequest;

#define OSRF_REQUEST_HASH_SIZE 64

/**
	@brief Default size of output buffer.
*/
#define OSRF_MSG_BUNDLE_SIZE 25600 /* 25K */
#define OSRF_MSG_CHUNK_SIZE  (OSRF_MSG_BUNDLE_SIZE * 2)

/**
	@brief Representation of a session with another application.

	An osrfAppSession is a list of lists.  It includes a list of osrfAppRequests
	representing outstanding requests.  Each osrfAppRequest includes a list of
	responses.
*/
struct osrf_app_session_struct {

	/** Our message passing object */
	transport_client* transport_handle;

	/** The original remote id of the remote service we're talking to */
	char* orig_remote_id;
	/** The current remote id of the remote service we're talking to */
	char* remote_id;

	/** Whom we're talking to if we're a client;
		what app we're serving if we're a server */
	char* remote_service;

	/** The current request thread_trace */
	int thread_trace;
	/** Our ID */
	char* session_id;

	/** Boolean; true if this session does not require connect messages.
	    Assigned a value depending on the compile-time macro ASSUME_STATELESS. */
	int stateless;

	/** The connect state */
	enum OSRF_SESSION_STATE state;

	/** SERVER or CLIENT */
	enum OSRF_SESSION_TYPE type;

	/** the current locale for this session. **/
	char* session_locale;

	/** the current TZ for this session. **/
	char* session_tz;

	/** let the user use the session to store their own session data. */
	void* userData;

	/** Callback function for freeing user's session data. */
	void (*userDataFree) (void*);

	int transport_error;

	/** Hash table of pending requests. */
	osrfAppRequest* request_hash[ OSRF_REQUEST_HASH_SIZE ];

	/** Boolean: true if the app wants to terminate the process.  Typically this means that */
	/** a drone has lost its database connection and can therefore no longer function.      */
	int panic;

	/** Buffer used by server drone to collect outbound response messages */
	growing_buffer* outbuf;
};
typedef struct osrf_app_session_struct osrfAppSession;

// --------------------------------------------------------------------------
// PUBLIC API ***
// --------------------------------------------------------------------------

osrfAppSession* osrfAppSessionClientInit( const char* remote_service );

osrfAppSession* osrf_app_server_session_init(
		const char* session_id, const char* our_app, const char* remote_id );

char* osrf_app_session_set_locale( osrfAppSession*, const char* );

char* osrf_app_session_set_tz( osrfAppSession*, const char* );

/* ingress used by all sessions until replaced */
char* osrfAppSessionSetIngress( const char* );

const char* osrfAppSessionGetIngress();

osrfAppSession* osrf_app_session_find_session( const char* session_id );

/* DEPRECATED; use osrfAppSessionSendRequest() instead. */
int osrfAppSessionMakeRequest(
		osrfAppSession* session, const jsonObject* params,
		const char* method_name, int protocol, osrfStringArray* param_strings);

int osrfAppSessionSendRequest(
		 osrfAppSession* session, const jsonObject* params,
		 const char* method_name, int protocol );

void osrf_app_session_set_complete( osrfAppSession* session, int request_id );

int osrf_app_session_request_complete( const osrfAppSession* session, int request_id );

osrfMessage* osrfAppSessionRequestRecv(
		osrfAppSession* session, int request_id, int timeout );

void osrf_app_session_request_finish( osrfAppSession* session, int request_id );

int osrf_app_session_request_resend( osrfAppSession*, int request_id );

int osrfSendChunkedResult(
		osrfAppSession* session, int request_id, const char* payload,
		size_t payload_size, size_t chunk_size );

int osrfSendTransportPayload( osrfAppSession* session, const char* payload );

void osrf_app_session_reset_remote( osrfAppSession* );

void osrf_app_session_set_remote( osrfAppSession* session, const char* remote_id );

void osrf_app_session_push_queue( osrfAppSession*, osrfMessage* msg );

int osrfAppSessionConnect( osrfAppSession* );

int osrf_app_session_disconnect( osrfAppSession* );

int osrf_app_session_queue_wait( osrfAppSession*, int timeout, int* recvd );

void osrfAppSessionFree( osrfAppSession* );

void osrf_app_session_request_reset_timeout( osrfAppSession* session, int req_id );

int osrfAppRequestRespond( osrfAppSession* ses, int requestId, const jsonObject* data );

int osrfAppRequestRespondComplete(
		osrfAppSession* ses, int requestId, const jsonObject* data );

int osrfAppSessionStatus( osrfAppSession* ses, int type,
		const char* name, int reqId, const char* message );

void osrfAppSessionCleanup( void );

void osrfAppSessionPanic( osrfAppSession* ses );

#ifdef __cplusplus
}
#endif

#endif
