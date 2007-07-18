/*
Copyright (C) 2006  Georgia Public Library Service 
Bill Erickson <billserickson@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <opensrf/osrf_json.h>
#include <opensrf/osrf_json_utils.h>

jsonObject* jsonNewObject(const char* data) {

	jsonObject* o;
	OSRF_MALLOC(o, sizeof(jsonObject));
	o->type = JSON_NULL;

	if(data) {
		o->type = JSON_STRING;
		o->value.s = strdup(data);
	}

	return o;
}

jsonObject* jsonNewObjectFmt(const char* data, ...) {

	jsonObject* o;
	OSRF_MALLOC(o, sizeof(jsonObject));
	o->type = JSON_NULL;

	if(data) {
		VA_LIST_TO_STRING(data);
		o->type = JSON_STRING;
		o->value.s = strdup(VA_BUF);
	}

	return o;
}

jsonObject* jsonNewNumberObject( double num ) {
	jsonObject* o = jsonNewObject(NULL);
	o->type = JSON_NUMBER;
	o->value.n = num;
	return o;
}

jsonObject* jsonNewBoolObject(int val) {
    jsonObject* o = jsonNewObject(NULL);
    o->type = JSON_BOOL;
    jsonSetBool(o, val);
    return o;
}

jsonObject* jsonNewObjectType(int type) {
	jsonObject* o = jsonNewObject(NULL);
	o->type = type;
	return o;
}

void jsonObjectFree( jsonObject* o ) {

	if(!o || o->parent) return;
	free(o->classname);

	switch(o->type) {
		case JSON_HASH		: osrfHashFree(o->value.h); break;
		case JSON_ARRAY	: osrfListFree(o->value.l); break;
		case JSON_STRING	: free(o->value.s); break;
	}
	free(o);
}

static void _jsonFreeHashItem(char* key, void* item){
	if(!item) return;
	jsonObject* o = (jsonObject*) item;
	o->parent = NULL; /* detach the item */
	jsonObjectFree(o);
}
static void _jsonFreeListItem(void* item){
	if(!item) return;
	jsonObject* o = (jsonObject*) item;
	o->parent = NULL; /* detach the item */
	jsonObjectFree(o);
}

void jsonSetBool(jsonObject* bl, int val) {
    if(!bl) return;
    JSON_INIT_CLEAR(bl, JSON_BOOL);
    bl->value.b = val;
}

unsigned long jsonObjectPush(jsonObject* o, jsonObject* newo) {
    if(!o) return -1;
    if(!newo) newo = jsonNewObject(NULL);
	JSON_INIT_CLEAR(o, JSON_ARRAY);
	newo->parent = o;
	osrfListPush( o->value.l, newo );
	o->size = o->value.l->size;
	return o->size;
}

unsigned long jsonObjectSetIndex(jsonObject* dest, unsigned long index, jsonObject* newObj) {
    if(!dest) return -1;
    if(!newObj) newObj = jsonNewObject(NULL);
	JSON_INIT_CLEAR(dest, JSON_ARRAY);
	newObj->parent = dest;
	osrfListSet( dest->value.l, newObj, index );
	dest->size = dest->value.l->size;
	return dest->value.l->size;
}

unsigned long jsonObjectSetKey( jsonObject* o, const char* key, jsonObject* newo) {
    if(!o) return -1;
    if(!newo) newo = jsonNewObject(NULL);
	JSON_INIT_CLEAR(o, JSON_HASH);
	newo->parent = o;
	osrfHashSet( o->value.h, newo, key );
	o->size = o->value.h->size;
	return o->size;
}

jsonObject* jsonObjectGetKey( const jsonObject* obj, const char* key ) {
	if(!(obj && obj->type == JSON_HASH && obj->value.h && key)) return NULL;
	return osrfHashGet( obj->value.h, key);
}

char* jsonObjectToJSON( const jsonObject* obj ) {
	jsonObject* obj2 = jsonObjectEncodeClass( (jsonObject*) obj);
	char* json = jsonObjectToJSONRaw(obj2);
	jsonObjectFree(obj2);
	return json;
}

char* jsonObjectToJSONRaw( const jsonObject* obj ) {
	if(!obj) return NULL;
	growing_buffer* buf = buffer_init(32);
	int i;
    char* json;

	switch(obj->type) {

		case JSON_BOOL :
			if(obj->value.b) OSRF_BUFFER_ADD(buf, "true"); 
			else OSRF_BUFFER_ADD(buf, "false"); 
			break;

		case JSON_NUMBER: {
			double x = obj->value.n;
			if( x == (int) x ) {
				INT_TO_STRING((int)x);	
				OSRF_BUFFER_ADD(buf, INTSTR);

			} else {
				DOUBLE_TO_STRING(x);
				OSRF_BUFFER_ADD(buf, DOUBLESTR);
			}
			break;
		}

		case JSON_NULL:
			OSRF_BUFFER_ADD(buf, "null");
			break;

		case JSON_STRING:
			OSRF_BUFFER_ADD_CHAR(buf, '"');
			char* data = obj->value.s;
			int len = strlen(data);
			
			char* output = uescape(data, len, 1);
			OSRF_BUFFER_ADD(buf, output);
			free(output);
			OSRF_BUFFER_ADD_CHAR(buf, '"');
			break;
			
		case JSON_ARRAY: {
			OSRF_BUFFER_ADD_CHAR(buf, '[');
			if( obj->value.l ) {
				for( i = 0; i != obj->value.l->size; i++ ) {
					json = jsonObjectToJSONRaw(OSRF_LIST_GET_INDEX(obj->value.l, i));
					if(i > 0) OSRF_BUFFER_ADD(buf, ",");
					OSRF_BUFFER_ADD(buf, json);
					free(json);
				}
			} 
			OSRF_BUFFER_ADD_CHAR(buf, ']');
			break;
		}

		case JSON_HASH: {

			OSRF_BUFFER_ADD_CHAR(buf, '{');
			osrfHashIterator* itr = osrfNewHashIterator(obj->value.h);
			jsonObject* item;
			int i = 0;

			while( (item = osrfHashIteratorNext(itr)) ) {
				if(i++ > 0) OSRF_BUFFER_ADD(buf, ",");
				buffer_fadd(buf, "\"%s\":", itr->current);
				char* json = jsonObjectToJSONRaw(item);
				OSRF_BUFFER_ADD(buf, json);
				free(json);
			}

			osrfHashIteratorFree(itr);
			OSRF_BUFFER_ADD_CHAR(buf, '}');
			break;
		}
	}

    return buffer_release(buf);
}


jsonIterator* jsonNewIterator(const jsonObject* obj) {
	if(!obj) return NULL;
	jsonIterator* itr;
	OSRF_MALLOC(itr, sizeof(jsonIterator));

	itr->obj		= (jsonObject*) obj;
	itr->index	= 0;
	itr->key		= NULL;

	if( obj->type == JSON_HASH )
		itr->hashItr = osrfNewHashIterator(obj->value.h);
	
	return itr;
}

void jsonIteratorFree(jsonIterator* itr) {
	if(!itr) return;
	free(itr->key);
	osrfHashIteratorFree(itr->hashItr);
	free(itr);
}

jsonObject* jsonIteratorNext(jsonIterator* itr) {
	if(!(itr && itr->obj)) return NULL;
	if( itr->obj->type == JSON_HASH ) {
		if(!itr->hashItr) return NULL;
		jsonObject* item = osrfHashIteratorNext(itr->hashItr);
		free(itr->key);
		itr->key = strdup(itr->hashItr->current);
		return item;
	} else {
		return jsonObjectGetIndex( itr->obj, itr->index++ );
	}
}

int jsonIteratorHasNext(const jsonIterator* itr) {
	if(!(itr && itr->obj)) return 0;
	if( itr->obj->type == JSON_HASH )
		return osrfHashIteratorHasNext( itr->hashItr );
	return (itr->index < itr->obj->size) ? 1 : 0;
}

jsonObject* jsonObjectGetIndex( const jsonObject* obj, unsigned long index ) {
	if(!obj) return NULL;
	return (obj->type == JSON_ARRAY) ? 
        (OSRF_LIST_GET_INDEX(obj->value.l, index)) : NULL;
}



unsigned long jsonObjectRemoveIndex(jsonObject* dest, unsigned long index) {
	if( dest && dest->type == JSON_ARRAY ) {
		osrfListRemove(dest->value.l, index);
		return dest->value.l->size;
	}
	return -1;
}


unsigned long jsonObjectRemoveKey( jsonObject* dest, const char* key) {
	if( dest && key && dest->type == JSON_HASH ) {
		osrfHashRemove(dest->value.h, key);
		return 1;
	}
	return -1;
}

char* jsonObjectGetString(const jsonObject* obj) {
	return (obj && obj->type == JSON_STRING) ? obj->value.s : NULL;
}

double jsonObjectGetNumber( const jsonObject* obj ) {
	return (obj && obj->type == JSON_NUMBER) ? obj->value.n : 0;
}

void jsonObjectSetString(jsonObject* dest, const char* string) {
	if(!(dest && string)) return;
	JSON_INIT_CLEAR(dest, JSON_STRING);
	free(dest->value.s);
	dest->value.s = strdup(string);
}

void jsonObjectSetNumber(jsonObject* dest, double num) {
	if(!dest) return;
	JSON_INIT_CLEAR(dest, JSON_NUMBER);
	dest->value.n = num;
}

void jsonObjectSetClass(jsonObject* dest, const char* classname ) {
	if(!(dest && classname)) return;
	free(dest->classname);
	dest->classname = strdup(classname);
}
const char* jsonObjectGetClass(const jsonObject* dest) {
    if(!dest) return NULL;
    return dest->classname;
}

jsonObject* jsonObjectClone( const jsonObject* o ) {
    if(!o) return jsonNewObject(NULL);

    int i;
    jsonObject* arr; 
    jsonObject* hash; 
    jsonIterator* itr;
    jsonObject* tmp;
    jsonObject* result = NULL;

    switch(o->type) {
        case JSON_NULL:
            result = jsonNewObject(NULL);
            break;
        case JSON_STRING:
            result = jsonNewObject(jsonObjectGetString(o));
            break;
        case JSON_NUMBER:
            result = jsonNewNumberObject(jsonObjectGetNumber(o));
            break;
        case JSON_BOOL:
            result = jsonNewBoolObject(jsonBoolIsTrue((jsonObject*) o));
            break;
        case JSON_ARRAY:
            arr = jsonNewObject(NULL);
            arr->type = JSON_ARRAY;
            for(i=0; i < o->size; i++) 
                jsonObjectPush(arr, jsonObjectClone(jsonObjectGetIndex(o, i)));
            result = arr;
            break;
        case JSON_HASH:
            hash = jsonNewObject(NULL);
            hash->type = JSON_HASH;
            itr = jsonNewIterator(o);
            while( (tmp = jsonIteratorNext(itr)) )
                jsonObjectSetKey(hash, itr->key, jsonObjectClone(tmp));
            jsonIteratorFree(itr);
            result = hash;
            break;
    }

    jsonObjectSetClass(result, jsonObjectGetClass(o));
    return result;
}

int jsonBoolIsTrue( jsonObject* boolObj ) {
    if( boolObj && boolObj->type == JSON_BOOL && boolObj->value.b )
        return 1;
    return 0;
}


char* jsonObjectToSimpleString( const jsonObject* o ) {
	if(!o) return NULL;

	char* value = NULL;

	switch( o->type ) {

		case JSON_NUMBER: {

			if( o->value.n == (int) o->value.n ) {
				INT_TO_STRING((int) o->value.n);	
				value = strdup(INTSTR);

			} else {
				DOUBLE_TO_STRING(o->value.n);
				value = strdup(DOUBLESTR);
			}

			break;
		}

		case JSON_STRING:
			value = strdup(o->value.s);
	}

	return value;
}


