#ifndef osrf_message_h
#define osrf_message_h

/**
	@file osrf_message.h
	@brief Header for osrfMessage

	An osrfMessage is the in-memory representation of a message between applications.

	For transmission, one or more messages are encoded in a JSON array and wrapped in a
	transport_message, which (together with its JSON cargo) is translated into XML as
	a Jabber message.

	There are five kinds of messages:
	- CONNECT -- request to establish a stateful session.
	- DISCONNECT -- ends a stateful session.
	- REQUEST -- a remote procedure call.
	- RESULT -- data returned by a remote procedure call.
	- STATUS -- reports the success or failure of a requested operation.

	Different kinds of messages use different combinations of the members of an osrfMessage.
*/

#include <opensrf/string_array.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSRF_XML_NAMESPACE "http://open-ils.org/xml/namespaces/oils_v1"

#define OSRF_STATUS_CONTINUE             100

#define OSRF_STATUS_OK                   200
#define OSRF_STATUS_ACCEPTED             202
#define OSRF_STATUS_COMPLETE             205

#define OSRF_STATUS_REDIRECTED           307

#define OSRF_STATUS_BADREQUEST           400
#define OSRF_STATUS_UNAUTHORIZED         401
#define OSRF_STATUS_FORBIDDEN            403
#define OSRF_STATUS_NOTFOUND             404
#define OSRF_STATUS_NOTALLOWED           405
#define OSRF_STATUS_TIMEOUT              408
#define OSRF_STATUS_EXPFAILED            417

#define OSRF_STATUS_INTERNALSERVERERROR  500
#define OSRF_STATUS_NOTIMPLEMENTED       501
#define OSRF_STATUS_VERSIONNOTSUPPORTED  505


enum M_TYPE { CONNECT, REQUEST, RESULT, STATUS, DISCONNECT };

struct osrf_message_struct {

	/** One of the four message types: CONNECT, REQUEST, RESULT, STATUS, or DISCONNECT. */
	enum M_TYPE m_type;
	
	/** Used to keep track of which responses go with which requests. */
	int thread_trace;
	
	/** Currently serves no discernible purpose, but may be useful someday. */
	int protocol;

	/** Used for STATUS or RESULT messages. */
	char* status_name;

	/** Used for STATUS or RESULT messages. */
	char* status_text;

	/** Used for STATUS or RESULT messages. */
	int status_code;

	/** Boolean: true for some kinds of error conditions. */
	int is_exception;

	/** Used for RESULT messages: contains the data returned by a remote procedure. */
	jsonObject* _result_content;

	/** For a REQUEST message: name of the remote procedure to call. */
	char* method_name;

	/** For a REQUEST message: parameters to pass to the remote procedure call. */
	jsonObject* _params;

	/** Pointer for linked lists.  Used only by calling code. */
	struct osrf_message_struct* next;

	/** Magical LOCALE hint. */
	char* sender_locale;
};
typedef struct osrf_message_struct osrfMessage;

const char* osrf_message_set_locale( osrfMessage* msg, const char* locale );

const char* osrf_message_set_default_locale( const char* locale );

const char* osrf_message_get_last_locale(void);

osrfMessage* osrf_message_init( enum M_TYPE type, int thread_trace, int protocol );

void osrf_message_set_status_info( osrfMessage*,
		const char* status_name, const char* status_text, int status_code );

void osrf_message_set_result_content( osrfMessage*, const char* json_string );

void osrfMessageFree( osrfMessage* );

char* osrf_message_to_xml( osrfMessage* );

char* osrf_message_serialize(const osrfMessage*);

osrfList* osrfMessageDeserialize( const char* string, osrfList* list );

int osrf_message_deserialize(const char* json, osrfMessage* msgs[], int count);

void osrf_message_set_params( osrfMessage* msg, const jsonObject* o );

void osrf_message_set_method( osrfMessage* msg, const char* method_name );

void osrf_message_add_object_param( osrfMessage* msg, const jsonObject* o );

void osrf_message_add_param( osrfMessage*, const char* param_string );

jsonObject* osrfMessageGetResult( osrfMessage* msg );

char* osrfMessageSerializeBatch( osrfMessage* msgs [], int count );

#ifdef __cplusplus
}
#endif

#endif
