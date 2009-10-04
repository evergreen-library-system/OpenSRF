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

/* parser states */
/**
	@name Parser states
	@brief Used internally by a JSON parser.

	A jsonParserContext stores these values in order to remember where the parser is in the
	parsing.
*/
/*@{*/
#define JSON_STATE_IN_OBJECT	0x1
#define JSON_STATE_IN_ARRAY		0x2
#define JSON_STATE_IN_STRING	0x4
#define JSON_STATE_IN_UTF		0x8
#define JSON_STATE_IN_ESCAPE	0x10
#define JSON_STATE_IN_KEY		0x20
#define JSON_STATE_IN_NULL		0x40
#define JSON_STATE_IN_TRUE		0x80
#define JSON_STATE_IN_FALSE		0x100
#define JSON_STATE_IN_NUMBER	0x200
#define JSON_STATE_IS_INVALID	0x400
#define JSON_STATE_IS_DONE		0x800
#define JSON_STATE_START_COMMEN	0x1000
#define JSON_STATE_IN_COMMENT	0x2000
#define JSON_STATE_END_COMMENT	0x4000
/*@}*/

/**
	@name Parser state operations
	@ Macros to manipulate the parser state in a jsonParserContext.
*/
/*@{*/
/** Set a state. */
#define JSON_STATE_SET(ctx,s) ctx->state |= s; 
/** Unset a state. */
#define JSON_STATE_REMOVE(ctx,s) ctx->state &= ~s;
/** Check if a state is set. */
#define JSON_STATE_CHECK(ctx,s) (ctx->state & s) ? 1 : 0
/** Pop a state off the stack. */
#define JSON_STATE_POP(ctx) osrfListPop( ctx->stateStack );
/** Push a state on the stack. */
#define JSON_STATE_PUSH(ctx, state) osrfListPush( ctx->stateStack,(void*) state );
/** Check which container type we're currently in. */
#define JSON_STATE_PEEK(ctx) osrfListGetIndex(ctx->stateStack, ctx->stateStack->size -1)
/** Compare stack values. */
#define JSON_STATE_CHECK_STACK(ctx, s) (JSON_STATE_PEEK(ctx) == (void*) s ) ? 1 : 0
/** Check if a parser state is set. */
#define JSON_PARSE_FLAG_CHECK(ctx, f) (ctx->flags & f) ? 1 : 0
/*@}*/

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
	This macro is used only by a JSON parser.  It probably has no business being published
	in a header.
*/
#define JSON_PARSE_LAST_CHUNK 0x1 /* this is the last part of the string we're parsing */

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
	@brief Stores the current state of a JSON parser.

	One form of JSON parser operates as a finite state machine.  It stores the various
	JSON_STATE_* values in order to keep track of what it's doing.  It also maintains a
	stack of previous states in order to keep track of nesting.

	The internals of this struct are published in the header in order to provide the client
	with a window into the operations of the parser.  By installing its own callback functions,
	and possibly by tinkering with the insides of the jsonParserContext, the client code can
	customize the behavior of the parser.

	In practice only the default callbacks are ever installed, at this writing.  The potential
	for customized parsing is unused.
*/
struct jsonParserContextStruct {
	int state;                  /**< What are we currently parsing. */
	const char* chunk;          /**< The chunk we're currently parsing. */
	int index;                  /**< Where we are in parsing the current chunk. */
	int chunksize;              /**< Size of the current chunk. */
	int flags;                  /**< Parser flags. */
	osrfList* stateStack;       /**< The nest of object/array states. */
	growing_buffer* buffer;     /**< Buffer for building strings, numbers, and keywords. */
	growing_buffer* utfbuf;     /**< Holds the current unicode characters. */
	void* userData;             /**< Opaque user pointer.  We ignore this. */
	const struct jsonParserHandlerStruct* handler; /**< The event handler struct. */
};
typedef struct jsonParserContextStruct jsonParserContext;

/**
	@brief A collection of function pointers for customizing parser behavior.

	The client code can install pointers to its own functions in this struct, in order to
	customize the behavior of the parser at various points in the parsing.
*/
struct jsonParserHandlerStruct {
	void (*handleStartObject)	(void* userData);
	void (*handleObjectKey)		(void* userData, char* key);
	void (*handleEndObject)		(void* userData);
	void (*handleStartArray)	(void* userData);
	void (*handleEndArray)		(void* userData);
	void (*handleNull)			(void* userData);
	void (*handleString)		(void* userData, char* string);
	void (*handleBool)			(void* userData, int boolval);
	void (*handleNumber)		(void* userData, const char* numstr);
	void (*handleError)			(void* userData, char* err, ...);
};
typedef struct jsonParserHandlerStruct jsonParserHandler;

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
	@brief Allocate a new parser context object.
	@param handler Pointer to a collection of function pointers for callback functions.
	@param userData Opaque user pointer which is available to callbacks; ignored by the parser.
	@return An allocated parser context, or NULL on error.
*/
jsonParserContext* jsonNewParser( const jsonParserHandler* handler, void* userData);

/**
	@brief Free a jsonParserContext.
	@param ctx Pointer to the jsonParserContext to be freed.
*/
void jsonParserFree( jsonParserContext* ctx );

/**
	@brief Parse a chunk of data.
	@param ctx Pointer to the parser context.
	@param data Pointer the data to parse.
	@param datalen The size of the chunk to parse.
	@param flags Reserved.
*/
int jsonParseChunk( jsonParserContext* ctx, const char* data, int datalen, int flags );

/**
	@name Parsing functions
	
	There are two sets of parsing functions, which are mostly plug-compatible with each other.
	The older series:

	- jsonParseString()
	- jsonParseStringRaw()
	- jsonParseStringFmt()

	...and a newer series:

	- jsonParse();
	- jsonParseRaw();
	- jsonParseFmt();

	The first series is based on a finite state machine.  Its innards are accessible, in
	theory, through the jsonParserContext structure and through callback functions.  In
	practice this flexibility is unused at this writing.

	The second series is based on recursive descent.  It doesn't use the jsonParserContext
	structure, nor does it accept callback functions.  However it is faster than the first
	parser.  In addition its syntax checking is much stricter -- it catches many kinds of
	syntax errors that slip through the first parser.
*/
/*@{*/
/**
	@brief Parse a JSON string, with decoding of classname hints.
	@param str Pointer to the JSON string to parse.
	@return A pointer to the resulting JSON object, or NULL on error.

	If any node in the jsonObject tree is of type JSON_HASH, with a tag of JSON_CLASS_KEY
	and another tag of JSON_DATA_KEY, the parser will collapse a level.  The subobject
	tagged with JSON_DATA_KEY will replace the JSON_HASH, and the string tagged as
	JSON_CLASS_KEY will be stored as its classname.  If there is no tag of JSON_DATA_KEY,
	the hash will be replaced by a jsonObject of type JSON_NULL.

	The calling code is responsible for freeing the resulting jsonObject.
*/
jsonObject* jsonParseString( const char* str );

/**
	@brief Parse a JSON string, with no decoding of classname hints.
	@param str Pointer to the JSON string to parse.
	@return A pointer to the resulting JSON object, or NULL on error.

	This function is similar to jsonParseString(), except that it does not give any special
	treatment to a JSON_HASH with the JSON_CLASS_KEY tag.

	The calling code is responsible for freeing the resulting jsonObject.
*/
jsonObject* jsonParseStringRaw( const char* str );

/**
	@brief Parse a JSON string received as a printf-style format string.
	@param str A printf-style format string.  Subsequent arguments, if any, are formatted
		and inserted into the JSON string before parsing.
	@return A pointer to the resulting JSON object, or NULL on error.

	Unlike jsonParseString(), this function does not give any special treatment to a
	JSON_HASH with tags JSON_CLASS_KEY or JSON_DATA_KEY.

	The calling code is responsible for freeing the resulting jsonObject.
*/
jsonObject* jsonParseStringFmt( const char* str, ... );

jsonObject* jsonParse( const char* str );
jsonObject* jsonParseRaw( const char* str );
jsonObject* jsonParseFmt( const char* s, ... );
/*@}*/

/**
	@brief Parses a JSON string, using a customized error handler.
	@param errorHandler A function pointer to an error-handling function.
	@param str The string to parse.
	@return The resulting JSON object, or NULL on error.
*/
jsonObject* jsonParseStringHandleError( void (*errorHandler) (const char*), char* str, ... );

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

/* sets the error handler for all parsers */
void jsonSetGlobalErrorHandler(void (*errorHandler) (const char*));

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
