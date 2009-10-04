/*
Copyright (C) 2009  Georgia Public Library Service
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
	@file osrf_parse_json.c
	@brief  Recursive descent parser for JSON.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <opensrf/osrf_json.h>
#include <opensrf/osrf_json_utils.h>

/**
	@brief A collection of things the parser uses to keep track of what it's doing.
*/
typedef struct {
	growing_buffer* str_buf;  /**< for building strings */
	size_t index;             /**< index into input buffer */
	const char* buff;         /**< client's buffer holding current chunk of input */
	int decode;               /**< boolean; true if we are decoding class hints */
} Parser;

/**
	@brief A small buffer for building Unicode byte sequences.

	Because we pass a Unibuff* instead of a bare char*, the receiving function doesn't
	have to worry about the size of the supplied buffer.  The size is known.
*/
typedef struct {
	/** @brief A small working buffer.

		We fill the buffer with four hex characters, and then transform them into a byte
		sequence up to three bytes long (plus terminal nul) encoding a UTF-8 character.
	*/
	unsigned char buff[ 4 ];
} Unibuff;

static jsonObject* parse_it( const char* s, int decode );

static jsonObject* get_json_node( Parser* parser, char firstc );
static const char* get_string( Parser* parser );
static jsonObject* get_number( Parser* parser, char firstc );
static jsonObject* get_array( Parser* parser );
static jsonObject* get_hash( Parser* parser );
static jsonObject* get_decoded_hash( Parser* parser );
static jsonObject* get_null( Parser* parser );
static jsonObject* get_true( Parser* parser );
static jsonObject* get_false( Parser* parser );
static int get_utf8( Parser* parser, Unibuff* unibuff );

static char skip_white_space( Parser* parser );
static inline void parser_ungetc( Parser* parser );
static inline char parser_nextc( Parser* parser );
static void report_error( Parser* parser, char badchar, const char* err );

/* ------------------------------------- */

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
jsonObject* jsonParse( const char* str ) {
	return parse_it( str, 1 );
}

/**
	@brief Parse a JSON string, with no decoding of classname hints.
	@param s Pointer to the JSON string to parse.
	@return A pointer to the resulting JSON object, or NULL on error.

	This function is similar to jsonParse(), except that it does not give any special
	treatment to a JSON_HASH with the JSON_CLASS_KEY tag.

	The calling code is responsible for freeing the resulting jsonObject.
*/
jsonObject* jsonParseRaw( const char* s ) {
	return parse_it( s, 0 );
}

/**
	@brief Parse a JSON string received as a printf-style format string.
	@param str A printf-style format string.  Subsequent arguments, if any, are formatted
		and inserted into the JSON string before parsing.
	@return A pointer to the resulting JSON object, or NULL on error.

	Unlike jsonParse(), this function does not give any special treatment to a JSON_HASH
	with tags JSON_CLASS_KEY or JSON_DATA_KEY.

	The calling code is responsible for freeing the resulting jsonObject.
*/
jsonObject* jsonParseFmt( const char* str, ... ) {
	if( !str )
		return NULL;
	VA_LIST_TO_STRING( str );
	return parse_it( VA_BUF, 0 );
}

/**
	@brief Parse a JSON string into a jsonObject.
	@param s Pointer to the string to be parsed.
	@param decode A boolean; true means decode class hints, false means don't.
	@return Pointer to the newly created jsonObject.

	Set up a Parser.  Call get_json_node() to do the real work, then make sure that there's
	nothing but white space at the end.
*/
static jsonObject* parse_it( const char* s, int decode ) {

	if( !s || !*s )
		return NULL;    // Nothing to parse

	Parser parser;

	parser.str_buf = NULL;
	parser.index = 0;
	parser.buff = s;
	parser.decode = decode;

	jsonObject* obj = get_json_node( &parser, skip_white_space( &parser ) );

	// Make sure there's nothing but white space at the end
	char c;
	if( obj && (c = skip_white_space( &parser )) ) {
		report_error( &parser, c, "Extra material follows JSON string" );
		jsonObjectFree( obj );
		obj = NULL;
	}

	buffer_free( parser.str_buf );
	return obj;
}

/**
	@brief Get the next JSON node -- be it string, number, hash, or whatever.
	@param parser Pointer to a Parser.
	@param firstc The first character in the part that we're parsing.
	@return Pointer to the next JSON node, or NULL upon error.

	The first character tells us what kind of thing we're parsing next: a string, an array,
	a hash, a number, a boolean, or a null.  Branch accordingly.

	In the case of an array or a hash, this function indirectly calls itself in order to
	parse subordinate nodes.
*/
static jsonObject* get_json_node( Parser* parser, char firstc ) {

	jsonObject* obj = NULL;

	// Branch on the first character
	if( '"' == firstc ) {
		const char* str = get_string( parser );
		if( str ) {
			obj = jsonNewObject( NULL );
			obj->type = JSON_STRING;
			obj->value.s = strdup( str );
		}
	} else if( '[' == firstc ) {
		obj = get_array( parser );
	} else if( '{' == firstc ) {
		if( parser->decode )
			obj = get_decoded_hash( parser );
		else
			obj = get_hash( parser );
	} else if( 'n' == firstc ) {
		obj = get_null( parser );
	} else if( 't' == firstc ) {
		obj = get_true( parser );
	} else if( 'f' == firstc ) {
		obj = get_false( parser );
	}
	else if( isdigit( (unsigned char) firstc ) ||
			 '.' == firstc ||
			 '-' == firstc ||
			 '+' == firstc ||
			 'e' == firstc ||
			 'E' == firstc ) {
		obj = get_number( parser, firstc );
	} else {
		report_error( parser, firstc, "Unexpected character" );
	}

	return obj;
}

/**
	@brief Collect characters into a character string.
	@param parser Pointer to a Parser.
	@return Pointer to parser->str_buf if successful, or NULL upon error.

	Translate the usual escape sequences.  In particular, "\u" escapes a sequence of four
	hex characters; turn the hex into the corresponding UTF-8 byte sequence.

	Return the string we have built, without the enclosing quotation marks, in
	parser->str_buf.  In case of error, log an error message.
*/
static const char* get_string( Parser* parser ) {

	if( parser->str_buf )
		buffer_reset( parser->str_buf );
	else
		parser->str_buf = buffer_init( 64 );

	growing_buffer* gb = parser->str_buf;

	// Collect the characters.
	for( ;; ) {
		char c = parser_nextc( parser );
		if( '"' == c )
			break;
		else if( !c ) {
			report_error( parser, parser->buff[ parser->index - 1  ],
						  "Quoted string not terminated" );
			return NULL;
		} else if( '\\' == c ) {
			c = parser_nextc( parser );
			switch( c ) {
				case '"'  : OSRF_BUFFER_ADD_CHAR( gb, '"'  ); break;
				case '\\' : OSRF_BUFFER_ADD_CHAR( gb, '\\' ); break;
				case '/'  : OSRF_BUFFER_ADD_CHAR( gb, '/'  ); break;
				case 'b'  : OSRF_BUFFER_ADD_CHAR( gb, '\b' ); break;
				case 'f'  : OSRF_BUFFER_ADD_CHAR( gb, '\f' ); break;
				case 'n'  : OSRF_BUFFER_ADD_CHAR( gb, '\n' ); break;
				case 'r'  : OSRF_BUFFER_ADD_CHAR( gb, '\r' ); break;
				case 't'  : OSRF_BUFFER_ADD_CHAR( gb, '\t' ); break;
				case 'u'  : {
					Unibuff unibuff;
					if( get_utf8( parser, &unibuff ) ) {
						return NULL;       // bad UTF-8
					} else if( unibuff.buff[0] ) {
						OSRF_BUFFER_ADD( gb, (char*) unibuff.buff );
					} else {
						report_error( parser, 'u', "Unicode sequence encodes a nul byte" );
						return NULL;
					}
					break;
				}
				default   : OSRF_BUFFER_ADD_CHAR( gb, c );    break;
			}
		}
		else
			OSRF_BUFFER_ADD_CHAR( gb, c );
	}

	return OSRF_BUFFER_C_STR( gb );
}

/**
	@brief Collect characters into a number, and create a JSON_NUMBER for it.
	@param parser Pointer to a parser.
	@param firstc The first character in the number.
	@return Pointer to a newly created jsonObject of type JSON_NUMBER, or NULL upon error.

	Collect digits, signs, decimal points, and 'E' or 'e' (for scientific notation) into
	a buffer.  Make sure that the result is numeric.  If it's not numeric by strict JSON
	rules, try to make it numeric by some judicious massaging (we aren't quite as strict
	as the official JSON rules).

	If successful, construct a jsonObject of type JSON_NUMBER containing the resulting
	numeric string.  Otherwise log an error message and return NULL.
*/
static jsonObject* get_number( Parser* parser, char firstc ) {

	if( parser->str_buf )
		buffer_reset( parser->str_buf );
	else
		parser->str_buf = buffer_init( 64 );

	growing_buffer* gb = parser->str_buf;
	OSRF_BUFFER_ADD_CHAR( gb, firstc );

	char c;

	for( ;; ) {
		c = parser_nextc( parser );
		if( isdigit( (unsigned char) c ) ||
			'.' == c ||
			'-' == c ||
			'+' == c ||
			'e' == c ||
			'E' == c ) {
			OSRF_BUFFER_ADD_CHAR( gb, c );
		} else {
			if( ! isspace( (unsigned char) c ) )
				parser_ungetc( parser );
			break;
		}
	}

	char* s = buffer_data( gb );
	if( ! jsonIsNumeric( s ) ) {
		char* temp = jsonScrubNumber( s );
		free( s );
		s = temp;
		if( !s ) {
			report_error( parser, parser->buff[ parser->index - 1 ],
					"Invalid numeric format" );
			return NULL;
		}
	}

	jsonObject* obj = jsonNewObject( NULL );
	obj->type = JSON_NUMBER;
	obj->value.s = s;

	return obj;
}

/**
	@brief Parse an array, and create a JSON_ARRAY for it.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject of type JSON_ARRAY, or NULL upon error.

	Look for a series of JSON nodes, separated by commas and terminated by a right square
	bracket.  Parse each node recursively, collect them all into a newly created jsonObject
	of type JSON_ARRAY, and return a pointer to the result.

	Upon error, log an error message and return NULL.
*/
static jsonObject* get_array( Parser* parser ) {

	jsonObject* array = jsonNewObjectType( JSON_ARRAY );

	char c = skip_white_space( parser );
	if( ']' == c )
		return array;          // Empty array

	for( ;; ) {
		jsonObject* obj = get_json_node( parser, c );
		if( !obj ) {
			jsonObjectFree( array );
			return NULL;         // Failed to get anything
		}

		// Add the entry to the array
		jsonObjectPush( array, obj );

		// Look for a comma or right bracket
		c = skip_white_space( parser );
		if( ']' == c )
			break;
		else if( c != ',' ) {
			report_error( parser, c, "Expected comma or bracket in array; didn't find it\n" );
			jsonObjectFree( array );
			return NULL;
		}
		c = skip_white_space( parser );
	}

	return array;
}

/**
	@brief Parse a hash (JSON object), and create a JSON_HASH for it.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject of type JSON_HASH, or NULL upon error.

	Look for a series of name/value pairs, separated by commas and terminated by a right
	curly brace.  Each name/value pair consists of a quoted string, followed by a colon,
	followed a JSON node of any sort.  Parse the value recursively.

	Collect the name/value pairs into a newly created jsonObject of type JSON_ARRAY, and
	return a pointer to it.

	Upon error, log an error message and return NULL.
*/
static jsonObject* get_hash( Parser* parser ) {
	jsonObject* hash = jsonNewObjectType( JSON_HASH );

	char c = skip_white_space( parser );
	if( '}' == c )
		return hash;           // Empty hash

	for( ;; ) {

		// Get the key string
		if( '"' != c ) {
			report_error( parser, c,
						  "Expected quotation mark to begin hash key; didn't find it\n" );
			jsonObjectFree( hash );
			return NULL;
		}

		const char* key = get_string( parser );
		if( ! key ) {
			jsonObjectFree( hash );
			return NULL;
		}
		char* key_copy = strdup( key );

		if( jsonObjectGetKey( hash, key_copy ) ) {
			report_error( parser, '"', "Duplicate key in JSON object" );
			jsonObjectFree( hash );
			return NULL;
		}

		// Get the colon
		c = skip_white_space( parser );
		if( c != ':' ) {
			report_error( parser, c,
						  "Expected colon after hash key; didn't find it\n" );
			free( key_copy );
			jsonObjectFree( hash );
			return NULL;
		}

		// Get the associated value
		jsonObject* obj = get_json_node( parser, skip_white_space( parser ) );
		if( !obj ) {
			free( key_copy );
			jsonObjectFree( hash );
			return NULL;
		}

		// Add a new entry to the hash
		jsonObjectSetKey( hash, key_copy, obj );
		free( key_copy );

		// Look for comma or right brace
		c = skip_white_space( parser );
		if( '}' == c )
			break;
		else if( c != ',' ) {
			report_error( parser, c,
						  "Expected comma or brace in hash, didn't find it" );
			jsonObjectFree( hash );
			return NULL;
		}
		c = skip_white_space( parser );
	}

	return hash;
}

/**
	@brief Parse a hash (JSON object), and create a JSON_HASH for it; decode class hints.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject, or NULL upon error.

	This function is similar to get_hash(), @em except:

	If the hash includes a member with a key equal to JSON_CLASS_KEY ("__c" by default),
	then look for a member whose key is JSON_DATA_KEY ("__p" by default).  If you find one,
	return the data associated with it; otherwise return a jsonObject of type JSON_NULL.

	If there is no member with a key equal to JSON_CLASS_KEY, then return the same sort of
	jsonObject as get_hash() would return (except of course that lower levels may be
	decoded as described above).
*/
static jsonObject* get_decoded_hash( Parser* parser ) {
	jsonObject* hash = jsonNewObjectType( JSON_HASH );

	char c = skip_white_space( parser );
	if( '}' == c )
		return hash;           // Empty hash

	char* class_name = NULL;

	for( ;; ) {

		// Get the key string
		if( '"' != c ) {
			report_error( parser, c,
					"Expected quotation mark to begin hash key; didn't find it\n" );
			jsonObjectFree( hash );
			return NULL;
		}

		const char* key = get_string( parser );
		if( ! key ) {
			jsonObjectFree( hash );
			return NULL;
		}
		char* key_copy = strdup( key );

		if( jsonObjectGetKey( hash, key_copy ) ) {
			report_error( parser, '"', "Duplicate key in JSON object" );
			jsonObjectFree( hash );
			return NULL;
		}

		// Get the colon
		c = skip_white_space( parser );
		if( c != ':' ) {
			report_error( parser, c,
					"Expected colon after hash key; didn't find it\n" );
			free( key_copy );
			jsonObjectFree( hash );
			return NULL;
		}

		// Get the associated value
		jsonObject* obj = get_json_node( parser, skip_white_space( parser ) );
		if( !obj ) {
			free( key_copy );
			jsonObjectFree( hash );
			return NULL;
		}

		// Add a new entry to the hash
		jsonObjectSetKey( hash, key_copy, obj );

		// Save info for class hint, if present
		if( !strcmp( key_copy, JSON_CLASS_KEY ) )
			class_name = jsonObjectToSimpleString( obj );

		free( key_copy );

		// Look for comma or right brace
		c = skip_white_space( parser );
		if( '}' == c )
			break;
		else if( c != ',' ) {
			report_error( parser, c,
					"Expected comma or brace in hash, didn't find it" );
			jsonObjectFree( hash );
			return NULL;
		}
		c = skip_white_space( parser );
	}

	if( class_name ) {
		// We found a class hint.  Extract the data node and return it.
		jsonObject* class_data = osrfHashExtract( hash->value.h, JSON_DATA_KEY );
		if( class_data ) {
			class_data->parent = NULL;
			jsonObjectFree( hash );
			hash = class_data;
			hash->parent = NULL;
			hash->classname = class_name;
		} else {
			// Huh?  We have a class name but no data for it.
			// Throw away what we have and return a JSON_NULL.
			jsonObjectFree( hash );
			hash = jsonNewObjectType( JSON_NULL );
		}

	} else {
		if( class_name )
			free( class_name );
	}

	return hash;
}

/**
	@brief Parse the JSON keyword "null", and create a JSON_NULL for it.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject of type JSON_NULL, or NULL upon error.

	We already saw an 'n', or we wouldn't be here.  Make sure that the next three characters
	are 'u', 'l', and 'l', and that the character after that is not a letter or a digit.

	If all goes well, create a jsonObject of type JSON_NULL, and return a pointer to it.
	Otherwise log an error message and return NULL.
*/
static jsonObject* get_null( Parser* parser ) {

	if( parser_nextc( parser ) != 'u' ||
		parser_nextc( parser ) != 'l' ||
		parser_nextc( parser ) != 'l' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
				"Expected \"ull\" to follow \"n\"; didn't find it" );
		return NULL;
	}

	// Peek at the next character to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c, "Found letter or number after \"null\"" );
		return NULL;
	}

	// Everything's okay.  Return a JSON_NULL.
	return jsonNewObject( NULL );
}

/**
	@brief Parse the JSON keyword "true", and create a JSON_BOOL for it.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject of type JSON_BOOL, or NULL upon error.

	We already saw a 't', or we wouldn't be here.  Make sure that the next three characters
	are 'r', 'u', and 'e', and that the character after that is not a letter or a digit.

	If all goes well, create a jsonObject of type JSON_BOOL, and return a pointer to it.
	Otherwise log an error message and return NULL.
*/
static jsonObject* get_true( Parser* parser ) {

	if( parser_nextc( parser ) != 'r' ||
		parser_nextc( parser ) != 'u' ||
		parser_nextc( parser ) != 'e' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
					  "Expected \"rue\" to follow \"t\"; didn't find it" );
		return NULL;
	}

	// Peek at the next character to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c, "Found letter or number after \"true\"" );
		return NULL;
	}

	// Everything's okay.  Return a JSON_BOOL.
	return jsonNewBoolObject( 1 );
}

/**
	@brief Parse the JSON keyword "false", and create a JSON_BOOL for it.
	@param parser Pointer to a Parser.
	@return Pointer to a newly created jsonObject of type JSON_BOOL, or NULL upon error.

	We already saw a 'f', or we wouldn't be here.  Make sure that the next four characters
	are 'a', 'l', 's', and 'e', and that the character after that is not a letter or a digit.

	If all goes well, create a jsonObject of type JSON_BOOL, and return a pointer to it.
	Otherwise log an error message and return NULL.
*/
static jsonObject* get_false( Parser* parser ) {

	if( parser_nextc( parser ) != 'a' ||
		parser_nextc( parser ) != 'l' ||
		parser_nextc( parser ) != 's' ||
		parser_nextc( parser ) != 'e' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
				"Expected \"alse\" to follow \"f\"; didn't find it" );
		return NULL;
	}

	// Peek at the next character to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c, "Found letter or number after \"false\"" );
		return NULL;
	}

	// Everything's okay.  Return a JSON_BOOL.
	return jsonNewBoolObject( 0 );
}

/**
	@brief Convert a hex digit to the corresponding numeric value.
	@param x A hex digit
	@return The corresponding numeric value.

	Warning #1: The calling code must ensure that the character to be converted is, in fact,
	a hex character.  Otherwise the results will be strange.

	Warning #2. This macro evaluates its argument three times.  Beware of side effects.
	(It might make sense to convert this macro to a static inline function.)

	Warning #3: This code assumes that the characters [a-f] and [A-F] are contiguous in the
	execution character set, and that the lower 4 bits for 'a' and 'A' are 0001.  Those
	assumptions are true for ASCII and EBCDIC, but there may be some character sets for
	which it is not true.
*/
#define hexdigit(x) ( ((x) <= '9') ? (x) - '0' : ((x) & 7) + 9)

/**
	@brief Translate the next four characters into a UTF-8 character.
	@param parser Pointer to a Parser.
	@param unibuff Pointer to a small buffer in which to return the results.
	@return 0 if successful, or 1 if not.

	Collect the next four characters into @a unibuff, and make sure that they're all hex.
	Translate them into a nul-terminated UTF-8 byte sequence, and return the result via
	@a unibuff.

	(Note that a UTF-8 byte sequence is guaranteed not to contain a nul byte.  Hence using
	a nul as a terminator creates no ambiguity.)
*/
static int get_utf8( Parser* parser, Unibuff* unibuff ) {
	char ubuff[ 5 ];
	int i = 0;

	// Accumulate four characters into a buffer.  Make sure that
	// there are four of them, and that they're all hex.
	for( i = 0; i < 4; ++i ) {
		int c = parser_nextc( parser );
		if( !c ) {
			report_error( parser, 'u', "Incomplete Unicode sequence" );
			unibuff->buff[ 0 ] = '\0';
			return 1;
		} else if( ! isxdigit( (unsigned char) c ) ) {
			report_error( parser, c, "Non-hex byte found in Unicode sequence" );
			unibuff->buff[ 0 ] = '\0';
			return 1;
		}
		else
			ubuff[ i ] = c;
	}

	/* The following code is adapted with permission from
	 * json-c http://oss.metaparadigm.com/json-c/
	 */

	// Convert the hex sequence to a single integer
	unsigned int ucs_char =
			(hexdigit(ubuff[ 0 ]) << 12) +
			(hexdigit(ubuff[ 1 ]) <<  8) +
			(hexdigit(ubuff[ 2 ]) <<  4) +
			 hexdigit(ubuff[ 3 ]);

	unsigned char* utf_out = unibuff->buff;

	if (ucs_char < 0x80) {
		utf_out[0] = ucs_char;
		utf_out[1] = '\0';

	} else if (ucs_char < 0x800) {
		utf_out[0] = 0xc0 | (ucs_char >> 6);
		utf_out[1] = 0x80 | (ucs_char & 0x3f);
		utf_out[2] = '\0';

	} else {
		utf_out[0] = 0xe0 | (ucs_char >> 12);
		utf_out[1] = 0x80 | ((ucs_char >> 6) & 0x3f);
		utf_out[2] = 0x80 | (ucs_char & 0x3f);
		utf_out[3] = '\0';
	}

	return 0;
}

/**
	@brief Skip over white space.
	@param parser Pointer to a Parser.
	@return The next non-whitespace character.
*/
static char skip_white_space( Parser* parser ) {
	char c;
	do {
		c = parser_nextc( parser );
	} while( isspace( (unsigned char) c ) );

	return c;
}

/**
	@brief Back up by one character.
	@param parser Pointer to a Parser.

	Decrement an index into the input string.  We don't guard against a negative index, so
	the calling code should make sure that it doesn't do anything stupid.
*/
static inline void parser_ungetc( Parser* parser ) {
	--parser->index;
}

/**
	@brief Get the next character
	@param parser Pointer to a Parser.
	@return The next character.

	Increment an index into the input string and return the corresponding character.
	The calling code should make sure that it doesn't try to read past the terminal nul.
*/
static inline char parser_nextc( Parser* parser ) {
	return parser->buff[ parser->index++ ];
}

/**
	@brief Report a syntax error to the log.
	@param parser Pointer to a Parser.
	@param badchar The character at the position where the error was detected.
	@param err Pointer to a descriptive error message.

	Format and log an error message.  Identify the location of the error and
	the character at that location.  Show the neighborhood of the error within
	the input string.
*/
static void report_error( Parser* parser, char badchar, const char* err ) {

	// Determine the beginning and ending points of a JSON
	// fragment to display, from the vicinity of the error

	const int max_margin = 15;  // How many characters to show
	                            // on either side of the error
	int pre = parser->index - max_margin;
	if( pre < 0 )
		pre = 0;

	int post = parser->index + 15;
	if( '\0' == parser->buff[ parser->index ] ) {
		post = parser->index - 1;
	} else {
		int remaining = strlen(parser->buff + parser->index);
		if( remaining < max_margin )
			post = parser->index + remaining;
	}

	// Copy the fragment into a buffer
	int len = post - pre + 1;  // length of fragment
	char buf[len + 1];
	memcpy( buf, parser->buff + pre, len );
	buf[ len ] = '\0';

	// Replace newlines and tabs with spaces
	char* p = buf;
	while( *p ) {
		if( '\n' == *p || '\t' == *p )
			*p = ' ';
		++p;
	}

	// Avoid trying to display a nul character
	if( '\0' == badchar )
		badchar = ' ';

	// Issue the message
	osrfLogError( OSRF_LOG_MARK,
		"*JSON Parser Error\n - char  = %c\n "
		"- index = %d\n - near  => %s\n - %s",
		badchar, parser->index, buf, err );
}
