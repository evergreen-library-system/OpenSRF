#include "apachetools.h"

osrfStringArray* apacheParseParms(request_rec* r) {

	if( r == NULL ) return NULL;

	char* arg = NULL;
	apr_pool_t *p = r->pool;	/* memory pool */
	growing_buffer* buffer = buffer_init(1025);

	/* gather the post args and append them to the url query string */
	if( !strcmp(r->method,"POST") ) {

		ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK);
		
		osrfLogDebug(OSRF_LOG_MARK, "gateway reading post data..");

		if(ap_should_client_block(r)) {


			/* Start with url query string, if any */
			
			if(r->args && r->args[0])
				buffer_add(buffer, r->args);

			char body[1025];

			osrfLogDebug(OSRF_LOG_MARK, "gateway client has post data, reading...");

			/* Append POST data */
			
			long bread;
			while( (bread = ap_get_client_block(r, body, sizeof(body) - 1)) ) {

				if(bread < 0) {
					osrfLogInfo(OSRF_LOG_MARK, 
						"ap_get_client_block(): returned error, exiting POST reader");
					break;
				}

				body[bread] = '\0';
				buffer_add( buffer, body );

				osrfLogDebug(OSRF_LOG_MARK, 
					"gateway read %ld bytes: %d bytes of data so far", bread, buffer->n_used);

				if(buffer->n_used > APACHE_TOOLS_MAX_POST_SIZE) {
					osrfLogError(OSRF_LOG_MARK, "gateway received POST larger "
						"than %d bytes. dropping request", APACHE_TOOLS_MAX_POST_SIZE);
					buffer_free(buffer);
					return NULL;
				}
			}

			osrfLogDebug(OSRF_LOG_MARK, "gateway done reading post data");
		}

	} else { /* GET */

        if(r->args && r->args[0])
            buffer_add(buffer, r->args);
    }


    if(buffer->n_used > 0)
        arg = apr_pstrdup(p, buffer->buf);
    else
        arg = NULL; 
    buffer_free(buffer);

	if( !arg || !arg[0] ) { /* we received no request */
		return NULL;
	}

	osrfLogDebug(OSRF_LOG_MARK, "parsing URL params from post/get request data: %s", arg);
	
	osrfStringArray* sarray		= osrfNewStringArray(12); /* method parameters */
	int sanity = 0;
	char* key					= NULL;	/* query item name */
	char* val					= NULL;	/* query item value */

	/* Parse the post/get request data into a series of name/value pairs.   */
	/* Load each name into an even-numbered slot of an osrfStringArray, and */
	/* the corresponding value into the following odd-numbered slot.        */

	while( arg && (val = ap_getword(p, (const char**) &arg, '&'))) {

		key = ap_getword(r->pool, (const char**) &val, '=');
		if(!key || !key[0])
			break;

		ap_unescape_url(key);
		ap_unescape_url(val);

		osrfLogDebug(OSRF_LOG_MARK, "parsed URL params %s=%s", key, val);

		osrfStringArrayAdd(sarray, key);
		osrfStringArrayAdd(sarray, val);

		if( sanity++ > 1000 ) {
			osrfLogError(OSRF_LOG_MARK, 
				"Parsing URL params failed sanity check: 1000 iterations");
			osrfStringArrayFree(sarray);
			return NULL;
		}

	}

	osrfLogDebug(OSRF_LOG_MARK,
		"Apache tools parsed %d params key/values", sarray->size / 2 );

	return sarray;
}



osrfStringArray* apacheGetParamKeys(osrfStringArray* params) {
	if(params == NULL) return NULL;	
	osrfStringArray* sarray = osrfNewStringArray(12);
	int i;
	osrfLogDebug(OSRF_LOG_MARK, "Fetching URL param keys");
	for( i = 0; i < params->size; i++ ) 
		osrfStringArrayAdd(sarray, osrfStringArrayGetString(params, i++));
	return sarray;
}

osrfStringArray* apacheGetParamValues(osrfStringArray* params, char* key) {

	if(params == NULL || key == NULL) return NULL;	
	osrfStringArray* sarray	= osrfNewStringArray(12);

	osrfLogDebug(OSRF_LOG_MARK, "Fetching URL values for key %s", key);
	int i;
	for( i = 0; i < params->size; i++ ) {
		const char* nkey = osrfStringArrayGetString(params, i++);
		if(nkey && !strcmp(nkey, key)) 
			osrfStringArrayAdd(sarray, osrfStringArrayGetString(params, i));
	}
	return sarray;
}


char* apacheGetFirstParamValue(osrfStringArray* params, char* key) {
	if(params == NULL || key == NULL) return NULL;	

	int i;
	osrfLogDebug(OSRF_LOG_MARK, "Fetching first URL value for key %s", key);
	for( i = 0; i < params->size; i++ ) {
		const char* nkey = osrfStringArrayGetString(params, i++);
		if(nkey && !strcmp(nkey, key)) 
			return strdup(osrfStringArrayGetString(params, i));
	}

	return NULL;
}


int apacheDebug( char* msg, ... ) {
	VA_LIST_TO_STRING(msg);
	fprintf(stderr, "%s\n", VA_BUF);
	fflush(stderr);
	return 0;
}


int apacheError( char* msg, ... ) {
	VA_LIST_TO_STRING(msg);
	fprintf(stderr, "%s\n", VA_BUF);
	fflush(stderr);
	return HTTP_INTERNAL_SERVER_ERROR; 
}


/* taken more or less directly from O'Reillly - Writing Apache Modules in Perl and C */
/* needs updating...
 
apr_table_t* apacheParseCookies(request_rec *r) {

   const char *data = apr_table_get(r->headers_in, "Cookie");
	osrfLogDebug(OSRF_LOG_MARK, "Loaded cookies: %s", data);

   apr_table_t* cookies;
   const char *pair;
   if(!data) return NULL;

   cookies = apr_make_table(r->pool, 4);
   while(*data && (pair = ap_getword(r->pool, &data, ';'))) {
       const char *name, *value;
       if(*data == ' ') ++data;
       name = ap_getword(r->pool, &pair, '=');
       while(*pair && (value = ap_getword(r->pool, &pair, '&'))) {
           ap_unescape_url((char *)value);
           apr_table_add(cookies, name, value);
       }
   }

    return cookies;
}

*/ 


