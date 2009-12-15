/**
	@file osrf_settings.c
	@brief Facility for retrieving server configuration settings.
*/
#include <opensrf/osrf_settings.h> 

/**
	@brief Stores a copy of server configuration settings as a jsonObject.

	It also stores the host name of the settings server which supplied the configuration
	settings.  In practice nothing uses the stored copy of the host name.
*/
struct osrf_host_config_ {
	/** @brief The host name of the settings server */
	char* hostname;
	/** @brief The configuration settings as a jsonObject */
jsonObject* config;
};

static osrf_host_config* osrf_settings_new_host_config(const char* hostname);

static osrf_host_config* config = NULL;

/**
	@brief Fetch a specified string from an already-loaded configuration.
	@param format A printf-style format string.  Subsequent parameters, if any, will be formatted
		and inserted into the format string.
	@return If the value is found, a pointer to a newly-allocated string containing the value;
		otherwise NULL.

	The format string, after expansion, defines a search path through a configuration previously
	loaded and stored as a jsonObject.

	The configuration must have been already been loaded via a call to osrf_settings_retrieve()
	(probably via a call to osrfSystemBootstrap()).  Otherwise this function will call exit()
	immediately.

	The calling code is responsible for freeing the string.
*/
char* osrf_settings_host_value(const char* format, ...) {
	VA_LIST_TO_STRING(format);

	if( ! config ) {
		const char * msg = "NULL config pointer; looking for config_context ";
		fprintf( stderr, "osrf_settings_host_value: %s\"%s\"\n",
			msg, VA_BUF );
		osrfLogError( OSRF_LOG_MARK, "%s\"%s\"", msg, VA_BUF );
		exit( 99 );
	}

	jsonObject* o = jsonObjectFindPath(config->config, VA_BUF);
	char* val = jsonObjectToSimpleString(o);
	jsonObjectFree(o);
	return val;
}

/**
	@brief Fetch a specified subset of an already-loaded configuration.
	@param format A printf-style format string.  Subsequent parameters, if any, will be formatted
		and inserted into the format string.
	@return If the value is found, a pointer to a newly created jsonObject containing the
		specified subset; otherwise NULL.

	The format string, after expansion, defines a search path through a configuration previously
	loaded and stored as a jsonObject.

	The configuration must have been already been loaded via a call to osrf_settings_retrieve()
	(probably via a call to osrfSystemBootstrap()).  Otherwise this function will call exit()
	immediately.

	The calling code is responsible for freeing the jsonObject.
 */
jsonObject* osrf_settings_host_value_object(const char* format, ...) {
	VA_LIST_TO_STRING(format);

	if( ! config ) {
		const char * msg = "config pointer is NULL; looking for config context ";
		fprintf( stderr, "osrf_settings_host_value_object: %s\"%s\"\n",
			msg, VA_BUF );
		osrfLogError( OSRF_LOG_MARK, "%s\"%s\"", msg, VA_BUF );
		exit( 99 );
	}

	return jsonObjectFindPath(config->config, VA_BUF);
}


/**
	@brief Look up the configuration settings and cache them for future reference.
	@param hostname The host name for the settings server.
	@return Zero if successful, or -1 if not.

	The configuration settings come from a settings server.  This arrangement is intended for
	use by servers, so that all server settings can be stored in a single location.  Typically
	a client process (that is not also a server in its own right) will read its own
	configuration file locally.

	The settings are cached as a jsonObject for future lookups by the functions
	osrf_settings_host_value() and osrf_settings_host_value_object().

	The calling code is responsible for freeing the cached settings by calling
	osrf_settings_free_host_config().
 */
int osrf_settings_retrieve(const char* hostname) {

	if(!config) {

		osrfAppSession* session = osrfAppSessionClientInit("opensrf.settings");
		jsonObject* params = jsonNewObject(NULL);
		jsonObjectPush(params, jsonNewObject(hostname));
		int req_id = osrfAppSessionSendRequest( 
			session, params, "opensrf.settings.host_config.get", 1 );
		osrfMessage* omsg = osrfAppSessionRequestRecv( session, req_id, 60 );
		jsonObjectFree(params);

		if(!omsg) {
			osrfLogError( OSRF_LOG_MARK, "No osrfMessage received from host %s (timeout?)", hostname);
		} else if(!omsg->_result_content) {
			osrfMessageFree(omsg);
			osrfLogError(
				OSRF_LOG_MARK,
			"NULL or non-existent osrfMessage result content received from host %s, "
				"broken message or no settings for host",
				hostname
			);
		} else {
			config = osrf_settings_new_host_config(hostname);
			config->config = jsonObjectClone(omsg->_result_content);
			osrfMessageFree(omsg);
		}

		osrf_app_session_request_finish( session, req_id );
		osrfAppSessionFree( session );

		if(!config) {
			osrfLogError( OSRF_LOG_MARK, "Unable to load config for host %s", hostname);
			return -1;
		}
	}

	return 0;
}

/**
	@brief Allocate and initialize an osrf_host_config for a given host name.
	@param hostname Pointer to a host name.
	@return Pointer to a newly created osrf_host_config.
*/
static osrf_host_config* osrf_settings_new_host_config(const char* hostname) {
	if(!hostname) return NULL;
	osrf_host_config* c = safe_malloc(sizeof(osrf_host_config));
	c->hostname = strdup(hostname);
	c->config = NULL;
	return c;
}

/**
	@brief Deallocate an osrf_host_config and its contents.
	@param c A pointer to the osrf_host_config to be deallocated.
*/
void osrf_settings_free_host_config(osrf_host_config* c) {
	if( !c ) {
		c = config;
		config = NULL;
	}
	if( c ) {
		free(c->hostname);
		jsonObjectFree(c->config);
		free(c);
	}
}
