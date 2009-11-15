#ifndef osrf_message_h
#define osrf_message_h

/**
	@file osrf_message.h
	@brief Header for osrfMessage
*/

#include <opensrf/string_array.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSRF_XML_NAMESPACE "http://open-ils.org/xml/namespaces/oils_v1"

#define OSRF_STATUS_CONTINUE						100

#define OSRF_STATUS_OK								200
#define OSRF_STATUS_ACCEPTED						202
#define OSRF_STATUS_COMPLETE						205

#define OSRF_STATUS_REDIRECTED					307

#define OSRF_STATUS_BADREQUEST					400
#define OSRF_STATUS_UNAUTHORIZED					401
#define OSRF_STATUS_FORBIDDEN						403
#define OSRF_STATUS_NOTFOUND						404
#define OSRF_STATUS_NOTALLOWED					405
#define OSRF_STATUS_TIMEOUT						408
#define OSRF_STATUS_EXPFAILED						417

#define OSRF_STATUS_INTERNALSERVERERROR		500
#define OSRF_STATUS_NOTIMPLEMENTED				501
#define OSRF_STATUS_VERSIONNOTSUPPORTED		505


enum M_TYPE { CONNECT, REQUEST, RESULT, STATUS, DISCONNECT };

#define OSRF_MAX_PARAMS								128;

struct osrf_message_struct {

	enum M_TYPE m_type;
	int thread_trace;
	int protocol;

	/* if we're a STATUS message */
	char* status_name;

	/* if we're a STATUS or RESULT */
	char* status_text;
	int status_code;

	int is_exception;

	/* if we're a RESULT */
	jsonObject* _result_content;

	/* unparsed json string */
	char* result_string;

	/* if we're a REQUEST */
	char* method_name;

	jsonObject* _params;

	/* in case anyone wants to make a list of us.  
		we won't touch this variable */
	struct osrf_message_struct* next;

	/* magical LOCALE hint */
	char* sender_locale;

	/* timezone offset from GMT of sender, in seconds */
	int sender_tz_offset;

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
