#ifndef osrf_message_h
#define osrf_message_h

#include <opensrf/string_array.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>


/* libxml stuff for the config reader */
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/tree.h>

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

	char* full_param_string;

	/* magical LOCALE hint */
	char* sender_locale;

	/* timezone offset from GMT of sender, in seconds */
	int sender_tz_offset;

};
typedef struct osrf_message_struct osrfMessage;

/* Set the locale hint for this message.
   default_locale is used if not set.
   Returns NULL if msg or locale is not set, char* to msg->sender_locale on success.
*/
char* osrf_message_set_locale( osrfMessage* msg, const char* locale );

/* Set the default locale hint to be used for future outgoing messages.
   Returns NULL if locale is NULL, const char* to default_locale otherwise.
*/
const char* osrf_message_set_default_locale( const char* locale );

/* Get the current locale hint -- either the default or most recently received locale.
   Returns const char* to current_locale.
*/
const char* osrf_message_get_last_locale(void);

osrfMessage* osrf_message_init( enum M_TYPE type, int thread_trace, int protocol );
//void osrf_message_set_request_info( osrfMessage*, char* param_name, json* params );
void osrf_message_set_status_info( osrfMessage*,
		const char* status_name, const char* status_text, int status_code );
void osrf_message_set_result_content( osrfMessage*, const char* json_string );
void osrfMessageFree( osrfMessage* );
char* osrf_message_to_xml( osrfMessage* );
char* osrf_message_serialize(const osrfMessage*);

/* count is the max number of messages we'll put into msgs[] */
int osrf_message_deserialize(const char* json, osrfMessage* msgs[], int count);



/** Pushes any message retreived from the xml into the 'msgs' array.
  * it is assumed that 'msgs' has beenn pre-allocated.
  * Returns the number of message that are in the buffer.
  */
int osrf_message_from_xml( char* xml, osrfMessage* msgs[] );

void osrf_message_set_params( osrfMessage* msg, const jsonObject* o );
void osrf_message_set_method( osrfMessage* msg, const char* method_name );
void osrf_message_add_object_param( osrfMessage* msg, const jsonObject* o );
void osrf_message_add_param( osrfMessage*, const char* param_string );


jsonObject* osrfMessageGetResult( osrfMessage* msg );

/**
  Returns the message as a jsonObject
  @return The jsonObject which must be freed by the caller.
  */
jsonObject* osrfMessageToJSON( const osrfMessage* msg );

char* osrfMessageSerializeBatch( osrfMessage* msgs [], int count );

#ifdef __cplusplus
}
#endif

#endif
