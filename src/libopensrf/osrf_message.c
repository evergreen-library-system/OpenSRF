#include <opensrf/osrf_message.h>

/**
	@file osrf_message.c
	@brief Implementation of osrfMessage.
*/

static jsonObject* osrfMessageToJSON( const osrfMessage* msg );

static char default_locale[17] = "en-US\0\0\0\0\0\0\0\0\0\0\0\0";
static char* current_locale = NULL;

/**
	@brief Allocate and initialize an osrfMessage.
	@param type One of CONNECT, REQUEST, RESULT, STATUS, or DISCONNECT.
	@param thread_trace Thread trace.
	@param protocol Protocol.
	@return Pointer to the newly allocated osrfMessage.

	The calling code is responsible for freeing the osrfMessage by calling osrfMessageFree().
*/
osrfMessage* osrf_message_init( enum M_TYPE type, int thread_trace, int protocol ) {

	osrfMessage* msg            = (osrfMessage*) safe_malloc(sizeof(osrfMessage));
	msg->m_type                 = type;
	msg->thread_trace           = thread_trace;
	msg->protocol               = protocol;
	msg->status_name            = NULL;
	msg->status_text            = NULL;
	msg->status_code            = 0;
	msg->next                   = NULL;
	msg->is_exception           = 0;
	msg->_params                = NULL;
	msg->_result_content        = NULL;
	msg->result_string          = NULL;
	msg->method_name            = NULL;
	msg->sender_locale          = NULL;
	msg->sender_tz_offset       = 0;

	return msg;
}


/**
	@brief Return the previous locale.
	@return A pointer to the locale specified by the last message to have been deserialized,
		or NULL.

	A JSON message may specify a locale string, which is saved as the current locale.  If
	the message does not specify a locale string, then the current locale becomes NULL.

	This function returns a pointer to an internal buffer.  Since both the address and the
	contents of this buffer may change from one call to the next, the calling code should
	either use the result immediately or make a copy of the string for later use.
*/
const char* osrf_message_get_last_locale() {
	return current_locale;
}

/**
	@brief Set the locale for a specified osrfMessage.
	@param msg Pointer to the osrfMessage.
	@param locale Pointer to the locale string to be installed in the osrfMessage.
	@return Pointer to the new locale string for the osrfMessage, or NULL if either
		parameter is NULL.

	If no locale is specified for an osrfMessage, we use the default locale.

	Used for a REQUEST message.
*/
const char* osrf_message_set_locale( osrfMessage* msg, const char* locale ) {
	if( msg == NULL || locale == NULL )
		return NULL;
	if( msg->sender_locale )
		free( msg->sender_locale );
	return msg->sender_locale = strdup( locale );
}

/**
	@brief Change the default locale.
	@param locale The new default locale.
	@return A pointer to the new default locale if successful, or NULL if not.

	The function returns NULL if the parameter is NULL, or if the proposed new locale is
	longer than 16 characters.

	At this writing, nothing calls this function.
*/
const char* osrf_message_set_default_locale( const char* locale ) {
	if( locale == NULL ) return NULL;
	if( strlen(locale) > sizeof(default_locale) - 1 ) return NULL;

	strcpy( default_locale, locale );
	return default_locale;
}

/**
	@brief Populate the method_name member of an osrfMessage.
	@param msg Pointer to the osrfMessage.
	@param method_name The method name.

	Used for a REQUEST message to specify what method to invoke.
*/
void osrf_message_set_method( osrfMessage* msg, const char* method_name ) {
	if( msg == NULL || method_name == NULL )
		return;
	if( msg->method_name )
		free( msg->method_name );
	msg->method_name = strdup( method_name );
}


/**
	@brief Add a copy of a jsonObject to an osrfMessage as a parameter for a method call.
	@param msg Pointer to the osrfMessage.
	@param o Pointer to the jsonObject of which a copy is to be stored.

	Make a copy of the input jsonObject, with all classnames encoded with JSON_CLASS_KEY and
	JSON_DATA_KEY.  Append it to a JSON_ARRAY stored at msg->_params.

	If there is nothing at msg->_params, create a new JSON_ARRAY for it and add the new object
	as the first element.

	If the jsonObject is raw (i.e. the class information has not been decoded into classnames),
	decode it.  If the class information has already been decoded, discard it.

	See also osrf_message_add_param().

	At this writing, nothing calls this function.
*/
void osrf_message_add_object_param( osrfMessage* msg, const jsonObject* o ) {
	if(!msg|| !o) return;
	if(!msg->_params)
		msg->_params = jsonNewObjectType( JSON_ARRAY );
	jsonObjectPush(msg->_params, jsonObjectDecodeClass( o ));
}

/**
	@brief Populate the params member of an osrfMessage.
	@param msg Pointer to the osrfMessage.
	@param o Pointer to a jsonObject representing the parameter(s) to a method.

	Make a copy of a jsonObject and install it as the parameter list for a method call.

	If the osrfMessage already has any parameters, discard them.

	The @a o parameter should point to a jsonObject of type JSON_ARRAY, with each element
	of the array being a parameter.  If @a o points to any other type of jsonObject, create
	a JSON_ARRAY as a wrapper for it, and install a copy of @a o as its only element.

	Used for a REQUEST message, to pass parameters to a method.  The alternative is to call
	osrf_message_add_param() or osrf_message_add_object_param() repeatedly as needed to add
	one parameter at a time.
*/
void osrf_message_set_params( osrfMessage* msg, const jsonObject* o ) {
	if(!msg || !o) return;

	if(msg->_params)
		jsonObjectFree(msg->_params);

	if(o->type == JSON_ARRAY) {
		msg->_params = jsonObjectClone(o);
	} else {
		osrfLogDebug( OSRF_LOG_MARK, "passing non-array to osrf_message_set_params(), fixing...");
		jsonObject* clone = jsonObjectClone(o);
		msg->_params = jsonNewObjectType( JSON_ARRAY );
		jsonObjectPush(msg->_params, clone);
		return;
	}
}


/**
	@brief Add a JSON string to an osrfMessage as a parameter for a method call.
	@param msg Pointer to the osrfMessage.
	@param param_string A JSON string encoding the parameter to be added.

	Translate the JSON string into a jsonObject, and append it to the parameter list.
	If the parameter list doesn't already exist, create an empty one and add the new
	parameter to it.

	Decode any class information in the raw JSON into classnames.

	If the JSON string is not valid JSON, append a new jsonObject of type JSON_NULL.

	Used for a REQUEST message, to pass a parameter to a method,  The alternative is to
	call osrf_message_set_params() to provide all the parameters at once.  See also
	osrf_message_add_object_param().
*/
void osrf_message_add_param( osrfMessage* msg, const char* param_string ) {
	if(msg == NULL || param_string == NULL) return;
	if(!msg->_params) msg->_params = jsonNewObjectType( JSON_ARRAY );
	jsonObjectPush(msg->_params, jsonParseString(param_string));
}


/**
	@brief Set the status_name, status_text, and status_code members of an osrfMessage.
	@param msg Pointer to the osrfMessage to be populated.
	@param status_name Status name (may be NULL).
	@param status_text Status text (may be NULL).
	@param status_code Status code.

	If the @a status_name or @a status_text parameter is NULL, the corresponding member
	is left unchanged.

	Used for a RESULT or STATUS message.
*/
void osrf_message_set_status_info( osrfMessage* msg,
		const char* status_name, const char* status_text, int status_code ) {
	if(!msg) return;

	if( status_name != NULL ) {
		if( msg->status_name )
			free( msg->status_name );
		msg->status_name = strdup( status_name );
	}

	if( status_text != NULL ) {
		if( msg->status_text )
			free( msg->status_text );
		msg->status_text = strdup( status_text );
	}

	msg->status_code = status_code;
}


/**
	@brief Populate the result_string and _result_content members of an osrfMessage.
	@param msg Pointer to the osrfMessage to be populated.
	@param json_string A JSON string encoding a result set.

	Used for a RESULT message to return the results of a request, such as a database lookup.
*/
void osrf_message_set_result_content( osrfMessage* msg, const char* json_string ) {
	if( msg == NULL || json_string == NULL) return;
	if( msg->result_string )
		free( msg->result_string );
	if( msg->_result_content )
		jsonObjectFree( msg->_result_content );

	msg->result_string   = strdup(json_string);
	msg->_result_content = jsonParseString(json_string);
}


/**
	@brief Free an osrfMessage and everything it owns.
	@param msg Pointer to the osrfMessage to be freed.
*/
void osrfMessageFree( osrfMessage* msg ) {
	if( msg == NULL )
		return;

	if( msg->status_name != NULL )
		free(msg->status_name);

	if( msg->status_text != NULL )
		free(msg->status_text);

	if( msg->_result_content != NULL )
		jsonObjectFree( msg->_result_content );

	if( msg->result_string != NULL )
		free( msg->result_string);

	if( msg->method_name != NULL )
		free(msg->method_name);

	if( msg->sender_locale != NULL )
		free(msg->sender_locale);

	if( msg->_params != NULL )
		jsonObjectFree(msg->_params);

	free(msg);
}


/**
	@brief Turn a collection of osrfMessages into one big JSON string.
	@param msgs Pointer to an array of osrfMessages.
	@param count Maximum number of messages to serialize.
	@return Pointer to the JSON string.

	Traverse the array, adding each osrfMessage in turn to a JSON_ARRAY.  Stop when you have added the
	maximum number of messages, or when you encounter a NULL pointer in the array.  Then translate the
	JSON_ARRAY into a JSON string.

	The calling code is responsible for freeing the returned string.
*/
char* osrfMessageSerializeBatch( osrfMessage* msgs [], int count ) {
	if( !msgs ) return NULL;

	jsonObject* wrapper = jsonNewObjectType(JSON_ARRAY);

	int i = 0;
	while( (i < count) && msgs[i] ) {
		jsonObjectPush(wrapper, osrfMessageToJSON( msgs[i] ));
		++i;
	}

	char* j = jsonObjectToJSON(wrapper);
	jsonObjectFree(wrapper);

	return j;
}


/**
	@brief Turn a single osrfMessage into a JSON string.
	@param msg Pointer to the osrfMessage to be serialized.
	@return Pointer to the resulting JSON string.

	Translate the osrfMessage into JSON, wrapped in a JSON array.

	This function is equivalent to osrfMessageSerializeBatch() for an array of one pointer.

	The calling code is responsible for freeing the returned string.
*/
char* osrf_message_serialize(const osrfMessage* msg) {

	if( msg == NULL ) return NULL;
	char* j = NULL;

	jsonObject* json = osrfMessageToJSON( msg );

	if(json) {
		jsonObject* wrapper = jsonNewObjectType(JSON_ARRAY);
		jsonObjectPush(wrapper, json);
		j = jsonObjectToJSON(wrapper);
		jsonObjectFree(wrapper);
	}

	return j;
}


/**
	@brief Translate an osrfMessage into a jsonObject.
	@param msg Pointer to the osrfMessage to be translated.
	@return Pointer to a newly created jsonObject.

	The resulting jsonObject is a JSON_HASH with a classname of "osrfMessage", and the following keys:
	- "threadTrace"
	- "locale"
	- "type"
	- "payload" (only for STATUS, REQUEST, and RESULT messages)

	The data for "payload" is also a JSON_HASH, whose structure depends on the message type:

	For a STATUS message, the payload's classname is msg->status_name.  The keys are "status" (carrying
	msg->status_text) and "statusCode" (carrying the status code as a string).

	For a REQUEST message, the payload's classname is "osrfMethod".  The keys are "method" (carrying
	msg->method_name) and "params" (carrying a jsonObject to pass any parameters to the method call).

	For a RESULT message, the payload's classname is "osrfResult".  The keys are "status" (carrying
	msg->status_text), "statusCode" (carrying the status code as a string), and "content" (carrying a jsonObject 
	to return any results from the method call).

	The calling code is responsible for freeing the returned jsonObject.
*/
static jsonObject* osrfMessageToJSON( const osrfMessage* msg ) {

	jsonObject* json = jsonNewObjectType(JSON_HASH);
	jsonObjectSetClass(json, "osrfMessage");
	jsonObject* payload;
	char sc[64];
	osrf_clearbuf(sc, sizeof(sc));

	INT_TO_STRING(msg->thread_trace);
	jsonObjectSetKey(json, "threadTrace", jsonNewObject(INTSTR));

	if (msg->sender_locale != NULL) {
		jsonObjectSetKey(json, "locale", jsonNewObject(msg->sender_locale));
	} else if (current_locale != NULL) {
		jsonObjectSetKey(json, "locale", jsonNewObject(current_locale));
	} else {
		jsonObjectSetKey(json, "locale", jsonNewObject(default_locale));
	}

	switch(msg->m_type) {

		case CONNECT:
			jsonObjectSetKey(json, "type", jsonNewObject("CONNECT"));
			break;

		case DISCONNECT:
			jsonObjectSetKey(json, "type", jsonNewObject("DISCONNECT"));
			break;

		case STATUS:
			jsonObjectSetKey(json, "type", jsonNewObject("STATUS"));
			payload = jsonNewObject(NULL);
			jsonObjectSetClass(payload, msg->status_name);
			jsonObjectSetKey(payload, "status", jsonNewObject(msg->status_text));
			snprintf(sc, sizeof(sc), "%d", msg->status_code);
			jsonObjectSetKey(payload, "statusCode", jsonNewObject(sc));
			jsonObjectSetKey(json, "payload", payload);
			break;

		case REQUEST:
			jsonObjectSetKey(json, "type", jsonNewObject("REQUEST"));
			payload = jsonNewObject(NULL);
			jsonObjectSetClass(payload, "osrfMethod");
			jsonObjectSetKey(payload, "method", jsonNewObject(msg->method_name));
			jsonObjectSetKey( payload, "params", jsonObjectDecodeClass( msg->_params ) );
			jsonObjectSetKey(json, "payload", payload);

			break;

		case RESULT:
			jsonObjectSetKey(json, "type", jsonNewObject("RESULT"));
			payload = jsonNewObject(NULL);
			jsonObjectSetClass(payload,"osrfResult");
			jsonObjectSetKey(payload, "status", jsonNewObject(msg->status_text));
			snprintf(sc, sizeof(sc), "%d", msg->status_code);
			jsonObjectSetKey(payload, "statusCode", jsonNewObject(sc));
			jsonObjectSetKey(payload, "content", jsonObjectDecodeClass( msg->_result_content ));
			jsonObjectSetKey(json, "payload", payload);
			break;
	}

	return json;
}


/**
	@brief Translate a JSON array into an array of osrfMessages.
	@param string The JSON string to be translated.
	@param msgs Pointer to an array of pointers to osrfMessage, to receive the results.
	@param count How many slots are available in the @a msgs array.
	@return The number of osrfMessages created.

	The JSON string is expected to be a JSON array, with each element encoding an osrfMessage.

	If there are too many messages in the JSON array to fit into the pointer array, we
	silently ignore the excess.
*/
int osrf_message_deserialize(const char* string, osrfMessage* msgs[], int count) {

	if(!string || !msgs || count <= 0) return 0;
	int numparsed = 0;

	// Parse the JSON
	jsonObject* json = jsonParseString(string);

	if(!json) {
		osrfLogWarning( OSRF_LOG_MARK,
			"osrf_message_deserialize() unable to parse data: \n%s\n", string);
		return 0;
	}

	// Traverse the JSON_ARRAY, turning each element into an osrfMessage
	int x;
	for( x = 0; x < json->size && x < count; x++ ) {

		const jsonObject* message = jsonObjectGetIndex(json, x);

		if(message && message->type != JSON_NULL &&
			message->classname && !strcmp(message->classname, "osrfMessage")) {

			osrfMessage* new_msg = safe_malloc(sizeof(osrfMessage));
			new_msg->m_type = 0;
			new_msg->thread_trace = 0;
			new_msg->protocol = 0;
			new_msg->status_name = NULL;
			new_msg->status_text = NULL;
			new_msg->status_code = 0;
			new_msg->is_exception = 0;
			new_msg->_result_content = NULL;
			new_msg->result_string = NULL;
			new_msg->method_name = NULL;
			new_msg->_params = NULL;
			new_msg->next = NULL;
			new_msg->sender_locale = NULL;
			new_msg->sender_tz_offset = 0;

			// Get the message type.  If not specified, default to CONNECT.
			const jsonObject* tmp = jsonObjectGetKeyConst(message, "type");

			const char* t;
			if( ( t = jsonObjectGetString(tmp)) ) {

				if(!strcmp(t, "CONNECT"))           new_msg->m_type = CONNECT;
				else if(!strcmp(t, "DISCONNECT"))   new_msg->m_type = DISCONNECT;
				else if(!strcmp(t, "STATUS"))       new_msg->m_type = STATUS;
				else if(!strcmp(t, "REQUEST"))      new_msg->m_type = REQUEST;
				else if(!strcmp(t, "RESULT"))       new_msg->m_type = RESULT;
			}

			// Get the thread trace, defaulting to zero.
			tmp = jsonObjectGetKeyConst(message, "threadTrace");
			if(tmp) {
				const char* tt = jsonObjectGetString(tmp);
				if(tt) {
					new_msg->thread_trace = atoi(tt);
				}
			}

			// Get the sender's locale, or leave it NULL if not specified.
			if (current_locale)
				free( current_locale );

			tmp = jsonObjectGetKeyConst(message, "locale");

			if(tmp && (new_msg->sender_locale = jsonObjectToSimpleString(tmp))) {
				current_locale = strdup( new_msg->sender_locale );
			} else {
				current_locale = NULL;
			}

			// Get the protocol, defaulting to zero.
			tmp = jsonObjectGetKeyConst(message, "protocol");

			if(tmp) {
				const char* proto = jsonObjectGetString(tmp);
				if(proto) {
					new_msg->protocol = atoi(proto);
				}
			}

			tmp = jsonObjectGetKeyConst(message, "payload");
			if(tmp) {
				// Get method name and parameters for a REQUEST
				const jsonObject* tmp0 = jsonObjectGetKeyConst(tmp,"method");
				const char* tmp_str = jsonObjectGetString(tmp0);
				if(tmp_str)
					new_msg->method_name = strdup(tmp_str);

				tmp0 = jsonObjectGetKeyConst(tmp,"params");
				if(tmp0) {
					// Note that we use jsonObjectDecodeClass() instead of
					// jsonObjectClone().  The classnames are already decoded,
					// but jsonObjectDecodeClass removes the decoded classnames.
					new_msg->_params = jsonObjectDecodeClass( tmp0 );
					if(new_msg->_params && new_msg->_params->type == JSON_NULL)
						new_msg->_params->type = JSON_ARRAY;
				}

				// Get status fields for a RESULT or STATUS
				if(tmp->classname)
					new_msg->status_name = strdup(tmp->classname);

				tmp0 = jsonObjectGetKeyConst(tmp,"status");
				tmp_str = jsonObjectGetString(tmp0);
				if(tmp_str)
					new_msg->status_text = strdup(tmp_str);

				tmp0 = jsonObjectGetKeyConst(tmp,"statusCode");
				if(tmp0) {
					tmp_str = jsonObjectGetString(tmp0);
					if(tmp_str)
						new_msg->status_code = atoi(tmp_str);
					if(tmp0->type == JSON_NUMBER)
						new_msg->status_code = (int) jsonObjectGetNumber(tmp0);
				}

				// Get the content for a RESULT
				tmp0 = jsonObjectGetKeyConst(tmp,"content");
				if(tmp0) {
					// Note that we use jsonObjectDecodeClass() instead of
					// jsonObjectClone().  The classnames are already decoded,
					// but jsonObjectDecodeClass removes the decoded classnames.
					new_msg->_result_content = jsonObjectDecodeClass( tmp0 );
				}

			}
			msgs[numparsed++] = new_msg;
		}
	}

	jsonObjectFree(json);
	return numparsed;
}



/**
	@brief Return a pointer to the result content of an osrfMessage.
	@param msg Pointer to the osrfMessage whose result content is to be returned.
	@return Pointer to the result content (or NULL if there is no such content, or if @a msg is NULL).

	The returned pointer points into the innards of the osrfMessage.  The calling code should @em not call
	jsonObjectFree() on it, because the osrfMessage still owns it.
*/
jsonObject* osrfMessageGetResult( osrfMessage* msg ) {
	if(msg) return msg->_result_content;
	return NULL;
}

