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

/**
	@file osrf_json_object.c
	@brief Implementation of the basic operations involving jsonObjects.

	As a performance tweak: we maintain a free list of jsonObjects that have already
	been allocated but are not currently in use.  When we need to create a jsonObject,
	we can take one from the free list, if one is available, instead of calling
	malloc().  Likewise when we free a jsonObject, we can stick it on the free list
	for potential reuse instead of calling free().
*/

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <opensrf/log.h>
#include <opensrf/osrf_json.h>
#include <opensrf/osrf_json_utils.h>
#include <opensrf/osrf_utf8.h>

/* cleans up an object if it is morphing another object, also
 * verifies that the appropriate storage container exists where appropriate */
/**
	@brief Coerce a jsonObj into a specified type, if it isn't already of that type.
	@param _obj_ Pointer to the jsonObject to be coerced.
	@param newtype The desired type.

	If the old type and the new type don't match, discard and free the old contents.

	If the old type is JSON_STRING or JSON_NUMBER, free the internal string buffer even
	if the type is not changing.

	If the new type is JSON_ARRAY or JSON_HASH, make sure there is an osrfList or osrfHash
	in the jsonObject, respectively.
*/
#define JSON_INIT_CLEAR(_obj_, newtype)		\
	if( _obj_->type == JSON_HASH && newtype != JSON_HASH ) {			\
		osrfHashFree(_obj_->value.h);			\
		_obj_->value.h = NULL; 					\
	} else if( _obj_->type == JSON_ARRAY && newtype != JSON_ARRAY ) {	\
		osrfListFree(_obj_->value.l);			\
		_obj_->value.l = NULL;					\
	} else if( _obj_->type == JSON_STRING || _obj_->type == JSON_NUMBER ) { \
		free(_obj_->value.s);					\
		_obj_->value.s = NULL;					\
	} else if( _obj_->type == JSON_BOOL && newtype != JSON_BOOL ) { \
		_obj_->value.l = NULL;					\
	} \
	_obj_->type = newtype; \
	if( newtype == JSON_HASH && _obj_->value.h == NULL ) {	\
		_obj_->value.h = osrfNewHash();		\
		osrfHashSetCallback( _obj_->value.h, _jsonFreeHashItem ); \
	} else if( newtype == JSON_ARRAY && _obj_->value.l == NULL ) {	\
		_obj_->value.l = osrfNewList();		\
		_obj_->value.l->freeItem = _jsonFreeListItem;\
	}

/** Count of the times we put a freed jsonObject on the free list instead of calling free() */
static int unusedObjCapture = 0;
/** Count of the times we reused a jsonObject from the free list instead of calling malloc() */
static int unusedObjRelease = 0;
/** Count of the times we allocated a jsonObject with malloc() */
static int mallocObjCreate = 0;
/** Number of unused jsonObjects currently on the free list */
static int currentListLen = 0;

/**
	Union overlaying a jsonObject with a pointer.  When the jsonObject is not in use as a
	jsonObject, we use the overlaid pointer to maintain a linked list of unused jsonObjects.
*/
union unusedObjUnion{

	union unusedObjUnion* next;
	jsonObject obj;
};
typedef union unusedObjUnion unusedObj;

/** Pointer to the head of the free list */
static unusedObj* freeObjList = NULL;

static void add_json_to_buffer( const jsonObject* obj,
	growing_buffer * buf, int do_classname, int second_pass );

/**
	@brief Return all jsonObjects in the free list to the heap.

	Reclaims memory occupied by unused jsonObjects in the free list.  It is never really
	necessary to call this function, assuming that we don't run out of memory.  However
	it might be worth calling if we have built and destroyed a lot of jsonObjects that
	we don't expect to need again, in order to reduce our memory footprint.
*/
void jsonObjectFreeUnused( void ) {

	unusedObj* temp;
	while( freeObjList ) {
		temp = freeObjList->next;
		free( freeObjList );
		freeObjList = temp;
	}
}

/**
	@brief Create a new jsonObject, optionally containing a string.
	@param data Pointer to a string to be stored in the jsonObject; may be NULL.
	@return Pointer to a newly allocate jsonObject.

	If @a data is NULL, create a jsonObject of type JSON_NULL.  Otherwise create
	a jsonObject of type JSON_STRING, containing the specified string.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
*/
jsonObject* jsonNewObject(const char* data) {

	jsonObject* o;

	// Allocate a jsonObject; from the free list if possible,
	// or from the heap if necessary.
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

/**
	@brief Create a jsonObject, optionally containing a formatted string.
	@param data Pointer to a printf-style format string; may be NULL.  Subsequent parameters,
	if any, will be formatted and inserted into the resulting string.
	@return Pointer to a newly created jsonObject.

	If @a data is NULL, create a jsonObject of type JSON_NULL, and ignore any extra parameters.
	Otherwise create a jsonObject of type JSON_STRING, containing the formatted string.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
 */
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

/**
	@brief Create a new jsonObject of type JSON_NUMBER.
	@param num The number to store in the jsonObject.
	@return Pointer to the newly created jsonObject.

	The number is stored internally as a character string, as formatted by
	doubleToString().

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
*/
jsonObject* jsonNewNumberObject( double num ) {
	jsonObject* o = jsonNewObject(NULL);
	o->type = JSON_NUMBER;
	o->value.s = doubleToString( num );
	return o;
}

/**
	@brief Create a new jsonObject of type JSON_NUMBER from a numeric string.
	@param numstr Pointer to a numeric character string.
	@return Pointer to a newly created jsonObject containing the numeric value, or NULL
		if the string is not numeric.

	The jsonIsNumeric() function determines whether the input string is numeric.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
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

/**
	@brief Create a new jsonObject of type JSON_BOOL, with a specified boolean value.
	@param val An int used as a boolean value.
	@return Pointer to the new jsonObject.

	Zero represents false, and non-zero represents true, according to the usual convention.
	In practice the value of @a val is stored unchanged, but the calling code should not try to
	take advantage of that fact, because future versions may behave differently.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
*/
jsonObject* jsonNewBoolObject(int val) {
    jsonObject* o = jsonNewObject(NULL);
    o->type = JSON_BOOL;
    jsonSetBool(o, val);
    return o;
}

/**
	@brief Create a new jsonObject of a specified type, with a default value.
	@param type One of the 6 JSON types, as specified by the JSON_* macros.
	@return Pointer to the new jsonObject.

	An invalid type parameter will go unnoticed, and may lead to unpleasantness.

	The default value is equivalent to an empty string (for a JSON_STRING), zero (for a
	JSON_NUMBER), an empty hash (for a JSON_HASH), an empty array (for a JSON_ARRAY), or
	false (for a JSON_BOOL).  A JSON_NULL, of course, has no value, but can still be
	useful.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
*/
jsonObject* jsonNewObjectType(int type) {
	jsonObject* o = jsonNewObject(NULL);
	o->type = type;
	if( JSON_BOOL == type )
		o->value.b = 0;
	return o;
}

/**
	@brief Free a jsonObject and everything in it.
	@param o Pointer to the jsonObject to be freed.

	Any jsonObjects stored inside the jsonObject (in hashes or arrays) will be freed as
	well, and so one, recursively.
*/
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

/**
	@brief Free a jsonObject through a void pointer.
	@param key Not used.
	@param item Pointer to the jsonObject to be freed, cast to a void pointer.

	This function is a callback for freeing jsonObjects stored in an osrfHash.
*/
static void _jsonFreeHashItem(char* key, void* item){
	if(!item) return;
	jsonObject* o = (jsonObject*) item;
	o->parent = NULL; /* detach the item */
	jsonObjectFree(o);
}

/**
	@brief Free a jsonObject through a void pointer.
	@param item Pointer to the jsonObject to be freed, cast to a void pointer.

	This function is a callback for freeing jsonObjects stored in an osrfList.
 */
static void _jsonFreeListItem(void* item){
	if(!item) return;
	jsonObject* o = (jsonObject*) item;
	o->parent = NULL; /* detach the item */
	jsonObjectFree(o);
}

/**
	@brief Assign a boolean value to a jsonObject of type JSON_BOOL.
	@param bl Pointer to the jsonObject.
	@param val The boolean value to be applied, encoded as an int.

	If the jsonObject is not already of type JSON_BOOL, it is converted to one, and
	any previous contents are freed.

	Zero represents false, and non-zero represents true, according to the usual convention.
	In practice the value of @a val is stored unchanged, but the calling code should not try to
	take advantage of that fact, because future versions may behave differently.
*/
void jsonSetBool(jsonObject* bl, int val) {
    if(!bl) return;
    JSON_INIT_CLEAR(bl, JSON_BOOL);
    bl->value.b = val;
}

/**
	@brief Insert one jsonObject into a jsonObect of type JSON_ARRAY, at the end of the array.
	@param o Pointer to the outer jsonObject that will receive additional payload.
	@param newo Pointer to the inner jsonObject, or NULL.
	@return The number of jsonObjects directly subordinate to the outer jsonObject,
		or -1 for error.

	If the pointer to the outer jsonObject is NULL, jsonObjectPush returns -1.

	If the outer jsonObject is not already of type JSON_ARRAY, it is converted to one, and
	any previous contents are freed.

	If the pointer to the inner jsonObject is NULL, jsonObjectPush creates a new jsonObject
	of type JSON_NULL, and appends it to the array.
*/
unsigned long jsonObjectPush(jsonObject* o, jsonObject* newo) {
    if(!o) return -1;
    if(!newo) newo = jsonNewObject(NULL);
	JSON_INIT_CLEAR(o, JSON_ARRAY);
	newo->parent = o;
	osrfListPush( o->value.l, newo );
	o->size = o->value.l->size;
	return o->size;
}

/**
	@brief Insert a jsonObject at a specified position in a jsonObject of type JSON_ARRAY.
	@param dest Pointer to the outer jsonObject that will receive the new payload.
	@param index A zero-based subscript specifying the position of the new jsonObject.
	@param newObj Pointer to the new jsonObject to be inserted, or NULL.
	@return The size of the internal osrfList where the array is stored, or -1 on error.

	If @a dest is NULL, jsonObjectSetIndex returns -1.

	If the outer jsonObject is not already of type JSON_ARRAY, it is converted to one, and
	any previous contents are freed.

	If @a newObj is NULL, jsonObject creates a new jsonObject of type JSON_NULL and
	inserts it into the array.

	If there is already a jsonObject at the specified location, it is freed and replaced.

	Depending on the placement of the inner jsonObject, it may leave unoccupied holes in the
	array that are included in the reported size.  As a result of this and other peculiarities
	of the underlying osrfList within the jsonObject, the reported size may not reflect the
	number of jsonObjects in the array.  See osrf_list.c for further details.
*/
unsigned long jsonObjectSetIndex(jsonObject* dest, unsigned long index, jsonObject* newObj) {
	if(!dest) return -1;
	if(!newObj) newObj = jsonNewObject(NULL);
	JSON_INIT_CLEAR(dest, JSON_ARRAY);
	newObj->parent = dest;
	osrfListSet( dest->value.l, newObj, index );
	dest->size = dest->value.l->size;
	return dest->value.l->size;
}

/**
	@brief Insert a jsonObject into a jsonObject of type JSON_HASH, for a specified key.
	@param o Pointer to the outer jsonObject that will receive the new payload.
	@param key Pointer to a string that will serve as the key.
	@param newo Pointer to the new jsonObject that will be inserted, or NULL.
	@return The number of items stored in the first level of the hash.

	If the @a o parameter is NULL, jsonObjectSetKey returns -1.

	If the outer jsonObject is not already of type JSON_HASH, it will be converted to one,
	and any previous contents will be freed.

	If @a key is NULL, jsonObjectSetKey returns the size of the hash without doing anything
	(apart from possibly converting the outer jsonObject as described above).

	If @a newo is NULL, jsonObjectSetKey creates a new jsonObject of type JSON_NULL and inserts
	it with the specified key.

	If a previous jsonObject is already stored with the same key, it is freed and replaced.
*/
unsigned long jsonObjectSetKey( jsonObject* o, const char* key, jsonObject* newo) {
    if(!o) return -1;
    if(!newo) newo = jsonNewObject(NULL);
	JSON_INIT_CLEAR(o, JSON_HASH);
	newo->parent = o;
	osrfHashSet( o->value.h, newo, key );
	o->size = osrfHashGetCount(o->value.h);
	return o->size;
}

/**
	@brief From a jsonObject of type JSON_HASH, find the inner jsonObject for a specified key.
	@param obj Pointer to the outer jsonObject.
	@param key The key for the associated item to be found.
	@return A pointer to the inner jsonObject, if found, or NULL if not.

	Returns NULL if either parameter is NULL, or if @a obj points to a jsonObject not of type
	JSON_HASH, or if the specified key is not found in the outer jsonObject.

	The returned pointer (if not NULL) points to the interior of the outer jsonObject.  The
	calling code should @em not try to free it, but it may change its contents.
*/
jsonObject* jsonObjectGetKey( jsonObject* obj, const char* key ) {
	if(!(obj && obj->type == JSON_HASH && obj->value.h && key)) return NULL;
	return osrfHashGet( obj->value.h, key);
}

/**
	@brief From a jsonObject of type JSON_HASH, find the inner jsonObject for a specified key.
	@param obj Pointer to the outer jsonObject.
	@param key The key for the associated item to be found.
	@return A pointer to the inner jsonObject, if found, or NULL if not.

	This function is identical to jsonObjectGetKey(), except that the outer jsonObject may be
	const, and the pointer returned points to const.  As a result, the calling code is put on
	notice that it should not try to modify the contents of the inner jsonObject.
 */
const jsonObject* jsonObjectGetKeyConst( const jsonObject* obj, const char* key ) {
	if(!(obj && obj->type == JSON_HASH && obj->value.h && key)) return NULL;
	return osrfHashGet( obj->value.h, key);
}

/**
	@brief Recursively traverse a jsonObject, translating it into a JSON string.
	@param obj Pointer to the jsonObject to be translated.
	@param buf Pointer to a growing_buffer that will receive the JSON string.
	@param do_classname Boolean; if true, expand class names.
	@param second_pass Boolean; should always be false except for some recursive calls.
 
	If @a do_classname is true, expand any class names, as described in the discussion of
	jsonObjectToJSON().

	@a second_pass should always be false except for some recursive calls.  It is used
	when expanding classnames, to distinguish between the first and second passes
	through a given node.
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
			buffer_append_utf8(buf, obj->value.s);
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
				buffer_append_utf8(buf, osrfHashIteratorKey(itr));
				OSRF_BUFFER_ADD(buf, "\":");
				add_json_to_buffer( item, buf, do_classname, second_pass );
			}

			osrfHashIteratorFree(itr);
			OSRF_BUFFER_ADD_CHAR(buf, '}');
			break;
		}
	}
}

/**
	@brief Translate a jsonObject into a JSON string, without expanding class names.
	@param obj Pointer to the jsonObject to be translated.
	@return A pointer to a newly allocated string containing the JSON.

	The calling code is responsible for freeing the resulting string.
*/
char* jsonObjectToJSONRaw( const jsonObject* obj ) {
	if(!obj) return NULL;
	growing_buffer* buf = buffer_init(32);
	add_json_to_buffer( obj, buf, 0, 0 );
	return buffer_release( buf );
}

/**
	@brief Translate a jsonObject into a JSON string, with expansion of class names.
	@param obj Pointer to the jsonObject to be translated.
	@return A pointer to a newly allocated string containing the JSON.

	At every level, any jsonObject containing a class name will be translated as if it were
	under an extra layer of JSON_HASH, with JSON_CLASS_KEY as the key for the class name and
	JSON_DATA_KEY as the key for the jsonObject.

	The calling code is responsible for freeing the resulting string.
 */
char* jsonObjectToJSON( const jsonObject* obj ) {
	if(!obj) return NULL;
	growing_buffer* buf = buffer_init(32);
	add_json_to_buffer( obj, buf, 1, 0 );
	return buffer_release( buf );
}

/**
	@brief Create a new jsonIterator for traversing a specified jsonObject.
	@param obj Pointer to the jsonObject to be traversed.
	@return A pointer to the newly allocated jsonIterator, or NULL upon error.

	jsonNewIterator returns NULL if @a obj is NULL.

	The new jsonIterator does not point to any particular position within the jsonObject to
	be traversed.  The next call to jsonIteratorNext() will position it at the beginning.

	The calling code is responsible for freeing the jsonIterator by calling jsonIteratorFree().
*/
jsonIterator* jsonNewIterator(const jsonObject* obj) {
	if(!obj) return NULL;
	jsonIterator* itr;
	OSRF_MALLOC(itr, sizeof(jsonIterator));

	itr->obj    = (jsonObject*) obj;
	itr->index  = 0;
	itr->key    = NULL;

	if( obj->type == JSON_HASH )
		itr->hashItr = osrfNewHashIterator(obj->value.h);
	else
		itr->hashItr = NULL;
	
	return itr;
}

/**
	@brief Free a jsonIterator and everything in it.
	@param itr Pointer to the jsonIterator to be freed.
*/
void jsonIteratorFree(jsonIterator* itr) {
	if(!itr) return;
	osrfHashIteratorFree(itr->hashItr);
	free(itr);
}

/**
	@brief Advance a jsonIterator to the next position within a jsonObject.
	@param itr Pointer to the jsonIterator to be advanced.
	@return A Pointer to the next jsonObject within the jsonObject being traversed; or NULL.

	If the jsonObject being traversed is of type JSON_HASH, jsonIteratorNext returns a pointer
	to the next jsonObject within the internal osrfHash.  The associated key string is available
	via the pointer member itr->key.

	If the jsonObject being traversed is of type JSON_ARRAY, jsonIteratorNext returns a pointer
	to the next jsonObject within the internal osrfList.

	In either case, the jsonIterator remains at the same level within the jsonObject that it is
	traversing.  It does @em not descend to traverse deeper levels recursively.

	If there is no next jsonObject within the jsonObject being traversed, jsonIteratorNext
	returns NULL.  It also returns NULL if @a itr is NULL, or if the jsonIterator is
	detectably corrupted, or if the jsonObject to be traversed is of a type other than
	JSON_HASH or JSON_ARRAY.

	Once jsonIteratorNext has returned NULL, subsequent calls using the same iterator will
	continue to return NULL.  There is no available function to start over at the beginning.

	The pointer returned, if not NULL, points to an internal element of the jsonObject being
	traversed.  The calling code should @em not try to free it, but it may modify its contents.
*/
jsonObject* jsonIteratorNext(jsonIterator* itr) {
	if(!(itr && itr->obj)) return NULL;
	if( itr->obj->type == JSON_HASH ) {
		if(!itr->hashItr)
			return NULL;

		jsonObject* item = osrfHashIteratorNext(itr->hashItr);
		if( item )
			itr->key = osrfHashIteratorKey(itr->hashItr);
		else
			itr->key = NULL;
		return item;
	} else {
		return jsonObjectGetIndex( itr->obj, itr->index++ );
	}
}

/**
	@brief Determine whether a jsonIterator is positioned at the last element of a jsonObject.
	@param itr Pointer to the jsonIterator whose position is to be tested.
	@return An int, as boolean: 0 if the iterator is positioned at the end, or 1 if it isn't.

	If the jsonIterator is positioned where a call to jsonIteratorNext() would return NULL,
	then jsonIteratorHasNext returns 0.  Otherwise it returns 1.
*/
int jsonIteratorHasNext(const jsonIterator* itr) {
	if(!(itr && itr->obj)) return 0;
	if( itr->obj->type == JSON_HASH )
		return osrfHashIteratorHasNext( itr->hashItr );
	return (itr->index < itr->obj->size) ? 1 : 0;
}

/**
	@brief Fetch a pointer to a specified element within a jsonObject of type JSON_ARRAY.
	@param obj Pointer to the outer jsonObject.
	@param index A zero-based index identifying the element to be fetched.
	@return A pointer to the element at the specified location, if any, or NULL.

	The return value is NULL if @a obj is null, or if the outer jsonObject is not of type
	JSON_ARRAY, or if there is no element at the specified location.

	If not NULL, the pointer returned points to an element within the outer jsonObject.  The
	calling code should @em not try to free it, but it may modify its contents.
*/
jsonObject* jsonObjectGetIndex( const jsonObject* obj, unsigned long index ) {
	if(!obj) return NULL;
	return (obj->type == JSON_ARRAY) ? 
        (OSRF_LIST_GET_INDEX(obj->value.l, index)) : NULL;
}

/**
	@brief Remove a specified element from a jsonObject of type JSON_ARRAY.
	@param dest Pointer to the jsonObject from which the element is to be removed.
	@param index A zero-based index identifying the element to be removed.
	@return The number of elements remaining at the top level, or -1 upon error.

	The return value is -1 if @a dest is NULL, or if it points to a jsonObject not of type
	JSON_ARRAY.  Otherwise it reflects the number of elements remaining in the top level of
	the outer jsonObject, not counting any at lower levels.
*/
unsigned long jsonObjectRemoveIndex(jsonObject* dest, unsigned long index) {
	if( dest && dest->type == JSON_ARRAY ) {
		osrfListRemove(dest->value.l, index);
		return dest->value.l->size;
	}
	return -1;
}

/**
	@brief Extract a specified element from a jsonObject of type JSON_ARRAY.
	@param dest Pointer to the jsonObject from which the element is to be extracted.
	@param index A zero-based index identifying the element to be extracted.
	@return A pointer to the extracted element, if successful; otherwise NULL.

	THe return value is NULL if @a dest is NULL, or if it points to a jsonObject not of type
	JSON_ARRAY, or if there is no element at the specified location.

	Otherwise, the calling code assumes ownership of the jsonObject to which the return value
	points, and is responsible for freeing it by calling jsonObjectFree().  The original outer
	jsonObject remains unchanged except for the removal of the specified element.

	This function is sijmilar to jsonObjectRemoveIndex(), except that it returns a pointer to
	the removed sub-object instead of destroying it.
*/
jsonObject* jsonObjectExtractIndex(jsonObject* dest, unsigned long index) {
	if( dest && dest->type == JSON_ARRAY ) {
		jsonObject* obj = osrfListExtract(dest->value.l, index);
		if( obj )
			obj->parent = NULL;
		return obj;
	} else
		return NULL;
}

/**
	@brief Remove an element, specified by key, from a jsonObject of type JSON_HASH.
	@param dest Pointer to the outer jsonObject from which an element is to be removed.
	@param key The key for the associated element to be removed.
	@return 1 if successful, or -1 if not.

	The operation is considered successful if @a dest and @a key are both non-NULL, and
	@a dest points to a jsonObject of type JSON_HASH, even if the specified key is not found.
*/
unsigned long jsonObjectRemoveKey( jsonObject* dest, const char* key) {
	if( dest && key && dest->type == JSON_HASH ) {
		osrfHashRemove(dest->value.h, key);
		return 1;
	}
	return -1;
}

/**
	@brief Format a double into a character string.
	@param num The double to be formatted.
	@return A newly allocated character string containing the formatted number.

	The number is formatted according to the printf-style format specification "%.30g". and
	will therefore contain no more than 30 significant digits.

	The calling code is responsible for freeing the resulting string.
*/
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

/**
	@brief Fetch a pointer to the string stored in a jsonObject, if any.
	@param obj Pointer to the jsonObject.
	@return Pointer to the string stored internally, or NULL.

	If @a obj points to a jsonObject of type JSON_STRING or JSON_NUMBER, the returned value
	points to the string stored internally (a numeric string in the case of a JSON_NUMBER).
	Otherwise the returned value is NULL.

	The returned pointer should be treated as a pointer to const.  In particular it should
	@em not be freed.  In a future release, the returned pointer may indeed be a pointer
	to const.
*/
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

/**
	@brief Translate a jsonObject to a double.
	@param @obj Pointer to the jsonObject.
	@return The numeric value stored in the jsonObject.

	If @a obj is NULL, or if it points to a jsonObject not of type JSON_NUMBER, the value
	returned is zero.
*/
double jsonObjectGetNumber( const jsonObject* obj ) {
	return (obj && obj->type == JSON_NUMBER && obj->value.s)
			? strtod( obj->value.s, NULL ) : 0;
}

/**
	@brief Store a copy of a specified character string in a jsonObject of type JSON_STRING.
	@param dest Pointer to the jsonObject in which the string will be stored.
	@param string Pointer to the string to be stored.

	Both @a dest and @a string must be non-NULL.

	If the jsonObject is not already of type JSON_STRING, it is converted to a JSON_STRING,
	with any previous contents freed.
*/
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
/**
	@brief Store a copy of a numeric character string in a jsonObject of type JSON_NUMBER.
	@param dest Pointer to the jsonObject in which the number will be stored.
	@param string Pointer to the numeric string to be stored.

	Both @a dest and @a string must be non-NULL.

	If the jsonObject is not already of type JSON_NUMBER, it is converted to a JSON_STRING,
	with any previous contents freed.

	If the input string is not numeric as determined by jsonIsNumeric(), the number stored
	is zero.
 */
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

/**
	@brief Store a number in a jsonObject of type JSON_NUMBER.
	@param dest Pointer to the jsonObject in which the number will be stored.
	@param num The number to be stored.

	If the jsonObject is not already of type JSON_NUMBER, it is converted to one, with any
	previous contents freed.
*/
void jsonObjectSetNumber(jsonObject* dest, double num) {
	if(!dest) return;
	JSON_INIT_CLEAR(dest, JSON_NUMBER);
	dest->value.s = doubleToString( num );
}

/**
	@brief Assign a class name to a jsonObject.
	@param dest Pointer to the jsonObject.
	@param classname Pointer to a string containing the class name.

	Both dest and classname must be non-NULL.
*/
void jsonObjectSetClass(jsonObject* dest, const char* classname ) {
	if(!(dest && classname)) return;
	free(dest->classname);
	dest->classname = strdup(classname);
}

/**
	@brief Fetch a pointer to the class name of a jsonObject, if any.
	@param dest Pointer to the jsonObject.
	@return Pointer to a string containing the class name, if there is one; or NULL.

	If not NULL, the pointer returned points to a string stored internally within the
	jsonObject.  The calling code should @em not try to free it.
*/
const char* jsonObjectGetClass(const jsonObject* dest) {
    if(!dest) return NULL;
    return dest->classname;
}

/**
	@brief Create a copy of an existing jsonObject, including all internal sub-objects.
	@param o Pointer to the jsonObject to be copied.
	@return A pointer to the newly created copy.

	The calling code is responsible for freeing the copy of the original.
*/
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

/**
	@brief Return the truth or falsity of a jsonObject of type JSON_BOOL.
	@param boolObj Pointer to the jsonObject.
	@return 1 or 0, depending on whether the stored boolean is true or false.

	If @a boolObj is NULL, or if it points to a jsonObject not of type JSON_BOOL, the
	returned value is zero.
*/
int jsonBoolIsTrue( const jsonObject* boolObj ) {
    if( boolObj && boolObj->type == JSON_BOOL && boolObj->value.b )
        return 1;
    return 0;
}


/**
	@brief Create a copy of the string stored in a jsonObject.
	@param o Pointer to the jsonObject whose string is to be copied.
	@return Pointer to a newly allocated string copied from the jsonObject.

	If @a o is NULL, or if it points to a jsonObject not of type JSON_STRING or JSON_NUMBER,
	the returned value is NULL.  In the case of a JSON_NUMBER, the string created is numeric.

	The calling code is responsible for freeing the newly created string.
*/
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
/**
	@brief Determine whether a specified character string is a valid JSON number.
	@param s Pointer to the string to be examined.
	@return 1 if the string is numeric, or 0 if not.

	This function defines numericity according to JSON rules; see http://json.org/.  This
	determination is based purely on the lexical properties of the string.  In particular
	there is no guarantee that the number in a numeric string is representable in C as a
	long long, a long double, or any other built-in type.

	A numeric string consists of:

	- An optional leading minus sign (but not a plus sign)
	- One or more decimal digits.  The first digit may be a zero only if it is the only
	  digit to the left of the decimal.
	- Optionally, a decimal point followed by one or more decimal digits.
	- An optional exponent, consisting of:
		- The letter E, in upper or lower case
		- An optional plus or minus sign
		- One or more decimal digits

	See also jsonScrubNumber().
*/
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
			// only digit to the left of the decimal

			if( isdigit( (unsigned char) *p ) )
				return 0;
		}
		else {

			// Skip over the following digits

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
	@brief Edit a string into a valid JSON number, if possible.
	@param s Pointer to the string to be edited.
	@return A pointer to a newly created numeric string, if possible; otherwise NULL.

	JSON has rather exacting requirements about what constitutes a valid numeric string (see
	jsonIsNumeric()).  Real-world input may be a bit sloppy.  jsonScrubNumber accepts numeric
	strings in a less formal format and reformats them, where possible, according to JSON
	rules.  It removes leading white space, a leading plus sign, and extraneous leading zeros.
	It adds a leading zero as needed when the absolute value is less than 1.  It also accepts
	scientific notation in the form of a bare exponent (e.g. "E-3"), supplying a leading factor
	of "1".

	If the input string is non-numeric even according to these relaxed rules, the return value
	is NULL.

	The calling code is responsible for freeing the newly created numeric string.
*/
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
