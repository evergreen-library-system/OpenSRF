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

#include <ctype.h>
#include "opensrf/osrf_json.h"

static jsonObject* findMultiPath( const jsonObject* o,
		const char* root, const char* path );
static jsonObject* findMultiPathRecurse( const jsonObject* o, const char* root );
static jsonObject* _jsonObjectEncodeClass( const jsonObject* obj, int ignoreClass );

/**
	@brief Append some spaces to a growing_buffer, for indentation.
	@param buf Pointer to the growing_buffer.
	@param depth Degree of indentation.

	We append 2 spaces per degree of indentation.
*/
static void append_indentation( growing_buffer* buf, int depth ) {
	size_t n = 2 * depth;
	char indent[ n ];
	memset( indent, ' ', n );
	osrf_buffer_add_n( buf, indent, n );
}

/**
	@brief Make a prettyprint copy of a JSON string.
	@param string Pointer to the JSON string to be formatted.
	@return Pointer to a newly allocated and reformatted JSON string.

	Create a copy of the input JSON string, with newlines and
	indentation for readability.

	If the input pointer is NULL, return an empty string.

	WARNING: if the input JSON is not well-formed, the output JSON is likely
	to be even worse-formed.

	The calling code is responsible for freeing the formatted copy.
*/
char* jsonFormatString( const char* string ) {
	if( !string ) return strdup( "" );

	growing_buffer* buf = osrf_buffer_init( 64 );
	int i;
	int depth = 0;
	int in_quote = 0;   // boolean; true if in a string literal
	int escaped = 0;    // boolean: true if previous character was a backslash
	int beginning = 1;  // boolean: true if we're starting a new line

	char c;
	for( i = 0; string[i]; i++ ) {
		c = string[ i ];

		if( c == '{' || c == '[' ) {

			OSRF_BUFFER_ADD_CHAR( buf, c );
			if( !in_quote ) {
				OSRF_BUFFER_ADD_CHAR( buf, '\n' );
				append_indentation( buf, ++depth );
				beginning = 1;
			}

		} else if( c == '}' || c == ']' ) {

			if( !in_quote ) {
				OSRF_BUFFER_ADD_CHAR( buf, '\n' );
				append_indentation( buf, --depth );
				beginning = 1;
			}
			OSRF_BUFFER_ADD_CHAR( buf, c );

		} else if( c == ',' ) {

			OSRF_BUFFER_ADD_CHAR( buf, ',' );
			if( !in_quote ) {
				OSRF_BUFFER_ADD_CHAR( buf, '\n' );
				append_indentation( buf, depth );
				beginning = 1;
			}

		} else {
			// Ignore white space at the beginning of a line
			if( beginning ) {
				if( !isspace( (unsigned char) c )) {
					OSRF_BUFFER_ADD_CHAR( buf, c );
					beginning = 0;
				}
			} else {
				OSRF_BUFFER_ADD_CHAR( buf, c );
			}
		}

		if( '\\' == c )
			escaped = !escaped;
		else {
			if( '\"' == c && !escaped )
				in_quote = !in_quote;
			escaped = 0;
		}
	}

    return osrf_buffer_release( buf );
}



jsonObject* jsonObjectDecodeClass( const jsonObject* obj ) {
	if(!obj) return jsonNewObject(NULL);

	jsonObject* newObj			 = NULL; 
	const jsonObject* classObj	 = NULL;
	const jsonObject* payloadObj = NULL;
	unsigned int i;

	if( obj->type == JSON_HASH ) {

		/* are we a special class object? */
		if( (classObj = jsonObjectGetKeyConst( obj, JSON_CLASS_KEY )) ) {

			/* do we have a payload */
			if( (payloadObj = jsonObjectGetKeyConst( obj, JSON_DATA_KEY )) ) {
				newObj = jsonObjectDecodeClass( payloadObj );
				jsonObjectSetClass( newObj, jsonObjectGetString(classObj) );

			} else { /* class is defined but there is no payload */
				return NULL;
			}

		} else { /* we're a regular hash */

			jsonIterator* itr = jsonNewIterator(obj);
			jsonObject* tmp;
			newObj = jsonNewObjectType(JSON_HASH);
			while( (tmp = jsonIteratorNext(itr)) ) {
				jsonObject* o = jsonObjectDecodeClass(tmp);
				jsonObjectSetKey( newObj, itr->key, o );
			}
			jsonIteratorFree(itr);
			if( obj->classname )
				jsonObjectSetClass( newObj, obj->classname );
		}

	} else {

		if( obj->type == JSON_ARRAY ) { /* we're an array */
			newObj = jsonNewObjectType(JSON_ARRAY);
			for( i = 0; i != obj->size; i++ ) {
				jsonObject* tmp = jsonObjectDecodeClass(jsonObjectGetIndex( obj, i ) );
				jsonObjectSetIndex( newObj, i, tmp );
			}
			if( obj->classname )
				jsonObjectSetClass( newObj, obj->classname );

		} else { /* not an aggregate type */
			newObj = jsonObjectClone(obj);
		}
	}
		
	return newObj;
}

jsonObject* jsonObjectEncodeClass( const jsonObject* obj ) {
	return _jsonObjectEncodeClass( obj, 0 );
}

static jsonObject* _jsonObjectEncodeClass( const jsonObject* obj, int ignoreClass ) {

	//if(!obj) return NULL;
	if(!obj) return jsonNewObject(NULL);
	jsonObject* newObj = NULL;

	if( obj->classname && ! ignoreClass ) {
		newObj = jsonNewObjectType(JSON_HASH);

		jsonObjectSetKey( newObj, 
			JSON_CLASS_KEY, jsonNewObject(obj->classname) ); 

		jsonObjectSetKey( newObj, 
			JSON_DATA_KEY, _jsonObjectEncodeClass(obj, 1));

	} else if( obj->type == JSON_HASH ) {

		jsonIterator* itr = jsonNewIterator(obj);
		jsonObject* tmp;
		newObj = jsonNewObjectType(JSON_HASH);

		while( (tmp = jsonIteratorNext(itr)) ) {
			jsonObjectSetKey( newObj, itr->key, 
					_jsonObjectEncodeClass(tmp, 0));
		}
		jsonIteratorFree(itr);

	} else if( obj->type == JSON_ARRAY ) {

		newObj = jsonNewObjectType(JSON_ARRAY);
		unsigned int i;
		for( i = 0; i != obj->size; i++ ) {
			jsonObjectSetIndex( newObj, i, 
				_jsonObjectEncodeClass(jsonObjectGetIndex( obj, i ), 0 ));
		}

	} else {
		newObj = jsonObjectClone(obj);
	}

	return newObj;
}

jsonObject* jsonObjectFindPath( const jsonObject* obj, const char* format, ...) {
	if(!obj || !format || strlen(format) < 1) return NULL;	

	VA_LIST_TO_STRING(format);
	char* buf = VA_BUF;
	char* token = NULL;
	char* tt; /* strtok storage */

	/* special case where path starts with //  (start anywhere) */
	if(buf[0] == '/' && buf[1] == '/' && buf[2] != '\0') {

		/* copy the path before strtok_r destroys it */
		char* pathcopy = strdup(buf);

		/* grab the root of the path */
		token = strtok_r(buf, "/", &tt);
		if(!token) {
			free(pathcopy);
			return NULL;
		}

		jsonObject* it = findMultiPath(obj, token, pathcopy + 1);
		free(pathcopy);
		return it;
	}
	else
	{
		/* grab the root of the path */
		token = strtok_r(buf, "/", &tt);
		if(!token) return NULL;

		do {
			obj = jsonObjectGetKeyConst(obj, token);
		} while( (token = strtok_r(NULL, "/", &tt)) && obj);

		return jsonObjectClone(obj);
	}
}

/* --------------------------------------------------------------- */

/* Utility method. finds any object in the tree that matches the path.  
	Use this for finding paths that start with '//' */
static jsonObject* findMultiPath(const jsonObject* obj,
		const char* root, const char* path) {

	if(!obj || ! root || !path) return NULL;

	/* collect all of the potential objects */
	jsonObject* arr = findMultiPathRecurse(obj, root);

	/* path is just /root or /root/ */
	if( strlen(root) + 2 >= strlen(path) ) {
		return arr;

	} else {

		/* container for fully matching objects */
		jsonObject* newarr = jsonNewObjectType(JSON_ARRAY);
		int i;

		/* gather all of the sub-objects that match the full path */
		for( i = 0; i < arr->size; i++ ) {
			const jsonObject* a = jsonObjectGetIndex(arr, i);
			jsonObject* thing = jsonObjectFindPath(a , path + strlen(root) + 1); 

			if(thing) { //jsonObjectPush(newarr, thing);
				if(thing->type == JSON_ARRAY) {
            		int i;
					for( i = 0; i != thing->size; i++ )
						jsonObjectPush(newarr, jsonObjectClone(jsonObjectGetIndex(thing,i)));
					jsonObjectFree(thing);
				} else {
					jsonObjectPush(newarr, thing);
				}
			}
		}

		jsonObjectFree(arr);
		return newarr;
	}
}

/* returns a list of object whose key is 'root'.  These are used as
	potential objects when doing a // search */
static jsonObject* findMultiPathRecurse(const jsonObject* obj, const char* root) {

	jsonObject* arr = jsonNewObjectType(JSON_ARRAY);
	if(!obj) return arr;

	int i;

	/* if the current object has a node that matches, add it */

	const jsonObject* o = jsonObjectGetKeyConst(obj, root);
	if(o) jsonObjectPush( arr, jsonObjectClone(o) );

	jsonObject* tmp = NULL;
	jsonObject* childarr;
	jsonIterator* itr = jsonNewIterator(obj);

	/* recurse through the children and find all potential nodes */
	while( (tmp = jsonIteratorNext(itr)) ) {
		childarr = findMultiPathRecurse(tmp, root);
		if(childarr && childarr->size > 0) {
			for( i = 0; i!= childarr->size; i++ ) {
				jsonObjectPush( arr, jsonObjectClone(jsonObjectGetIndex(childarr, i)) );
			}
		}
		jsonObjectFree(childarr);
	}

	jsonIteratorFree(itr);

	return arr;
}
