/*
Copyright (C) 2005  Georgia Public Library Service 
Bill Erickson <highfalutin@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <opensrf/osrf_cache.h>

static struct memcache* _osrfCache = NULL;
static time_t _osrfCacheMaxSeconds = -1;

int osrfCacheInit( const char* serverStrings[], int size, time_t maxCacheSeconds ) {
	if( !(serverStrings && size > 0) ) return -1;
    osrfCacheCleanup(); /* in case we've already been init-ed */

	int i;
	_osrfCache = mc_new();
	_osrfCacheMaxSeconds = maxCacheSeconds;

	for( i = 0; i < size && serverStrings[i]; i++ ) 
		mc_server_add4( _osrfCache, serverStrings[i] );

	return 0;
}

int osrfCachePutObject( char* key, const jsonObject* obj, time_t seconds ) {
	if( !(key && obj) ) return -1;
	char* s = jsonObjectToJSON( obj );
	osrfLogInternal( OSRF_LOG_MARK, "osrfCachePut(): Putting object (key=%s): %s", key, s);
    osrfCachePutString(key, s, seconds);
	free(s);
	return 0;
}

int osrfCachePutString( char* key, const char* value, time_t seconds ) {
	if( !(key && value) ) return -1;
    seconds = (seconds <= 0 || seconds > _osrfCacheMaxSeconds) ? _osrfCacheMaxSeconds : seconds;
	osrfLogInternal( OSRF_LOG_MARK, "osrfCachePutString(): Putting string (key=%s): %s", key, value);
	mc_set(_osrfCache, key, strlen(key), value, strlen(value), seconds, 0);
	return 0;
}

jsonObject* osrfCacheGetObject( const char* key, ... ) {
	jsonObject* obj = NULL;
	if( key ) {
		VA_LIST_TO_STRING(key);
		const char* data = (const char*) mc_aget( _osrfCache, VA_BUF, strlen(VA_BUF) );
		if( data ) {
			osrfLogInternal( OSRF_LOG_MARK, "osrfCacheGetObject(): Returning object (key=%s): %s", VA_BUF, data);
			obj = jsonParse( data );
			return obj;
		}
		osrfLogDebug(OSRF_LOG_MARK, "No cache data exists with key %s", VA_BUF);
	}
	return NULL;
}

char* osrfCacheGetString( const char* key, ... ) {
	if( key ) {
		VA_LIST_TO_STRING(key);
		char* data = (char*) mc_aget(_osrfCache, VA_BUF, strlen(VA_BUF) );
		osrfLogInternal( OSRF_LOG_MARK, "osrfCacheGetString(): Returning object (key=%s): %s", VA_BUF, data);
		if(!data) osrfLogDebug(OSRF_LOG_MARK, "No cache data exists with key %s", VA_BUF);
		return data;
	}
	return NULL;
}


int osrfCacheRemove( const char* key, ... ) {
	if( key ) {
		VA_LIST_TO_STRING(key);
		return mc_delete(_osrfCache, VA_BUF, strlen(VA_BUF), 0 );
	}
	return -1;
}


int osrfCacheSetExpire( time_t seconds, const char* key, ... ) {
	if( key ) {
		VA_LIST_TO_STRING(key);
		jsonObject* o = osrfCacheGetObject( VA_BUF );
		//osrfCacheRemove(VA_BUF);
		int rc = osrfCachePutObject( VA_BUF, o, seconds );
		jsonObjectFree(o);
		return rc;
	}
	return -1;
}

void osrfCacheCleanup() {
    if(_osrfCache)
        mc_free(_osrfCache);
}


