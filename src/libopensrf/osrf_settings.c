#include <opensrf/osrf_settings.h> 

osrf_host_config* config = NULL;

char* osrf_settings_host_value(const char* format, ...) {
	VA_LIST_TO_STRING(format);

	if( ! config ) {
		const char * msg = "NULL config pointer";
		fprintf( stderr, "osrf_settings_host_value: %s\n", msg );
		osrfLogError( OSRF_LOG_MARK, msg );
		exit( 99 );
	}

	jsonObject* o = jsonObjectFindPath(config->config, VA_BUF);
	char* val = jsonObjectToSimpleString(o);
	jsonObjectFree(o);
	return val;
}

jsonObject* osrf_settings_host_value_object(const char* format, ...) {
	VA_LIST_TO_STRING(format);

	if( ! config ) {
		const char * msg = "config pointer is NULL";
		fprintf( stderr, "osrf_settings_host_value_object: %s\n", msg );
		osrfLogError( OSRF_LOG_MARK, msg );
		exit( 99 );
	}

	return jsonObjectFindPath(config->config, VA_BUF);
}


int osrf_settings_retrieve(const char* hostname) {

	if(!config) {

		osrf_app_session* session = osrf_app_client_session_init("opensrf.settings");
		jsonObject* params = jsonNewObject(NULL);
		jsonObjectPush(params, jsonNewObject(hostname));
		int req_id = osrf_app_session_make_req( 
			session, params, "opensrf.settings.host_config.get", 1, NULL );
		osrf_message* omsg = osrf_app_session_request_recv( session, req_id, 60 );
		jsonObjectFree(params);

		if(!omsg) {
			osrfLogError( OSRF_LOG_MARK, "No osrf_message received from host %s (timeout?)", hostname);
		} else if(!omsg->_result_content) {
			osrf_message_free(omsg);
			osrfLogError(
				OSRF_LOG_MARK,
				"NULL or non-existant osrf_message result content received from host %s, "
				"broken message or no settings for host",
				hostname
			);
		} else {
			config = osrf_settings_new_host_config(hostname);
			config->config = jsonObjectClone(omsg->_result_content);
			osrf_message_free(omsg);
		}

		osrf_app_session_request_finish( session, req_id );
		osrf_app_session_destroy( session );

		if(!config) {
			osrfLogError( OSRF_LOG_MARK, "Unable to load config for host %s", hostname);
			return -1;
		}
	}

	return 0;
}

osrf_host_config* osrf_settings_new_host_config(const char* hostname) {
	if(!hostname) return NULL;
	osrf_host_config* c = safe_malloc(sizeof(osrf_host_config));
	c->hostname = strdup(hostname);
	c->config = NULL;
	return c;
}

void osrf_settings_free_host_config(osrf_host_config* c) {
	if(!c) c = config;
	if(!c) return;
	free(c->hostname);
	jsonObjectFree(c->config);	
	free(c);
}
