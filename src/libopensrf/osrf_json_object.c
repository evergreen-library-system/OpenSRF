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

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>
#include <opensrf/osrf_json_utils.h>

/* cleans up an object if it is morphing another object, also
 * verifies that the appropriate storage container exists where appropriate */
#define JSON_INIT_CLEAR(_obj_, newtype)		\
	if( _obj_->type == JSON_HASH && newtype != JSON_HASH ) {			\
		osrfHashFree(_obj_->value.h);			\
		_obj_->value.h = NULL; 					\
} else if( _obj_->type == JSON_ARRAY && newtype != JSON_ARRAY ) {	\
		osrfListFree(_obj_->value.l);			\
		_obj_->value.l = NULL;					\
} else if( _obj_->type == JSON_STRING || _obj_->type == JSON_NUMBER ) { \
		free(_obj_->value.s);						\
		_obj_->value.s = NULL;					\
} \
	_obj_->type = newtype;\
	if( newtype == JSON_HASH && _obj_->value.h == NULL ) {	\
		_obj_->value.h = osrfNewHash();		\
		osrfHashSetCallback( _obj_->value.h, _jsonFreeHashItem ); \
} else if( newtype == JSON_ARRAY && _obj_->value.l == NULL ) {	\
		_obj_->value.l = osrfNewList();		\
		_obj_->value.l->freeItem = _jsonFreeListItem;\
}

static int unusedObjCapture = 0;
static int unusedObjRelease = 0;
static int mallocObjCreate = 0;
static int currentListLen = 0;

union unusedObjUnion{

	union unusedObjUnion* next;
	jsonObject obj;
};
typedef union unusedObjUnion unusedObj;

// We maintain a free list of jsonObjects that are available
// for use, in order to reduce the churning through
// malloc() and free().

static unusedObj* freeObjList = NULL;

static void add_json_to_buffer( const jsonObject* obj,
	growing_buffer * buf, int do_classname, int second_pass );

/**
 * Return all unused jsonObjects to the heap
 * @return Nothing
 */
void jsonObjectFreeUnused( void ) {

	unusedObj* temp;
	while( freeObjList ) {
		temp = freeObjList->next;
		free( freeObjList );
		freeObjList = temp;
	}
}

jsonObject* jsonNewObject(const char* data) {

	jsonObject* o;

	if( freeObjList ) {
		o = (jsonObject*) freeObjList;
		freeObjList = freeObjList->next;
        unusedObjRelease++;
        currentListLen--;
	} else {
		OSRF_MALLOC( o, sizeof(jsonObject) );
        mallocObjCreate++;
    }

	o->size = 0;
	o->classname = NULL;
	o->parent = NULL;

	if(data) {
		o->type = JSON_STRING;
		o->value.s = strdup(data);
	} else {
		o->type = JSON_NULL;
		o->value.s = NULL;
	}

	return o;
}

jsonObject* jsonNewObjectFmt(const char* data, ...) {

	jsonObject* o;

	if( freeObjList ) {
		o = (jsonObject*) freeObjList;
		freeObjList = freeObjList->next;
        unusedObjRelease++;
        currentListLen--;
	} else {
		OSRF_MALLOC( o, sizeof(jsonObject) );
        mallocObjCreate++;
    }

	o->size = 0;
	o->classname = NULL;
	o->parent = NULL;

	if(data) {
		VA_LIST_TO_STRING(data);
		o->type = JSON_STRING;
		o->value.s = strdup(VA_BUF);
	}
	else {
		o->type = JSON_NULL;
		o->value.s = NULL;
	}
	
	return o;
}

jsonObject* jsonNewNumberObject( double num ) {
	jsonObject* o = jsonNewObject(NULL);
	o->type = JSON_NUMBER;
	o->value.s = doubleToString( num );
	return o;
}

/**
 * Creates a new number object from a numeric string
 */
jsonObject* jsonNewNumberStringObject( const char* numstr ) {
	if( !numstr )
		numstr = "0";
	else if( !jsonIsNumeric( numstr ) )
		return NULL;

	jsonObject* o = jsonNewObject(NULL);
	o->type = JSON_NUMBER;
	o->value.s = strdup( numstr );
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
		case JSON_NUMBER	: free(o->value.s); break;
	}

	// Stick the old jsonObject onto a free list
	// for potential reuse
	
	unusedObj* unused = (unusedObj*) o;
	unused->next = freeObjList;
	freeObjList = unused;

    unusedObjCapture++;
    currentListLen++;
    if (unusedObjCapture > 1 && !(unusedObjCapture % 1000))
        osrfLogDebug( OSRF_LOG_MARK, "Objects malloc()'d: %d, "
			"Reusable objects captured: %d, Objects reused: %d, "
			"Current List Length: %d",
			mallocObjCreate, unusedObjCapture, unusedObjRelease, currentListLen );
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
	o->size = osrfHashGetCount(o->value.h);
	return o->size;
}

jsonObject* jsonObjectGetKey( jsonObject* obj, const char* key ) {
	if(!(obj && obj->type == JSON_HASH && obj->value.h && key)) return NULL;
	return osrfHashGet( obj->value.h, key);
}

const jsonObject* jsonObjectGetKeyConst( const jsonObject* obj, const char* key ) {
	if(!(obj && obj->type == JSON_HASH && obj->value.h && key)) return NULL;
	return osrfHashGet( obj->value.h, key);
}

/**
 * Recursively traverse a jsonObject, formatting it into a JSON string.
 *
 * The last two parameters are booleans.
 *
 * If do_classname is true, examine each node for a classname, and if you
 * find one, pretend that the node is under an extra layer of JSON_HASH, with
 * JSON_CLASS_KEY and JSON_DATA_KEY as keys.
 *
 * second_pass should always be false except for some recursive calls.  It
 * is used when expanding classnames, to distinguish between the first and
 * second passes through a given node.
 *
 * @return Nothing
 */
static void add_json_to_buffer( const jsonObject* obj,
	growing_buffer * buf, int do_classname, int second_pass ) {

    if(NULL == obj) {
        OSRF_BUFFER_ADD(buf, "null");
        return;
    }

	if( obj->classname && do_classname )
	{
		if( second_pass )
			second_pass = 0;
		else
		{
			// Pretend we see an extra layer of JSON_HASH
			
			OSRF_BUFFER_ADD( buf, "{\"" );
			OSRF_BUFFER_ADD( buf, JSON_CLASS_KEY );
			OSRF_BUFFER_ADD( buf, "\":\"" );
			OSRF_BUFFER_ADD( buf, obj->classname );
			OSRF_BUFFER_ADD( buf, "\",\"" );
			OSRF_BUFFER_ADD( buf, JSON_DATA_KEY );
			OSRF_BUFFER_ADD( buf, "\":" );
			add_json_to_buffer( obj, buf, 1, 1 );
			buffer_add_char( buf, '}' );
			return;
		}
	}

	switch(obj->type) {

		case JSON_BOOL :
			if(obj->value.b) OSRF_BUFFER_ADD(buf, "true"); 
			else OSRF_BUFFER_ADD(buf, "false"); 
			break;

        case JSON_NUMBER: {
            if(obj->value.s) OSRF_BUFFER_ADD( buf, obj->value.s );
            else OSRF_BUFFER_ADD_CHAR( buf, '0' );
            break;
        }

		case JSON_NULL:
			OSRF_BUFFER_ADD(buf, "null");
			break;

		case JSON_STRING:
			OSRF_BUFFER_ADD_CHAR(buf, '"');
			buffer_append_uescape(buf, obj->value.s);
			OSRF_BUFFER_ADD_CHAR(buf, '"');
			break;
			
		case JSON_ARRAY: {
			OSRF_BUFFER_ADD_CHAR(buf, '[');
			if( obj->value.l ) {
				int i;
				for( i = 0; i != obj->value.l->size; i++ ) {
					if(i > 0) OSRF_BUFFER_ADD(buf, ",");
					add_json_to_buffer(
						OSRF_LIST_GET_INDEX(obj->value.l, i), buf, do_classname, second_pass );
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
				if(i++ > 0) OSRF_BUFFER_ADD_CHAR(buf, ',');
				OSRF_BUFFER_ADD_CHAR(buf, '"');
				OSRF_BUFFER_ADD(buf, osrfHashIteratorKey(itr));
				OSRF_BUFFER_ADD(buf, "\":");
				add_json_to_buffer( item, buf, do_classname, second_pass );
			}

			osrfHashIteratorFree(itr);
			OSRF_BUFFER_ADD_CHAR(buf, '}');
			break;
		}
	}
}

char* jsonObjectToJSONRaw( const jsonObject* obj ) {
	if(!obj) return NULL;
	growing_buffer* buf = buffer_init(32);
	add_json_to_buffer( obj, buf, 0, 0 );
	return buffer_release( buf );
}

char* jsonObjectToJSON( const jsonObject* obj ) {
	if(!obj) return NULL;
	growing_buffer* buf = buffer_init(32);
	add_json_to_buffer( obj, buf, 1, 0 );
	return buffer_release( buf );
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
        if(!item) return NULL;
		free(itr->key);
		itr->key = strdup( osrfHashIteratorKey(itr->hashItr) );
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

/**
 Allocate a buffer and format a specified numeric value into it.
 Caller is responsible for freeing the buffer.
**/
char* doubleToString( double num ) {
	
	char buf[ 64 ];
	size_t len = snprintf(buf, sizeof( buf ), "%.30g", num) + 1;
	if( len < sizeof( buf ) )
		return strdup( buf );
	else
	{
		// Need a bigger buffer (should never be necessary)
		
		char* bigger_buff = safe_malloc( len + 1 );
		(void) snprintf(bigger_buff, len + 1, "%.30g", num);
		return bigger_buff;
	}
}

char* jsonObjectGetString(const jsonObject* obj) {
	if(obj)
	{
		if( obj->type == JSON_STRING )
			return obj->value.s;
		else if( obj->type == JSON_NUMBER )
			return obj->value.s ? obj->value.s : "0";
		else
			return NULL;
	}
	else
		return NULL;
}

double jsonObjectGetNumber( const jsonObject* obj ) {
	return (obj && obj->type == JSON_NUMBER && obj->value.s)
			? strtod( obj->value.s, NULL ) : 0;
}

void jsonObjectSetString(jsonObject* dest, const char* string) {
	if(!(dest && string)) return;
	JSON_INIT_CLEAR(dest, JSON_STRING);
	dest->value.s = strdup(string);
}

/**
 Turn a jsonObject into a JSON_NUMBER (if it isn't already one) and store
 a specified numeric string in it.  If the string is not numeric,
 store the equivalent of zero, and return an error status.
**/
int jsonObjectSetNumberString(jsonObject* dest, const char* string) {
	if(!(dest && string)) return -1;
	JSON_INIT_CLEAR(dest, JSON_NUMBER);

	if( jsonIsNumeric( string ) ) {
		dest->value.s = strdup(string);
		return 0;
	}
	else {
		dest->value.s = NULL;  // equivalent to zero
		return -1;
	}
}

void jsonObjectSetNumber(jsonObject* dest, double num) {
	if(!dest) return;
	JSON_INIT_CLEAR(dest, JSON_NUMBER);
	dest->value.s = doubleToString( num );
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
			result = jsonNewObject( o->value.s );
			result->type = JSON_NUMBER;
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

int jsonBoolIsTrue( const jsonObject* boolObj ) {
    if( boolObj && boolObj->type == JSON_BOOL && boolObj->value.b )
        return 1;
    return 0;
}


char* jsonObjectToSimpleString( const jsonObject* o ) {
	if(!o) return NULL;

	char* value = NULL;

	switch( o->type ) {

		case JSON_NUMBER:
			value = strdup( o->value.s ? o->value.s : "0" );
			break;

		case JSON_STRING:
			value = strdup(o->value.s);
	}

	return value;
}

/**
 Return 1 if the string is numeric, otherwise return 0.
 This validation follows the rules defined by the grammar at:
 http://www.json.org/
 **/
int jsonIsNumeric( const char* s ) {

	if( !s || !*s ) return 0;

	const char* p = s;

	// skip leading minus sign, if present (leading plus sign not allowed)

	if( '-' == *p )
		++p;

	// There must be at least one digit to the left of the decimal

	if( isdigit( (unsigned char) *p ) ) {
		if( '0' == *p++ ) {

			// If the first digit is zero, it must be the
			// only digit to the lerft of the decimal

			if( isdigit( (unsigned char) *p ) )
				return 0;
		}
		else {

			// Skip oer the following digits

			while( isdigit( (unsigned char) *p ) ) ++p;
		}
	}
	else
		return 0;

	if( !*p )
		return 1;             // integer

	if( '.' == *p ) {

		++p;

		// If there is a decimal point, there must be
		// at least one digit to the right of it

		if( isdigit( (unsigned char) *p ) )
			++p;
		else
			return 0;

		// skip over contiguous digits

		while( isdigit( (unsigned char) *p ) ) ++p;
	}

	if( ! *p )
		return 1;  // decimal fraction, no exponent
	else if( *p != 'e' && *p != 'E' )
		return 0;  // extra junk, no exponent
	else
		++p;

	// If we get this far, we have the beginnings of an exponent.
	// Skip over optional sign of exponent.

	if( '-' == *p || '+' == *p )
		++p;

	// There must be at least one digit in the exponent
	
	if( isdigit( (unsigned char) *p ) )
		++p;
	else
		return 0;

	// skip over contiguous digits

	while( isdigit( (unsigned char) *p ) ) ++p;

	if( *p )
		return 0;  // extra junk
	else
		return 1;  // number with exponent
}

/**
 Allocate and reformat a numeric string into one that is valid
 by JSON rules.  If the string is not numeric, return NULL.
 Caller is responsible for freeing the buffer.
 **/
char* jsonScrubNumber( const char* s ) {
	if( !s || !*s ) return NULL;

	growing_buffer* buf = buffer_init( 64 );

	// Skip leading white space, if present

	while( isspace( (unsigned char) *s ) ) ++s;

	// Skip leading plus sign, if present, but keep a minus

	if( '-' == *s )
	{
		buffer_add_char( buf, '-' );
		++s;
	}
	else if( '+' == *s )
		++s;

	if( '\0' == *s ) {
		// No digits found

		buffer_free( buf );
		return NULL;
	}
	// Skip any leading zeros

	while( '0' == *s ) ++s;

	// Capture digits to the left of the decimal,
	// and note whether there are any.

	int left_digit = 0;  // boolean

	if( isdigit( (unsigned char) *s ) ) {
		buffer_add_char( buf, *s++ );
		left_digit = 1;
	}
	
	while( isdigit( (unsigned char) *s  ) )
		buffer_add_char( buf, *s++ );

	// Now we expect to see a decimal point,
	// an exponent, or end-of-string.

	switch( *s )
	{
		case '\0' :
			break;
		case '.' :
		{
			// Add a single leading zero, if we need to

			if( ! left_digit )
				buffer_add_char( buf, '0' );
			buffer_add_char( buf, '.' );
			++s;

			if( ! left_digit && ! isdigit( (unsigned char) *s ) )
			{
				// No digits on either side of decimal

				buffer_free( buf );
				return NULL;
			}

			// Collect digits to right of decimal

			while( isdigit( (unsigned char) *s ) )
				buffer_add_char( buf, *s++ );

			break;
		}
		case 'e' :
		case 'E' :

			// Exponent; we'll deal with it later, but
			// meanwhile make sure we have something
			// to its left

			if( ! left_digit )
				buffer_add_char( buf, '1' );
			break;
		default :

			// Unexpected character; bail out

			buffer_free( buf );
			return NULL;
	}

	if( '\0' == *s )    // Are we done yet?
		return buffer_release( buf );

	if( 'e' != *s && 'E' != *s ) {

		// Unexpected character: bail out

		buffer_free( buf );
		return NULL;
	}

	// We have an exponent.  Load the e or E,
	// and the sign if there is one.

	buffer_add_char( buf, *s++ );

	if( '+' == *s || '-' == *s )
		buffer_add_char( buf, *s++ );

	// Collect digits of the exponent

	while( isdigit( (unsigned char) *s ) )
		buffer_add_char( buf, *s++ );

	// There better not be anything left

	if( *s ) {
		buffer_free( buf );
		return NULL;
	}

	return buffer_release( buf );
}
