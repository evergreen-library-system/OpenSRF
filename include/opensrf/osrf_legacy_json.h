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




/* ---------------------------------------------------------------------------------------
	JSON parser.
 * --------------------------------------------------------------------------------------- */
#ifndef LEGACY_JSON_H
#define LEGACY_JSON_H

#include <opensrf/osrf_json.h>
#include <ctype.h>



/* Parses the given JSON string and returns the built object. 
 *	returns NULL (and prints parser error to stderr) on error.  
 */

jsonObject* json_parse_string(char* string);

jsonObject* legacy_jsonParseString(const char* string);
jsonObject* legacy_jsonParseStringFmt( const char* string, ... );

jsonObject* json_parse_file( const char* filename );

jsonObject* legacy_jsonParseFile( const char* string );



/* does the actual parsing work.  returns 0 on success.  -1 on error and
 * -2 if there was no object to build (string was all comments) 
 */
int _json_parse_string(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* returns 0 on success and turns obj into a string object */
int json_parse_json_string(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* returns 0 on success and turns obj into a number or double object */
int json_parse_json_number(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* returns 0 on success and turns obj into an 'object' object */
int json_parse_json_object(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* returns 0 on success and turns object into an array object */
int json_parse_json_array(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* churns through whitespace and increments index as it goes.
 * eat_all == true means we should eat newlines, tabs
 */
void json_eat_ws(char* string, unsigned long* index, int eat_all, int current_strlen);

int json_parse_json_bool(char* string, unsigned long* index, jsonObject* obj, int current_strlen);

/* removes comments from a json string.  if the comment contains a class hint
 * and class_hint isn't NULL, an allocated char* with the class name will be
 * shoved into *class_hint.  returns 0 on success, -1 on parse error.
 * 'index' is assumed to be at the second character (*) of the comment
 */
int json_eat_comment(char* string, unsigned long* index, char** class_hint, int parse_class, int current_strlen);

/* prints a useful error message to stderr. always returns -1 */
int json_handle_error(char* string, unsigned long* index, char* err_msg);

int json_parse_json_null(char* string, unsigned long* index, jsonObject* obj, int current_strlen);


char* legacy_jsonObjectToJSON( const jsonObject* obj );



/* LEGACY ITERATOR CODE ---------------------------------------------------  
   ------------------------------------------------------------------------ */

struct _jsonObjectNodeStruct {
	unsigned long index; /* our array position */
	char* key; /* our hash key */
	jsonObject* item; /* our object */
};
typedef struct _jsonObjectNodeStruct jsonObjectNode;



/* utility object for iterating over hash objects */
struct _jsonObjectIteratorStruct {
    jsonIterator* iterator;
	const jsonObject* obj; /* the topic object */
	jsonObjectNode* current; /* the current node within the object */
    int done;
};
typedef struct _jsonObjectIteratorStruct jsonObjectIterator;


/** Allocates a new iterator 
	@param obj The object over which to iterate.
*/
jsonObjectIterator* jsonNewObjectIterator(const jsonObject* obj);

/** 
	De-allocates an iterator 
	@param iter The iterator object to free
*/
void jsonObjectIteratorFree(jsonObjectIterator* iter);

/** 
	Returns the object_node currently pointed to by the iterator
  	and increments the pointer to the next node
	@param iter The iterator in question.
 */
jsonObjectNode* jsonObjectIteratorNext(jsonObjectIterator* iter);

/** 
	@param iter The iterator.
	@return True if there is another node after the current node.
 */
int jsonObjectIteratorHasNext(const jsonObjectIterator* iter);


#endif


