/*
Copyright (C) 2009 Equinox Software Inc.
Scott McKellar <scott@esilibrary.com>

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
	@file jsonpush.c
	@brief Push parser for JSON.

	This parser parses JSON incrementally, without necessarily holding the entire JSON string
	(or any representation thereof) in memory at once.  It is therefore suitable for parsing
	large input files.

	A format such as JSON, with its arbitrarily nestable elements, cries out piteously for a
	recursive descent parser to match the recursive structure of the format.  Unfortunately,
	recursive descent doesn't work for an incremental parser, because the boundaries of
	incoming chunks don't respect syntactic boundaries.

	This parser is based on a finite state automaton, using a structure to retain state across
	chunks, and a stack to simulate recursion.  The calling code designates a series of
	callback functions to respond to various syntactic features as they are encountered.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "opensrf/osrf_json.h"
#include "opensrf/jsonpush.h"

/** Enumeration of states for a finite state automaton */
typedef enum {
	PP_BEGIN,            // outside of any JSON
	PP_STR,              // inside a string literal
	PP_SLASH,            // found a backslash in a string literal
	PP_UTF8,             // collecting a UTF8 sequence
	PP_NUM,              // inside a numeric literal
	PP_ARRAY_BEGIN,      // started an array
	PP_ARRAY_VALUE,      // found an array element
	PP_ARRAY_COMMA,      // found a comma between array elements
	PP_OBJ_BEGIN,        // started a JSON object
	PP_OBJ_KEY,          // found a string for a key in an object
	PP_OBJ_COLON,        // found a colon after a key in an object
	PP_OBJ_VALUE,        // found a value for a key in an object
	PP_OBJ_COMMA,        // found a comma separating entries in an object
	PP_TRUE,             // true keyword
	PP_FALSE,            // false keyword
	PP_NULL,             // null keyword
	PP_END,              // reached the end of the JSON stream
	PP_ERROR             // encountered invalid JSON; can't continue
} PPState;

struct StateNodeStruct;
typedef struct StateNodeStruct StateNode;

/**
	@brief Represents a parser state at a given level of nesting.

	The parser maintains a stack of StateNodes to simulate recursive descent.
*/
struct StateNodeStruct {
	StateNode* next;            /**< For a linked list to implement the stack */
	PPState state;              /**< State to which we will return */
	osrfStringArray* keylist;   /**< List of key strings, if the level is for a JSON object */
};

/**
	@brief A collection of things the parser needs to remember about what it's doing.

	This structure enables the parser to retain state from one chunk of input to the next.
*/
struct JSONPushParserStruct {
	JSONHandlerMap handlers;
	void* blob;               /**< To be passed back to callback functions. */
	unsigned line;            /**< Line number. */
	unsigned pos;             /**< Character position within line. */
	PPState state;            /**< For finite state automaton. */
	char again;               /**< If non-zero, re-read it as the next character. */
	growing_buffer* buf;      /**< For accumulating strings and numbers. */
	StateNode* state_stack;   /**< For simulating recursive descent. */
	StateNode* free_states;   /**< Free list of unused StateNodes. */
	unsigned word_idx;        /**< index of current characters keyword,
	                               such as "true", "false", or "null". */
	unsigned int point_code;  /**< for UTF-8 transformations. */
	osrfStringArray* keylist; /**< Stores keys in current JSON object. */
};

// State handlers for the finite state automaton
static int do_begin( JSONPushParser* parser, char c );
static int do_str  ( JSONPushParser* parser, char c );
static int do_slash( JSONPushParser* parser, char c );
static int do_utf8 ( JSONPushParser* parser, char c );
static int do_num  ( JSONPushParser* parser, char c );
static int do_array_begin( JSONPushParser* parser, char c );
static int do_array_value( JSONPushParser* parser, char c );
static int do_array_comma( JSONPushParser* parser, char c );
static int do_obj_begin( JSONPushParser* parser, char c );
static int do_obj_key  ( JSONPushParser* parser, char c );
static int do_obj_colon( JSONPushParser* parser, char c );
static int do_obj_value( JSONPushParser* parser, char c );
static int do_obj_comma( JSONPushParser* parser, char c );
static int do_true ( JSONPushParser* parser, char c );
static int do_false( JSONPushParser* parser, char c );
static int do_null ( JSONPushParser* parser, char c );
static int do_end( JSONPushParser* parser, char c );

static int found_keyword( JSONPushParser* parser, char c,
		const char* keyword, unsigned maxlen );
static void push_pp_state( JSONPushParser* parser, PPState state );
static void pop_pp_state( JSONPushParser* parser );
static void check_pp_end( JSONPushParser* parser );
static void report_pp_error( JSONPushParser* parser, const char* msg, ... );

/**
	@brief Create a new JSONPushParser.
	@param map Pointer to a JSONHandlerMap designating the callback functions to call.
	@param blob An arbitrary pointer to be passed to the callback functions.
	@return A pointer to the new parser.

	The calling code can use the @a blob parameter to specify its own context for the
	callback functions.

	The calling code is responsible for freeing the parser by calling jsonPushParserFree().
*/
JSONPushParser* jsonNewPushParser( const JSONHandlerMap* map, void* blob )
{
	if( ! map )
		return NULL;

	JSONPushParser* parser = safe_malloc( sizeof( JSONPushParser ) );
	parser->handlers    = *map;
	parser->blob        = blob;
	parser->line        = 1;
	parser->pos         = 1;
	parser->state       = PP_BEGIN;
	parser->again       = '\0';
	parser->buf         = osrf_buffer_init( 64 );
	parser->state_stack = NULL;
	parser->free_states = NULL;
	parser->word_idx     = 0;
	parser->keylist     = osrfNewStringArray( 8 );
	return parser;
}

/**
	@brief Restore a JSONPushParser to its original pristine state.
	@param parser Pointer to the JSONPushParser to be reset.

	This function makes it possible to reuse the same parser for multiple documents, e.g.
	multiple input files, without having to destroy and recreate it.  The expectation is
	that it be called after jsonPush() returns.
*/
void jsonPushParserReset( JSONPushParser* parser ) {
	if( parser ) {
		parser->line = 1;
		parser->pos = 1;
		parser->state = PP_BEGIN;
	}
}

/**
	@brief Restore a JSONPushParser to a starting state.
	@param parser Pointer to the JSONPushParser to be resumed.

	This function is similar to jsonPushParserReset(), with two exceptions:
	- It only works if the parser is between JSON values.  Otherwise it wouldn't be able
	to continue sensibly.
	- It doesn't reset the line number or position number used for error messages.

	Purpose: make it possible to parse multiple JSON values in the same stream.  The
	expectation is that it be called by the callback function that responds to end-of-JSON.
*/
void jsonPushParserResume( JSONPushParser* parser ) {
	if( parser ) {
		parser->state = PP_BEGIN;
	}
}

/**
	@brief Tell the JSON push parser that there is no more input to parse.
	@param parser Pointer to the parser.
	@return 0 if successful, or 1 upon error.

	A call to this function is comparable to an end-of-file marker.  Without it, the parser
	would be unable to recognize certain tokens at the very end of the last buffer, because
	it wouldn't know that the token was finished.

	For example: if the last byte is part of a number, the parser will not have reported the
	numeric token because it was waiting to see if the next character was numeric.

	Likewise, certain kinds of errors would be unrecognizable, such as a failure to complete
	the current JSON expression.
*/
int jsonPushParserFinish( JSONPushParser* parser ) {
	int rc = 0;

	// If we're currently accumulating a token, finish it
	if( PP_NUM == parser->state ) {
		const char* num_str = OSRF_BUFFER_C_STR( parser->buf );

		// Validate number
		if( jsonIsNumeric( num_str ) ) {
			if( parser->handlers.handleNumber )
			rc = parser->handlers.handleNumber( parser->blob, num_str );
			pop_pp_state( parser );
			check_pp_end( parser );
		} else {                            // Not numeric?  Try to fix it
			char* temp = jsonScrubNumber( num_str );
			if( temp ) {                    // Fixed
				if( parser->handlers.handleNumber )
					rc = parser->handlers.handleNumber( parser->blob, temp );
				free( temp );
				pop_pp_state( parser );
				check_pp_end( parser );
			} else {                       // Can't be fixed
				report_pp_error( parser, "Invalid number: \"%s\"", num_str );
				rc = 1;
				parser->state = PP_ERROR;
			}
		}
	} else if( PP_TRUE == parser->state ) {
		if( 3 == parser->word_idx ) {
			if( parser->handlers.handleBool )
				rc = parser->handlers.handleBool( parser->blob, 1 );
		} else {
			report_pp_error( parser, "Keyword \"true\" is incomplete at end of input" );
			printf( "Wordlen = %d\n", parser->word_idx );
			rc = 1;
			parser->state = PP_ERROR;
		}
		pop_pp_state( parser );
		check_pp_end( parser );
	} else if( PP_FALSE == parser->state ) {
		if( 4 == parser->word_idx ) {
			if( parser->handlers.handleBool )
				rc = parser->handlers.handleBool( parser->blob, 0 );
		} else {
			report_pp_error( parser, "Keyword \"false\" is incomplete at end of input" );
			rc = 1;
			parser->state = PP_ERROR;
		}
		pop_pp_state( parser );
		check_pp_end( parser );
	} else if( PP_NULL == parser->state ) {
		if( 3 == parser->word_idx ) {
			if( parser->handlers.handleNull )
				rc = parser->handlers.handleNull( parser->blob );
		} else {
			report_pp_error( parser, "Keyword \"null\" is incomplete at end of input" );
			rc = 1;
			parser->state = PP_ERROR;
		}
		pop_pp_state( parser );
		check_pp_end( parser );
	}

	// At this point the state should be PP_END, or possibly PP_BEGIN if the JSON value is
	// empty, or PP_ERROR if we already encountered an error.  Anything else means that the
	// JSON value is incomplete.

	switch( parser->state ) {
		case PP_BEGIN       :
			parser->state = PP_END;       // JSON value was empty
			break;
		case PP_STR         :
		case PP_SLASH       :
		case PP_UTF8        :
			report_pp_error( parser, "String literal not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_NUM         :             // not possible
			break;
		case PP_ARRAY_BEGIN :
			report_pp_error( parser, "Empty JSON array not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_ARRAY_VALUE :
			report_pp_error( parser, "JSON array begun but not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_ARRAY_COMMA :
			report_pp_error( parser, "JSON array not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_OBJ_BEGIN   :
			report_pp_error( parser, "Empty JSON object not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_OBJ_KEY     :
			report_pp_error( parser, "JSON object not continued after key" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_OBJ_COLON   :
			report_pp_error( parser, "JSON object not continued after colon" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_OBJ_VALUE   :
			report_pp_error( parser, "JSON object begun but not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_OBJ_COMMA   :
			report_pp_error( parser, "JSON object not closed" );
			parser->state = PP_ERROR;
			rc = 1;
			break;
		case PP_TRUE        :   // not possible
		case PP_FALSE       :   // not possible
		case PP_NULL        :   // not possible
		case PP_END         :   // okay
		case PP_ERROR       :   // previous error, presumably already reported
			break;
	}

	return rc;
}

/**
	@brief Incrementally parse a chunk of JSON.
	@param parser Pointer to the JSONPushParser that will do the parsing.
	@param str Pointer to a chunk of JSON, either all or part of a JSON stream.
	@param length Length of the chunk of JSON.
	@return 0 if successful, or 1 upon error.

	Parse a fragment of JSON, possibly preceded or followed by one or more other chunks
	in the same JSON stream.  Respond to various syntactical features by calling the
	corresponding callback functions that were designated when the parser was created.
*/
int jsonPush( JSONPushParser* parser, const char* str, size_t length ) {
	if( ! parser )
		return 1;
	else if( ! str ) {
		report_pp_error( parser, "JSON parser received a NULL parameter for input" );
		return 1;
	} else if( PP_ERROR == parser->state ) {
		report_pp_error( parser, "JSON parser cannot continue due to previous error" );
		return 1;
	}

	int rc = 0;
	// Loop through the chunk
	int i = 0;
	while( str[i] && i < length && parser->state != PP_ERROR ) {
		// branch on the current parser state
		switch( parser->state ) {
			case PP_BEGIN :
				rc = do_begin( parser, str[i] );
				break;
			case PP_STR :
				rc = do_str( parser, str[i] );
				break;
			case PP_SLASH :
				rc = do_slash( parser, str[i] );
				break;
			case PP_UTF8 :
				rc = do_utf8( parser, str[i] );
				break;
			case PP_NUM :
				rc = do_num( parser, str[i] );
				break;
			case PP_ARRAY_BEGIN :
				rc = do_array_begin( parser, str[i] );
				break;
			case PP_ARRAY_VALUE :
				rc = do_array_value( parser, str[i] );
				break;
			case PP_ARRAY_COMMA :
				rc = do_array_comma( parser, str[i] );
				break;
			case PP_OBJ_BEGIN :
				rc = do_obj_begin( parser, str[i] );
				break;
			case PP_OBJ_KEY :
				rc = do_obj_key( parser, str[i] );
				break;
			case PP_OBJ_COLON :
				rc = do_obj_colon( parser, str[i] );
				break;
			case PP_OBJ_VALUE :
				rc = do_obj_value( parser, str[i] );
				break;
			case PP_OBJ_COMMA :
				rc = do_obj_comma( parser, str[i] );
				break;
			case PP_TRUE :
				rc = do_true( parser, str[i] );
				break;
			case PP_FALSE :
				rc = do_false( parser, str[i] );
				break;
			case PP_NULL :
				rc = do_null( parser, str[i] );
				break;
			case PP_END :
				rc = do_end( parser, str[i] );
				break;
			default :
				break;     // stub for now; should be error
		}
		if( rc )
			break;
		else if( parser->again )
			parser->again = '\0';  // reuse the current character
		else {
			// Advance to the next character
			++i;
			if( '\n' == str[i] ) {
				++parser->line;
				parser->pos = 0;
			} else
				++parser->pos;
		}
	}

	if( 1 == rc )
		parser->state = PP_ERROR;

	return rc;
}

// -------- Beginning of state handlers --------------------------

/**
	@brief Look for the beginning of a JSON value.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	After some optional leading white space, look for a value comprising the entire
	JSON stream.
*/
static int do_begin( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {         // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_END );
		parser->state = PP_STR;
	} else if( '[' == c ) {        // Found an array
		if( parser->handlers.handleBeginArray )
			rc = parser->handlers.handleBeginArray( parser->blob );
		push_pp_state( parser, PP_END );
		parser->state = PP_ARRAY_BEGIN;
	} else if( '{' == c ) {     // Found an object
		if( parser->handlers.handleBeginObj )
			rc = parser->handlers.handleBeginObj( parser->blob );
		push_pp_state( parser, PP_END );
		parser->state = PP_OBJ_BEGIN;
	} else if( 't' == c ) {
		push_pp_state( parser, PP_END );
		parser->word_idx = 0;
		parser->state = PP_TRUE;
	} else if( 'f' == c ) {
		push_pp_state( parser, PP_END );
		parser->word_idx = 0;
		parser->state = PP_FALSE;
	} else if( 'n' == c ) {
		push_pp_state( parser, PP_END );
		parser->word_idx = 0;
		parser->state = PP_NULL;
	} else if( isdigit( (unsigned char) c )
			   || '-' == c
			   || '-' == c
			   || '+' == c
			   || '.' == c
			   || 'e' == c
			   || 'E' == c ) {      // Found a number
		osrf_buffer_reset( parser->buf );
		osrf_buffer_add_char( parser->buf, c );
		push_pp_state( parser, PP_END );
		parser->state = PP_NUM;
	} else {
		report_pp_error( parser, "Unexpected character \'%c\' at beginning of JSON string", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Accumulate characters in a string literal.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.
*/
static int do_str    ( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( '\"' == c ) {
		// Reached the end of the string.  Report it either as a string
		// or as a key, depending on the context.
		pop_pp_state( parser );
		if( PP_OBJ_KEY == parser->state ) {         // Report as a key
			const char* key = OSRF_BUFFER_C_STR( parser->buf );
			if( osrfStringArrayContains( parser->keylist, key ) ) {
				report_pp_error( parser, "Duplicate key \"%s\" in JSON object", key );
				rc = 1;
			} else {
				osrfStringArrayAdd( parser->keylist, key );
				if( parser->handlers.handleObjKey ) {
					rc = parser->handlers.handleObjKey(
							parser->blob, key );
				}
			}
		} else {                                    // Report as a string
			if( parser->handlers.handleString ) {
				rc = parser->handlers.handleString(
						parser->blob, OSRF_BUFFER_C_STR( parser->buf ) );
			}
			check_pp_end( parser );
		}
	} else if( '\\' == c ) {
		parser->state = PP_SLASH;       // Handle an escaped special character
	} else if( iscntrl( (unsigned char) c ) || ! isprint( (unsigned char) c ) ) {
		report_pp_error( parser, "Illegal character 0x%02X in string literal",
			(unsigned int) c );
		rc = 1;
	} else {
		osrf_buffer_add_char( parser->buf, c );
	}

	return rc;
}

/**
	@brief Look for an escaped special character.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.
*/
static int do_slash( JSONPushParser* parser, char c ) {
	int rc = 0;

	switch( c ) {
		case '\"' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\"' );
			parser->state = PP_STR;
			break;
		case '\\' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\\' );
			parser->state = PP_STR;
			break;
		case '/' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '/' );
			parser->state = PP_STR;
			break;
		case 'b' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\b' );
			parser->state = PP_STR;
			break;
		case 'f' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\f' );
			parser->state = PP_STR;
			break;
		case 'n' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\n' );
			parser->state = PP_STR;
			break;
		case 'r' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\r' );
			parser->state = PP_STR;
			break;
		case 't' :
			OSRF_BUFFER_ADD_CHAR( parser->buf, '\t' );
			parser->state = PP_STR;
			break;
		case 'u' :
			parser->word_idx = 0;
			parser->point_code = 0;
			parser->state = PP_UTF8;
			break;
		default :
			report_pp_error( parser,
				"Unexpected character '%c' escaped by preceding backslash", c );
			rc = 1;
			break;
	}

	return rc;
}

/**
	@brief Accumulate and convert hex digits into a multibyte UTF-8 character.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character (should be a hex digit).
	@return 0 if successful, or 1 upon error.

	Convert each character to the corresponding numeric value and incorporate it into a sum.
	When all four characters have been accumulated, translate the result into a multibyte
	UTF-8 character and append it to the buffer.

	The algorithm for converting the input character into a numeric value assumes that the
	the characters [a-f] and [A-F] are contiguous in the execution character set, and that
	the lower 4 bits for 'a' and 'A' are 0001.  Those assumptions are true for ASCII and
	EBCDIC, but there may be some character sets for which it is not true.
*/
static int do_utf8( JSONPushParser* parser, char c ) {
	int rc = 0;

	if( isxdigit( (unsigned char) c ) ) {
		// Convert the numeric character to a hex value
		unsigned char hex = (c <= '9') ? c - '0' : (c & 7) + 9;

		// Branch according to how many characters we have so far
		switch( parser->word_idx ) {
			case 0 :
				parser->point_code += hex << 12;
				++parser->word_idx;
				break;
			case 1 :
				parser->point_code += hex << 8;
				++parser->word_idx;
				break;
			case 2 :
				parser->point_code += hex << 4;
				++parser->word_idx;
				break;
			default : {
				// We have all four hex characters.  Now finish the
				// point code and translate it to a UTF-8 character.
				unsigned int point_code = parser->point_code + hex;
				unsigned char ubuf[ 4 ];

				if (point_code < 0x80) {
					ubuf[0] = point_code;
					ubuf[1] = '\0';

				} else if (point_code < 0x800) {
					ubuf[0] = 0xc0 | (point_code >> 6);
					ubuf[1] = 0x80 | (point_code & 0x3f);
					ubuf[2] = '\0';

				} else {
					ubuf[0] = 0xe0 | (point_code >> 12);
					ubuf[1] = 0x80 | ((point_code >> 6) & 0x3f);
					ubuf[2] = 0x80 | (point_code & 0x3f);
					ubuf[3] = '\0';
				}

				if( ubuf[ 0 ] ) {
					// Append the UTF-8 sequence to the buffer
					OSRF_BUFFER_ADD( parser->buf, (char*) ubuf );
					parser->state = PP_STR;
				} else {
					report_pp_error( parser, "UTF-8 sequence codes for nul character" );
					rc = 1;
				}
			} // end default
		} // end switch
	} else {
		report_pp_error( parser, "Non-hex character '%c' found in UTF-8 sequence", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Accumulate characters into a numeric literal.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	Once we see a character that doesn't belong in a numeric literal, we check to make sure
	that the characters we accumulate are a well-formed number according to JSON rules.  If
	they aren't, we try to massage them into something valid (e.g. by removing a leading
	plus sign, which official JSON doesn't allow).
*/
static int do_num  ( JSONPushParser* parser, char c ) {
	int rc = 0;

	if( isdigit( (unsigned char) c )
				|| '-' == c
				|| '-' == c
				|| '+' == c
				|| '.' == c
				|| 'e' == c
				|| 'E' == c ) {
		osrf_buffer_add_char( parser->buf, c );
	} else {
		const char* num_str = OSRF_BUFFER_C_STR( parser->buf );

		// Validate number
		if( jsonIsNumeric( num_str ) ) {
			if( parser->handlers.handleNumber )
				rc = parser->handlers.handleNumber( parser->blob, num_str );
			parser->again = c;
			pop_pp_state( parser );
			check_pp_end( parser );
		} else {                            // Not valid?  Try to fix it
			char* temp = jsonScrubNumber( num_str );
			if( temp ) {                    // Fixed
				if( parser->handlers.handleNumber )
					rc = parser->handlers.handleNumber( parser->blob, temp );
				free( temp );
				parser->again = c;
				pop_pp_state( parser );
				check_pp_end( parser );
			} else {                       // Can't be fixed
				report_pp_error( parser, "Invalid number: \"%s\"", num_str );
				rc = 1;
			}
		}
	}
	return rc;
}

/**
	@brief Look for the first element of a JSON array, or the end of the array.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just entered a JSON array.  We expect to see either a value or (in the case of
	an empty array) a closing brace.  Anything else is an error.
*/
static int do_array_begin( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {    // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_STR;
	} else if( '[' == c ) {     // Found a nested array
		if( parser->handlers.handleBeginArray )
			rc  = parser->handlers.handleBeginArray( parser->blob );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_ARRAY_BEGIN;
	} else if( '{' == c ) {     // Found a nested object
		if( parser->handlers.handleBeginObj )
			rc = parser->handlers.handleBeginObj( parser->blob );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_OBJ_BEGIN;
	} else if( ']' == c ) {     // End of array
		if( parser->handlers.handleEndArray )
			rc = parser->handlers.handleEndArray( parser->blob );
		pop_pp_state( parser );
		check_pp_end( parser );
	} else if( 't' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_TRUE;
	} else if( 'f' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_FALSE;
	} else if( 'n' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_NULL;
	} else if( isdigit( (unsigned char) c )  // Found a number
				|| '-' == c
				|| '-' == c
				|| '+' == c
				|| '.' == c
				|| 'e' == c
				|| 'E' == c ) {
		osrf_buffer_reset( parser->buf );
		osrf_buffer_add_char( parser->buf, c );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_NUM;
	} else {
		report_pp_error( parser, "Unexpected character \'%c\' at beginning of array", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for the comma after a value in an array, or the end of the array.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just passed a value in a JSON array.  We expect to see either a separating
	comma or a right square bracket.
*/
static int do_array_value( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( ',' == c ) {       // Found a comma
		parser->state = PP_ARRAY_COMMA;
	} else if( ']' == c ) {     // End of array
		if( parser->handlers.handleEndArray )
			rc = parser->handlers.handleEndArray( parser->blob );
		pop_pp_state( parser );
		check_pp_end( parser );
	} else {
		report_pp_error( parser,
			"Unexpected character \'%c\' in array; expected comma or right bracket", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for the next element of a JSON array, or the end of the array.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just passed a separator comma within a JSON array.  We expect to see a value.
	Anything else is an error.
*/
static int do_array_comma( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {    // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_STR;
	} else if( '[' == c ) {     // Found a nested array
		if( parser->handlers.handleBeginArray )
			rc  = parser->handlers.handleBeginArray( parser->blob );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_ARRAY_BEGIN;
	} else if( '{' == c ) {     // Found a nested object
		if( parser->handlers.handleBeginObj )
			rc = parser->handlers.handleBeginObj( parser->blob );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_OBJ_BEGIN;
	} else if( 't' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_TRUE;
	} else if( 'f' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_FALSE;
	} else if( 'n' == c ) {
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->word_idx = 0;
		parser->state = PP_NULL;
	} else if( isdigit( (unsigned char) c )  // Found a number
				|| '-' == c
				|| '-' == c
				|| '+' == c
				|| '.' == c
				|| 'e' == c
				|| 'E' == c ) {
		osrf_buffer_reset( parser->buf );
		osrf_buffer_add_char( parser->buf, c );
		push_pp_state( parser, PP_ARRAY_VALUE );
		parser->state = PP_NUM;
	} else {
		report_pp_error( parser, "Expected array value; found \'%c\'", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for the first entry of a JSON object, or the end of the object.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just entered a JSON object.  We expect to see a string literal (the key for the
	first entry), or the end of the object.  Anything else is an error.
*/
static int do_obj_begin( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {    // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_OBJ_KEY );
		parser->state = PP_STR;
	} else if( '}' == c ) {     // End of object
		if( parser->handlers.handleEndObj )
			rc = parser->handlers.handleEndObj( parser->blob );
		pop_pp_state( parser );
		check_pp_end( parser );
	} else {
		report_pp_error( parser, "Unexpected character \'%c\' at beginning of object", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for a colon between the key and value of an entry in a JSON object.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just found the key for an entry in a JSON object.  We expect to see a colon next.
	Anything else is an error.
*/
static int do_obj_key  ( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( ':' == c ) {
		parser->state = PP_OBJ_COLON;
	} else {
		report_pp_error( parser, "Expected colon within JSON object; found \'%c\'", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for a value in a JSON object.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just found a colon after the key of an entry in a JSON object.  We expect to see
	the associated value next.  Anything else is an error.
*/
static int do_obj_colon( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {    // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->state = PP_STR;
	} else if( '[' == c ) {     // Found a nested array
		if( parser->handlers.handleBeginArray )
			rc = parser->handlers.handleBeginArray( parser->blob );
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->state = PP_ARRAY_BEGIN;
	} else if( '{' == c ) {     // Found a nested object
		if( parser->handlers.handleBeginObj )
			rc = parser->handlers.handleBeginObj( parser->blob );
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->state = PP_OBJ_BEGIN;
	} else if( 't' == c ) {
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->word_idx = 0;
		parser->state = PP_TRUE;
	} else if( 'f' == c ) {
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->word_idx = 0;
		parser->state = PP_FALSE;
	} else if( 'n' == c ) {
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->word_idx = 0;
		parser->state = PP_NULL;
	} else if( isdigit( (unsigned char) c )  // Found a number
				|| '-' == c
				|| '-' == c
				|| '+' == c
				|| '.' == c
				|| 'e' == c
				|| 'E' == c ) {
		osrf_buffer_reset( parser->buf );
		osrf_buffer_add_char( parser->buf, c );
		push_pp_state( parser, PP_OBJ_VALUE );
		parser->state = PP_NUM;
	} else {
		report_pp_error( parser,
			"Unexpected character \'%c\' after colon within JSON object", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for a comma in a JSON object, or for the end of the object.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just finished a key/value entry in a JSON object.  We expect to see either a comma
	or a right curly brace.  Anything else is an error.
*/
static int do_obj_value( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( ',' == c ) {
		parser->state = PP_OBJ_COMMA;
	} else if( '}' == c ) {
		if( parser->handlers.handleEndObj )
			rc = parser->handlers.handleEndObj( parser->blob );
		pop_pp_state( parser );
		check_pp_end( parser );
	} else {
		report_pp_error( parser, "Expected comma or '}' within JSON object; found \'%c\'", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Look for the next entry in a JSON object.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	We have just found a separator comma within a JSON object.  We expect to find a string to
	serve as the key for the next entry.  Anything else is an error.
*/
static int do_obj_comma( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else if( '\"' == c ) {    // Found a string
		osrf_buffer_reset( parser->buf );
		push_pp_state( parser, PP_OBJ_KEY );
		parser->state = PP_STR;
	} else {
		report_pp_error( parser, "Expected key string in a JSON object; found \'%c\'", c );
		rc = 1;
	}

	return rc;
}

/**
	@brief Accumulate characters of the keyword "true".
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	There are several ways to recognize keywords.  You can accumulate characters and then
	look at the whole thing; you can have a distinct parser state for each letter; etc..

	In this parser we have only three keywords to recognize, starting with three different
	letters; no other bare words are allowed.  When we see the opening "t" we expect to
	see "rue" following it, and similarly for "false" and "null".  We compare each letter
	to the letter we expect to see at that position, and complain if they don't match.
*/
static int do_true( JSONPushParser* parser, char c ) {
	int rc = 0;
	switch ( found_keyword( parser, c, "true", 4 ) ) {
		case -1 :
			rc = 1;       // wrong character found (already reported)
			break;
		case 0  :         // so far so good
			break;
		case 1  :         // we have all the right characters
			if( parser->handlers.handleBool )
				rc = parser->handlers.handleBool( parser->blob, 1 );
			parser->again = c;
			pop_pp_state( parser );
			check_pp_end( parser );
			break;
	}

	return rc;
}

/**
	@brief Accumulate characters of the keyword "false".
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	See the discussion of do_true().
*/
static int do_false( JSONPushParser* parser, char c ) {
	int rc = 0;
	switch ( found_keyword( parser, c, "false", 5 ) ) {
		case -1 :
			rc = 1;       // wrong character found (already reported)
			break;
		case 0  :         // so far so good
			break;
		case 1  :         // we have all the right characters
			if( parser->handlers.handleBool )
				rc = parser->handlers.handleBool( parser->blob, 0 );
			parser->again = c;
			pop_pp_state( parser );
			check_pp_end( parser );
			break;
	}

	return rc;
}

/**
	@brief Accumulate characters of the keyword "null".
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

	See the discussion of do_true().
*/
static int do_null( JSONPushParser* parser, char c ) {
	int rc = 0;
	switch ( found_keyword( parser, c, "null", 4 ) ) {
		case -1 :
			rc = 1;       // wrong character found (already reported)
			break;
		case 0  :         // so far so good
			break;
		case 1  :         // we have all the right characters
			if( parser->handlers.handleNull )
				rc = parser->handlers.handleNull( parser->blob );
			parser->again = c;         // Revisit this character next time around
			pop_pp_state( parser );
			check_pp_end( parser );
			break;
	}

	return rc;
}

/**
	@brief Accumulate a character for a specified keyword
	@param parser Pointer to the current JSONPushParser
	@param c The current input character
	@param keyword The keyword we're looking for
	@param maxlen The length of the keyword (obviating strlen())
	@return 0 If @a c is the correct next letter in the keyword,
	or 1 if the keyword is finished correctly, or -1 upon error.

	Accumulate successive letters in a specified keyword.  We don't actually store the
	letters anywhere; we just check to make sure they're the letters we expect.
*/
static int found_keyword( JSONPushParser* parser, char c,
		const char* keyword, unsigned maxlen ) {
	int rc = 0;
	if( ++parser->word_idx >= maxlen ) {
		// We have all the characters; now check the one following.  It had better be
		// either white space or punctuation.
		if( !isspace( (unsigned char) c ) && !ispunct( (unsigned char) c ) ) {
			report_pp_error( parser, "Unexpected character '%c' after \"true\" keyword", c );
			return -1;     // bad character at end of keyword -- e.g. "trueY"
		} else
			return 1;
	} else if( keyword[ parser->word_idx ] == c ) {
		;        // so far so good
	} else {
		report_pp_error( parser, "Expected '%c' in keyword \"%s\"; found '%c'\n",
			keyword[ parser->word_idx ], keyword, c );
		rc = -1;
	}
	return rc;
}

/**
	@brief We have reached the end of the JSON string.  There should be nothing but white space.
	@param parser Pointer to the current JSONPushParser.
	@param c The current input character.
	@return 0 if successful, or 1 upon error.

*/
static int do_end( JSONPushParser* parser, char c ) {
	int rc = 0;
	if( isspace( (unsigned char) c ) )   // skip white space
		;
	else {
		report_pp_error( parser,
			"Expected nothing but white space afer a JSON string; found \'%c\'", c );
		rc = 1;
	}

	return rc;
}

// -------- End of state handlers --------------------------

/**
	@brief Push the current parser state onto a stack.
	@param parser Pointer to the current JSONPushParser.
	@param state The state to which we will return when we pop it off.

	We use a stack to simulate recursive descent.  At every point where a recursive descent
	parser would descend, we push the a state onto the stack, i.e. the state we want to
	go when we come back.  Where a recursive descent parser would return from the descent,
	we pop the previously stored state off the stack.

	Note that the state we push is not the current state, but some other state.  We simulate
	a descent in order to parse some JSON value, and after parsing it, we need to be in some
	other state.  So we push that future state onto the stack in advance.
*/
static void push_pp_state( JSONPushParser* parser, PPState state ) {
	// Allocate a StateNode -- from the free list if possible,
	// Or from the heap if necessary.
	StateNode* node;
	if( parser->free_states ) {
		node = parser->free_states;
		parser->free_states = node->next;
	} else {
		node = safe_malloc( sizeof( StateNode ) );
		node->keylist = osrfNewStringArray( 8 );
	}

	// Now popuate it, and push it onto the stack.
	node->state = state;
	osrfStringArraySwap( parser->keylist, node->keylist );
	node->next = parser->state_stack;
	parser->state_stack = node;
}

/**
	@brief Restore the previous state of the parser.
	@param parser Pointer to the current JSONPushParser.

	See also push_pp_state().
*/
static void pop_pp_state( JSONPushParser* parser ) {
	if( ! parser->state_stack ) {
		parser->state = PP_END;    // shouldn't happen
	} else {
		StateNode* node = parser->state_stack;
		parser->state_stack = node->next;
		node->next = parser->free_states;
		parser->free_states = node;
		// Transfer the contents of the popped node to the parser
		parser->state = node->state;
		osrfStringArraySwap( parser->keylist, node->keylist );
		osrfStringArrayClear( node->keylist );
	}
}

static void check_pp_end( JSONPushParser* parser ) {
	if( PP_END == parser->state && parser->handlers.handleEndJSON )
		parser->handlers.handleEndJSON( parser->blob );
}

/**
	@brief Issue an error message from the parser.
	@param parser Pointer to the parser issuing the message
	@param msg A printf-style format string.  Subsequent parameters, if any, will be
	expanded and inserted into the output message.
*/
static void report_pp_error( JSONPushParser* parser, const char* msg, ... ) {
	VA_LIST_TO_STRING( msg );
	if( parser->handlers.handleError )
		parser->handlers.handleError( parser->blob, VA_BUF, parser->line, parser->pos );
	else
		osrfLogError( OSRF_LOG_MARK, "JSON Error at line %u, position %u: %s",
			parser->line, parser->pos, VA_BUF );
}

/**
	@brief Free a JSONPushParser and everything it owns.
	@param parser Pointer to the JSONPustParser to be freed.
*/
void jsonPushParserFree( JSONPushParser* parser ) {
	if( parser ) {
		osrf_buffer_free( parser->buf );

		// Pop off all the StateNodes, and then free them
		while( parser->state_stack ) {
			pop_pp_state( parser );
		}

		while( parser->free_states ) {
			StateNode* temp = parser->free_states->next;
			osrfStringArrayFree( parser->free_states->keylist );
			free( parser->free_states  );
			parser->free_states = temp;
		}
		osrfStringArrayFree( parser->keylist );
		free( parser );
	}
}
