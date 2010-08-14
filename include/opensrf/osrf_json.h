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
	@file osrf_json.h
	@brief Header for parsing JSON structures and representing them in memory.

	JSON is a format for representing hierarchical data structures as text.

	A JSON string may be a quoted string, a numeric literal, or any of the keywords true, false
	and null.

	A JSON string may also be an array, i.e. a series of values, separated by commas and enclosed
	in square brackets.  For example: [ "Adams", 42, null, true ]

	A JSON string may also be an object, i.e. a series of name/value pairs, separated by commas
	and enclosed by curly braces.  Each name/value pair is a quoted string, followed by a colon,
	followed by a value.  For example: { "Author":"Adams", "answer":42, "question":null,
	"profound": true }

	The values in a JSON array or object may themselves be arrays or objects, nested to any
	depth, in a hierarchical structure of arbitrary complexity.  For more information about
	JSON, see http://json.org/.

	Like a JSON string, a jsonObject can take several different forms.  It can hold a string, a
	number, a boolean, or a null.  It can also hold an array, implemented internally by an
	osrfList (see osrf_list.h).  It can also hold a series of name/value pairs, implemented
	internally by an osrfHash (see osrf_hash.h).

	A jsonObject can also tag its contents with a class name, typically referring to a
	database table or view.  Such an object can be translated into a JSON string where the
	class is encoded as the value of a name/value pair, with the original jsonObject encoded
	as the value of a second name/value pair.
*/

#ifndef JSON_H
#define JSON_H

#include <opensrf/utils.h>
#include <opensrf/osrf_list.h>
#include <opensrf/osrf_hash.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@name JSON types
	@brief Macros defining types of jsonObject.

	A jsonObject includes a @em type member with one of these values, identifying the type of
	jsonObject.  Client code should treat the @em type member as read-only.
*/
/*@{*/
#define JSON_HASH 	0
#define JSON_ARRAY	1
#define JSON_STRING	2
#define JSON_NUMBER	3
#define JSON_NULL 	4
#define JSON_BOOL 	5
/*@}*/

/**
	@name JSON extensions

	These two macros define tags used for encoding class information.  @em JSON_CLASS_KEY
	labels a class name, and @em JSON_DATA_KEY labels an associated value.

	Because each of these macros is subject to an ifndef clause, client code may override
	the default choice of labels by defining alternatives before including this header.
	At this writing, this potential for customization is unused.
*/
/*@{*/
#ifndef JSON_CLASS_KEY
#define JSON_CLASS_KEY "__c"
#endif
#ifndef JSON_DATA_KEY
#define JSON_DATA_KEY "__p"
#endif
/*@}*/

/**
	@brief Representation of a JSON string in memory

	Different types of jsonObject use different members of the @em value union.

	Numbers are stored as character strings, using the same buffer that we use for
	strings.  As a result, a JSON_NUMBER can store numbers that cannot be represented
	by native C types.

	(We used to store numbers as doubles.  We still have the @em n member lying around as
	a relic of those times, but we don't use it.  We can't get rid of it yet, either.  Long
	story.)
*/
struct _jsonObjectStruct {
	unsigned long size;     /**< Number of sub-items. */
	char* classname;        /**< Optional class hint (not part of the JSON spec). */
	int type;               /**< JSON type. */
	struct _jsonObjectStruct* parent;   /**< Whom we're attached to. */
	/** Union used for various types of cargo. */
	union _jsonValue {
		osrfHash*	h;      /**< Object container. */
		osrfList*	l;      /**< Array container. */
		char* 		s;      /**< String or number. */
		int 		b;      /**< Bool. */
		double	n;          /**< Number (no longer used). */
	} value;
};
typedef struct _jsonObjectStruct jsonObject;

/**
	@brief Iterator for traversing a jsonObject.

	A jsonIterator traverses a jsonIterator only at a single level.  It does @em not descend
	into lower levels to traverse them recursively.
*/
struct _jsonIteratorStruct {
	jsonObject* obj;           /**< The object we're traversing. */
	osrfHashIterator* hashItr; /**< The iterator for this hash. */
	const char* key;           /**< If this object is a hash, the current key. */
	unsigned long index;       /**< If this object is an array, the index. */
};
typedef struct _jsonIteratorStruct jsonIterator;

/**
	@brief Macros for upward compatibility with an old, defunct version
    of the JSON parser.
*/
/*@{*/
#define jsonParseString                jsonParse
#define jsonParseStringRaw             jsonParseRaw
#define jsonParseStringFmt             jsonParseFmt
/*@}*/

jsonObject* jsonParse( const char* str );

jsonObject* jsonParseRaw( const char* str );

jsonObject* jsonParseFmt( const char* str, ... );

jsonObject* jsonNewObject(const char* data);

jsonObject* jsonNewObjectFmt(const char* data, ...);

jsonObject* jsonNewObjectType(int type);

jsonObject* jsonNewNumberObject( double num );

jsonObject* jsonNewNumberStringObject( const char* numstr );

jsonObject* jsonNewBoolObject(int val);

void jsonObjectFree( jsonObject* o );

void jsonObjectFreeUnused( void );

unsigned long jsonObjectPush(jsonObject* o, jsonObject* newo);

unsigned long jsonObjectSetKey(
		jsonObject* o, const char* key, jsonObject* newo);

/*
 * Turns the object into a JSON string.  The string must be freed by the caller */
char* jsonObjectToJSON( const jsonObject* obj );
char* jsonObjectToJSONRaw( const jsonObject* obj );

jsonObject* jsonObjectGetKey( jsonObject* obj, const char* key );

const jsonObject* jsonObjectGetKeyConst( const jsonObject* obj, const char* key );

jsonIterator* jsonNewIterator(const jsonObject* obj);

void jsonIteratorFree(jsonIterator* itr);

jsonObject* jsonIteratorNext(jsonIterator* iter);

int jsonIteratorHasNext(const jsonIterator* itr);

jsonObject* jsonObjectGetIndex( const jsonObject* obj, unsigned long index );

unsigned long jsonObjectSetIndex(jsonObject* dest, unsigned long index, jsonObject* newObj);

unsigned long jsonObjectRemoveIndex(jsonObject* dest, unsigned long index);

jsonObject* jsonObjectExtractIndex(jsonObject* dest, unsigned long index);

unsigned long jsonObjectRemoveKey( jsonObject* dest, const char* key);

const char* jsonObjectGetString(const jsonObject*);

double jsonObjectGetNumber( const jsonObject* obj );

void jsonObjectSetString(jsonObject* dest, const char* string);

void jsonObjectSetNumber(jsonObject* dest, double num);

int jsonObjectSetNumberString(jsonObject* dest, const char* string);

void jsonObjectSetClass(jsonObject* dest, const char* classname );

const char* jsonObjectGetClass(const jsonObject* dest);

int jsonBoolIsTrue( const jsonObject* boolObj );

void jsonSetBool(jsonObject* bl, int val);

jsonObject* jsonObjectClone( const jsonObject* o );

char* jsonObjectToSimpleString( const jsonObject* o );

char* doubleToString( double num );

int jsonIsNumeric( const char* s );

char* jsonScrubNumber( const char* s );

/**
	@brief Provide an XPATH style search interface to jsonObjects.
	@param obj Pointer to the jsonObject to be searched.
	@param path Pointer to a printf-style format string specifying the search path.  Subsequent
		parameters, if any, are formatted and inserted into the formatted string.
	@return A copy of the object at the specified location, if it exists, or NULL if it doesn't.

	Example search path: /some/node/here.
	
	Every element in the path must be a proper object, a JSON_HASH.

	The calling code is responsible for freeing the jsonObject to which the returned pointer
	points.
*/
jsonObject* jsonObjectFindPath( const jsonObject* obj, const char* path, ... );


/**
	@brief Prettify a JSON string for printing, by adding newlines and other white space.
	@param jsonString Pointer to the original JSON string.
	@return Pointer to a prettified JSON string.

	The calling code is responsible for freeing the returned string.
*/
char* jsonFormatString( const char* jsonString );

/* ------------------------------------------------------------------------- */
/*
 * The following methods provide a facility for serializing and
 * deserializing "classed" JSON objects.  To give a JSON object a 
 * class, simply call jsonObjectSetClass().
 * Then, calling jsonObjectEncodeClass() will convert the JSON
 * object (and any sub-objects) to a JSON object with class
 * wrapper objects like so:
 * { "__c" : "classname", "__p" : [json_thing] }
 * In this example __c is the class key and __p is the data (object)
 * key.  The keys are defined by the constants 
 * JSON_CLASS_KEY and JSON_DATA_KEY
 * To revive a serialized object, simply call
 * jsonObjectDecodeClass()
 */


/* Converts a class-wrapped object into an object with the
 * classname set
 * Caller must free the returned object 
 */ 
jsonObject* jsonObjectDecodeClass( const jsonObject* obj );


/* Converts an object with a classname into a
 * class-wrapped (serialized) object
 * Caller must free the returned object 
 */ 
jsonObject* jsonObjectEncodeClass( const jsonObject* obj );

/* ------------------------------------------------------------------------- */


/*
 *	Generates an XML representation of a JSON object */
char* jsonObjectToXML(const jsonObject*);

/*
 * Builds a JSON object from the provided XML 
 */
jsonObject* jsonXMLToJSONObject(const char* xml);

#ifdef __cplusplus
}
#endif

#endif
