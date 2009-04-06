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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <opensrf/osrf_json.h>
#include <opensrf/osrf_json_utils.h>

typedef struct {
	growing_buffer* str_buf;  // for building strings
	size_t index;             // index into buffer
	const char* buff;         // client's buffer holding current chunk of input
} Parser;

// For building Unicode byte sequences
typedef struct {
	unsigned char buff[ 4 ];
} Unibuff;

static jsonObject* parse( Parser* parser );

static jsonObject* get_json_thing( Parser* parser, char firstc );
static const char* get_string( Parser* parser );
static jsonObject* get_number( Parser* parser, char firstc );
static jsonObject* get_array( Parser* parser );
static jsonObject* get_hash( Parser* parser );
static jsonObject* get_null( Parser* parser );
static jsonObject* get_true( Parser* parser );
static jsonObject* get_false( Parser* parser );
static int get_utf8( Parser* parser, Unibuff* unibuff );

static char skip_white_space( Parser* parser );
static inline void parser_ungetc( Parser* parser );
static inline char parser_nextc( Parser* parser );
static void report_error( Parser* parser, char badchar, char* err );

/* ------------------------------------- */

// Parse a JSON string; expand classes; construct a jsonObject.
// Return NULL if the JSON string is invalid.
jsonObject* jsonParse( const char* str ) {
	if(!str)
		return NULL;

	jsonObject* obj  = jsonParseRaw( str );

	jsonObject* obj2 = NULL;
	if( obj )
		obj2 = jsonObjectDecodeClass( obj );

	jsonObjectFree( obj  );

	return obj2;
}

// Parse a JSON string with variable arguments; construct a jsonObject.
// Return NULL if the resulting JSON string is invalid.
jsonObject* jsonParseFmt( const char* str, ... ) {
	if( !str )
		return NULL;
	VA_LIST_TO_STRING(str);
	return jsonParseRaw( VA_BUF );
}

// Parse a JSON string; construct a jsonObject.
// Return NULL if the JSON string is invalid.
jsonObject* jsonParseRaw( const char* s ) {

	if( !s || !*s )
		return NULL;    // Nothing to parse

	Parser parser;

	parser.str_buf = NULL;
	parser.index = 0;
	parser.buff = s;

	jsonObject* obj = parse( &parser );

	buffer_free( parser.str_buf );
	return obj;
}

// Parse a text string into a jsonObject.
static jsonObject* parse( Parser* parser ) {

	if( ! parser->buff ) {
		osrfLogError( OSRF_LOG_MARK, "Internal error; no input buffer available" );
		return NULL;         // Should never happen
	}

	jsonObject* obj = get_json_thing( parser, skip_white_space( parser ) );

	char c;
	if( obj && (c = skip_white_space( parser )) ) {
		report_error( parser, c, "Extra material follows JSON string" );
		jsonObjectFree( obj );
		obj = NULL;
	}

	return obj;
}

// Get the next JSON node -- be it string, number, hash, or whatever.
// Return a pointer to it if successful, or NULL if not.
static jsonObject* get_json_thing( Parser* parser, char firstc ) {

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

// Collect characters from the input stream into a character
// string, terminated by '"'.  Return a char* if successful,
// or NULL if not.
static const char* get_string( Parser* parser ) {

	if( parser->str_buf )
		buffer_reset( parser->str_buf );
	else
		parser->str_buf = buffer_init( 64 );

	growing_buffer* gb = parser->str_buf;

	// Collect the characters.
	// This is a naive implementation so far.
	// We need to worry about UTF-8.
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
				case '"'  : OSRF_BUFFER_ADD_CHAR( gb, '"'  );  break;
				case '\\' : OSRF_BUFFER_ADD_CHAR( gb, '\\' ); break;
				case '/'  : OSRF_BUFFER_ADD_CHAR( gb, '/'  );  break;
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

// We found what looks like the first character of a number.
// Collect all the eligible characters, and verify that they
// are numeric (possibly after some scrubbing).  Return a
// pointer to a JSON_NUMBER if successful, or NULL if not.
static jsonObject* get_number( Parser* parser, char firstc ) {

	growing_buffer* gb = buffer_init( 32 );
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

	char* s = buffer_release( gb );
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

// We found a '['.  Create a JSON_ARRAY with all its subordinates.
static jsonObject* get_array( Parser* parser ) {

	jsonObject* array = jsonNewObjectType( JSON_ARRAY );

	char c = skip_white_space( parser );
	if( ']' == c )
		return array;          // Empty array

	for( ;; ) {
		jsonObject* obj = get_json_thing( parser, c );
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

// We found '{' Get a JSON_HASH, with all its subordinates.
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
		jsonObject* obj = get_json_thing( parser, skip_white_space( parser ) );
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

// We found an 'n'.  Verify that the next four characters are "ull",
// and that there are no further characters in the token.
static jsonObject* get_null( Parser* parser ) {

	if( parser_nextc( parser ) != 'u' ||
		parser_nextc( parser ) != 'l' ||
		parser_nextc( parser ) != 'l' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
				"Expected \"ull\" to follow \"n\"; didn't find it" );
		return NULL;
	}

	// Sneak a peek at the next character
	// to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c,
				"Found letter or number after \"null\"" );
		return NULL;
	}

	// Everythings okay.  Return a JSON_BOOL.
	return jsonNewObject( NULL );
}

// We found a 't'.  Verify that the next four characters are "rue",
// and that there are no further characters in the token.
static jsonObject* get_true( Parser* parser ) {

	if( parser_nextc( parser ) != 'r' ||
		parser_nextc( parser ) != 'u' ||
		parser_nextc( parser ) != 'e' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
					  "Expected \"rue\" to follow \"t\"; didn't find it" );
		return NULL;
	}

	// Sneak a peek at the next character
	// to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c,
				"Found letter or number after \"true\"" );
		return NULL;
	}

	// Everythings okay.  Return a JSON_NULL.
	return jsonNewBoolObject( 1 );
}

// We found an 'f'.  Verify that the next four characters are "alse",
// and that there are no further characters in the token.
static jsonObject* get_false( Parser* parser ) {

	if( parser_nextc( parser ) != 'a' ||
		parser_nextc( parser ) != 'l' ||
		parser_nextc( parser ) != 's' ||
		parser_nextc( parser ) != 'e' ) {
		report_error( parser, parser->buff[ parser->index - 1 ],
				"Expected \"alse\" to follow \"f\"; didn't find it" );
		return NULL;
	}

	// Sneak a peek at the next character
	// to make sure that it's kosher
	char c = parser_nextc( parser );
	if( ! isspace( (unsigned char) c ) )
		parser_ungetc( parser );

	if( isalnum( (unsigned char) c ) ) {
		report_error( parser, c,
				"Found letter or number after \"false\"" );
		return NULL;
	}

	// Everythings okay.  Return a JSON_BOOL.
	return jsonNewBoolObject( 0 );
}

// We found \u.  Grab the next 4 characters, confirm that they are hex,
// and convert them to Unicode.
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
	#define hexdigit(x) ( ((x) <= '9') ? (x) - '0' : ((x) & 7) + 9)

	// Convert the hex sequence into a single integer
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

// Return the next non-whitespace character in the input stream.
static char skip_white_space( Parser* parser ) {
	char c;
	do {
		c = parser_nextc( parser );
	} while( isspace( (unsigned char) c ) );

	return c;
}

// Put a character back into the input stream.
// It is the responsibility of the caller not to back up
// past the beginning of the input string.
static inline void parser_ungetc( Parser* parser ) {
	--parser->index;
}

// Get the next character.  It is the responsibility of
//the caller not to read past the end of the input string.
static inline char parser_nextc( Parser* parser ) {
	return parser->buff[ parser->index++ ];
}

// Report a syntax error to standard error.
static void report_error( Parser* parser, char badchar, char* err ) {

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
