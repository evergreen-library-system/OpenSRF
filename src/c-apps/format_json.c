/*
Copyright (C) 2009  Equinox Software Inc.
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
	@file format_json.c
	@brief Pretty-print JSON.

	Read JSON from a file and output it to standard output with consistent indentation
	and white space.

	Synopsis:

		format_json  [ filename [ ... ] ]

	Each command-line argument is the name of a file that format_json will read in turn
	and format as JSON.  A single hyphen denotes standard input.  If no file is specified,
	format_json reads standard input.

	The input file[s] may contain multiple JSON values, but a JSON value may not span more
	than a single file.  In the output, successive JSON values are separated by blank lines.

	The primary purpose of this formatter is to translate JSON into a canonical format that
	can be easily read and parsed by, for example, a perl script, without having to create
	a full JSON parser.  For that reason, every square bracket and curly brace is put on a
	line by itself, although it might be more aesthetically pleasing to put it at the end of
	the same line as whatever precedes it.

	A secondary purpose is to make ugly, all-run-together JSON more readable to the human eye.

	Finally, this program serves as an example of how to use the stream parser, especially
	for files that are too big to be loaded into memory at once.  To that end, the internal
	logic is extensively commented.

	Implementation details:

	When using a stream parser it is almost always necessary to implement a finite state
	automaton, and this formatter is no exception.

	We define a collection of callback functions for the parser to call at various points,
	We also set up a structure (called a Formatter) for the parser to pass back to the
	callbacks via a void pointer.  The Formatter supplies information about where we are and
	what we're doing; in particular, it includes the state variable for our finite state
	automaton.

	The parser is also a finite state automaton internally, and it also needs a struct (called
	a JSONPushParser) to keep track of where it is and what it's doing.  As a result, we have
	two finite state automatons passing control back and forth.  The parser handles syntax and
	the Formatter handles semantics.

	With a couple of exceptions, each callback returns a status code back to the parser that
	calls it: 0 for success and non-zero for error.  For example, a numeric literal might be
	out of range, or an object key might be misspelled or out of place, or we might encounter
	an object when we expect an array.  Those rules reflect the semantics of the particular
	kind of JSON that we're trying to parse.  If a callback returns non-zero, the parser stops.

	In the case of this formatter, any JSON is okay as long as the syntax is valid, and the
	parser takes care of the syntax.  Hence the callback functions routinely return zero.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "opensrf/utils.h"
#include "opensrf/osrf_utf8.h"
#include "opensrf/jsonpush.h"

/**
	@brief Enumeration of states for a finite state automaton.
*/
typedef enum {
	CTX_OPEN,           /**< Not currently within a JSON value. */
	CTX_ARRAY_BEGIN,    /**< At the beginning of a JSON array. */
	CTX_ARRAY,          /**< In a JSON array with at least one value so far. */
	CTX_OBJ_BEGIN,      /**< At the beginning of a JSON object. */
	CTX_OBJ_KEY,        /**< Between a key and its value in a JSON object. */
	CTX_OBJ             /**< In a JSON object with at least one entry so far. */
} Context;

/**
	@brief Node for storing a Context in a stack.
*/
struct ContextNode {
	struct ContextNode* next;      /**< Linkage pointer for linked list. */
	Context context;               /**< The Context being stored for eventual restoration. */
};
typedef struct ContextNode ContextNode;

/**
	@brief Structure to be passed back to callback functions to keep track of where we are.
*/
typedef struct {
	const char* filename;         /**< Name of input file, or NULL for standard input */
	Context context;              /**< Current state. */
	ContextNode* context_stack;   /**< Stack of previous states. */
	int indent;                   /**< How many current levels of indentation. */
	growing_buffer* buf;          /**< For formatting strings with escaped characters. */
	JSONPushParser* parser;       /**< Points to the current parser. */
} Formatter;

static int format_file( Formatter* formatter, FILE* infile );
static void install_parser( Formatter* formatter );

static void indent( unsigned n );
static int formatString( void* blob, const char* str );
static int formatNumber( void* blob, const char* str );
static int formatLeftBracket( void* blob );
static int formatRightBracket( void* blob );
static int formatKey( void* blob, const char* str );
static int formatLeftBrace( void* blob );
static int formatRightBrace( void* blob );
static int formatBool( void* blob, int b );
static int formatNull( void* blob );
static void formatEnd( void* blob );

static void show_error( void* blob, const char* msg, unsigned line, unsigned pos );

static void push_context( Formatter* formatter );
static void pop_context( Formatter* formatter );

static ContextNode* free_context = NULL;    // Free list for ContextNodes

/**
	@brief The usual.
	@param argc Number of command line parameters, plus one.
	@param argv Pointer to ragged array representing the command line.
	@return EXIT_SUCCESS on success, or EXIT_FAILURE upon failure.
*/
int main( int argc, char* argv[] ) {

	int rc = EXIT_SUCCESS;

	// Declare and initialize a Formatter
	static Formatter formatter;
	formatter.filename = NULL;
	formatter.context = CTX_OPEN;
	formatter.context_stack = NULL;
	formatter.indent = 0;
	formatter.buf = buffer_init( 32 );
	install_parser( &formatter );

	if( argc > 1 ) {
		int i = 0;
		while( (++i < argc) && (0 == rc) ) {
			// Iterate over the command line arguments.
			// An argument "-" means to read standard input.
			const char* filename = argv[ i ];
			FILE* in;
			if( '-' == filename[ 0 ] && '\0' == filename[ 1 ] ) {
				in = stdin;
				formatter.filename = NULL;
			} else {
				in = fopen( filename, "r" );
				formatter.filename = filename;
			}

			if( !in ) {
				fprintf( stderr, "Unable to open %s\n", filename );
			} else {
				// Reset the parser.  This tells the parser that we're starting over for a new
				// JSON value, and that it needs to reset the line counter and position counter
				// for error messages.  (We don't really need this for the first file, but it
				// does no harm.)
				jsonPushParserReset( formatter.parser );

				// Format the file
				if( format_file( &formatter, in ) )
					rc = EXIT_FAILURE;
				if( formatter.filename )
					fclose( in );
			}
		} // end while
	} else {
		// No command line arguments?  Read standard input.  Note that we don't have to
		// reset the parser in this case, because we're only parsing once anyway.
		format_file( &formatter, stdin );
	}

	// Clean up the formatter
	jsonPushParserFree( formatter.parser );
	buffer_free( formatter.buf );
	while( formatter.context_stack )
		pop_context( &formatter );

	// Free the free ContextNodes shed from the stack
	while( free_context ) {
		ContextNode* temp = free_context->next;
		free( free_context );
		free_context = temp;
	}

	return rc;
}

/**
	@brief Read and format a JSON file.
	@param formatter Pointer to the current Formatter.
	@param infile Pointer to the input file.
	@return 0 if successful, or 1 upon error.
*/
static int format_file( Formatter* formatter, FILE* infile ) {

	const int bufsize = 4096;
	char buf[ bufsize ];
	int num_read;
	int rc = 0;

	do {
		num_read = fread( buf, 1, bufsize, infile );
		if( num_read > 0 )
			if( jsonPush( formatter->parser, buf, num_read ) )
				rc = 1;
	} while( num_read == bufsize && 0 == rc );

	if( jsonPushParserFinish( formatter->parser ) )
		rc = 1;

	if( rc )
		fprintf( stderr, "\nError found in JSON file\n" );

	return rc;
}

/**
	@brief Create a JSONPushParser and install it in a Formatter.
	@param formatter Pointer to the Formatter in which the parser is to be installed.

	First we create a JSONHandlerMap to tell the parser what callback functions to call
	at various points.  Then we pass it to jsonNewPushParser, which makes its own copy of
	the map, so it's okay for our original map to go out of scope.
*/
static void install_parser( Formatter* formatter ) {

	// Designate the callback functions to be installed in the parser.
	JSONHandlerMap map = {
		formatString,         // string
		formatNumber,         // number
		formatLeftBracket,    // begin array
		formatRightBracket,   // end array
		formatLeftBrace,      // begin object
		formatKey,            // object key
		formatRightBrace,     // end object
		formatBool,           // keyword true or false
		formatNull,           // keyword null
		formatEnd,            // end of JSON
		show_error            // error handler
	};

	formatter->parser = jsonNewPushParser( &map, formatter );
}

/**
	@brief Format a string literal.
	@param blob Pointer to Formatter, cast to a void pointer.
	@param str Pointer to the contents of the string, with all escape sequences decoded.
	@return zero.

	Called by the parser when it finds a string literal (other than the name portion of a
	name/value pair in a JSON object).

	Write the literal within double quotes, with special and multibyte characters escaped
	as needed, and a comma and white as needed.
*/
static int formatString( void* blob, const char* str ) {
	Formatter* formatter = (Formatter*) blob;
	if( CTX_ARRAY == formatter->context )
		printf( ",\n" );
	else if( formatter->context != CTX_OBJ_KEY )
		printf( "\n" );

	if( formatter->context != CTX_OBJ_KEY )
		indent( formatter->indent );

	// Escape characters as needed
	buffer_reset( formatter->buf );
	buffer_append_utf8( formatter->buf, str );

	printf( "\"%s\"", OSRF_BUFFER_C_STR( formatter->buf ) );

	// Pick the next state
	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_KEY == formatter->context )
		formatter->context = CTX_OBJ;

	return 0;
}

/**
	@brief Format a numeric literal.
	@param blob Pointer to Formatter, cast to a void pointer.
	@param str Pointer to a string containing the numeric literal.
	@return zero.

	Called by the parser when it finds a numeric literal.

	Write the numeric literal, with a comma and white space as needed.
*/
static int formatNumber( void* blob, const char* str ) {
	Formatter* formatter = (Formatter*) blob;
	if( CTX_ARRAY == formatter->context )
		printf( ",\n" );
	else if( formatter->context != CTX_OBJ_KEY )
		printf( "\n" );

	if( formatter->context != CTX_OBJ_KEY )
		indent( formatter->indent );

	printf( "%s", str );

	// Pick the next state
	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_KEY == formatter->context )
		formatter->context = CTX_OBJ;

	return 0;
}

/**
	@brief Format a left square bracket.
	@param blob Pointer to Formatter, cast to a void pointer.
	@return zero.

	Called by the parser when it finds a left square bracket opening a JSON array.

	Write a left square bracket, with a comma and white space as needed.
*/
static int formatLeftBracket( void* blob ) {
	Formatter* formatter = blob;
	if( CTX_ARRAY == formatter->context || CTX_OBJ == formatter->context )
		printf( "," );
	printf( "\n" );
	indent( formatter->indent++ );
	printf( "[" );

	// Pick the state to return to when we close the array.
	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_BEGIN == formatter->context )
		formatter->context = CTX_OBJ;
	push_context( formatter );

	formatter->context = CTX_ARRAY_BEGIN;
	return 0;
}

/**
	@brief Format a right square bracket.
	@param blob Pointer to Formatter, cast to a void pointer.
	@return zero.

	Called by the parser when it finds a right square bracket closing a JSON array.

	Write a newline, indentation, and a right square bracket.
*/
static int formatRightBracket( void* blob ) {
	Formatter* formatter = blob;
	printf( "\n" );
	indent( --formatter->indent );
	printf( "]" );

	pop_context( formatter );
	return 0;
}

/**
	@brief Formate a left curly brace.
	@param blob Pointer to Formatter, cast to a void pointer.
	@return zero.

	Called by the parser when it finds a left curly brace opening a JSON object.

	Write a left curly brace, with a comma and white space as needed.
*/
static int formatLeftBrace( void* blob ) {
	Formatter* formatter = blob;
	if( CTX_ARRAY == formatter->context || CTX_OBJ == formatter->context )
		printf( "," );
	printf( "\n" );
	indent( formatter->indent++ );
	printf( "{" );

	// Pick the state to return to when we close the object.
	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_BEGIN == formatter->context )
		formatter->context = CTX_OBJ;
	push_context( formatter );

	formatter->context = CTX_OBJ_BEGIN;
	return 0;
}

/**
	@brief Format a right curly brace.
	@param blob Pointer to Formatter, cast to a void pointer.
	@return zero.

	Called by the parser when it finds a right curly brace closing a JSON object.

	Write a newline, indentation, and a right curly brace.
*/
static int formatRightBrace( void* blob ) {
	Formatter* formatter = blob;
	printf( "\n" );
	indent( --formatter->indent );
	printf( "}" );

	pop_context( formatter );
	return 0;
}

/**
	@brief Format the key of a key/value pair in a JSON object.
	@param blob Pointer to Formatter, cast to a void pointer.
	@param str Pointer to a string containing the key.
	@return zero.

	Called by the parser when it finds the key of a key/value pair.  It hasn't found the
	accompanying colon yet, and if it doesn't find it later, it will return an error.

	Write the key in double quotes, with a comma and white space as needed.
*/
static int formatKey( void* blob, const char* str ) {
	Formatter* formatter = blob;
	if( CTX_OBJ == formatter->context )
		printf( ",\n" );
	else
		printf( "\n" );
	indent( formatter->indent );

	// Escape characters as needed
	buffer_reset( formatter->buf );
	buffer_append_utf8( formatter->buf, str );

	printf( "\"%s\" : ", OSRF_BUFFER_C_STR( formatter->buf ) );

	formatter->context = CTX_OBJ_KEY;
	return 0;
}

/**
	@brief Format a boolean value.
	@param blob Pointer to Formatter, cast to a void pointer.
	@param b An int used as a boolean to indicate whether the boolean value is true or false.
	@return zero.

	Called by the parser when it finds the JSON keyword "true" or "false".

	Write "true" or "false" (without the quotes) with a comma and white as needed.
*/
static int formatBool( void* blob, int b ) {
	Formatter* formatter = (Formatter*) blob;
	if( CTX_ARRAY == formatter->context )
		printf( ",\n" );
	else if( formatter->context != CTX_OBJ_KEY )
		printf( "\n" );

	if( formatter->context != CTX_OBJ_KEY )
		indent( formatter->indent );

	printf( "%s", b ? "true" : "false" );

	// Pick the next state.
	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_KEY == formatter->context )
		formatter->context = CTX_OBJ;

	return 0;
}

/**
	@brief Format a null value.
	@param blob Pointer to Formatter, cast to a void pointer.
	@return zero.

	Called by the parser when it finds the JSON keyword "null".

	Write "null" (without the quotes) with a comma and white as needed.
*/
static int formatNull( void* blob ) {
	Formatter* formatter = (Formatter*) blob;
	if( CTX_ARRAY == formatter->context )
		printf( ",\n" );
	else if( formatter->context != CTX_OBJ_KEY )
		printf( "\n" );

	if( formatter->context != CTX_OBJ_KEY )
		indent( formatter->indent );

	printf( "null" );

	if( CTX_ARRAY_BEGIN == formatter->context )
		formatter->context = CTX_ARRAY;
	else if ( CTX_OBJ_KEY == formatter->context )
		formatter->context = CTX_OBJ;

	return 0;
}

/**
	@brief Respond to the end of a JSON value.
	@param blob Pointer to Formatter, cast to a void pointer.

	Called by the parser when it reaches the end of a JSON value.

	This formatter acccepts multiple JSON values in succession.  Tell the parser to look
	for another one.  Otherwise the parser will treat anything other than white space
	beyond this point as an error.

	Note that jsonPushParserResume() does @em not reset the line number and column number
	used by the parser for error messages.  If you want to do that. call jsonPushParserReset().
*/
static void formatEnd( void* blob ) {
	Formatter* formatter = blob;
	jsonPushParserResume( formatter->parser );
	printf( "\n" );
}

/**
	@brief Issue an error message about a syntax error detected by the parser.
	@param blob
	@param msg Pointer to a message describing the syntax error.
	@param line Line number in the current file where the error was detected.
	@param pos Column position in the current line where the error was detected.

	Called by the parser when it encounters a syntax error.

	Write the message to standard error, providing the file name (saved in the Formatter),
	line number, and column position.
*/
static void show_error( void* blob, const char* msg, unsigned line, unsigned pos ) {
	Formatter* formatter = (Formatter*) blob;
	const char* filename = formatter->filename;
	if( !filename )
		filename = "standard input";
	fprintf( stderr, "\nError in %s at line %u, position %u:\n%s\n",
		filename, line, pos, msg );
}

/**
	@brief Write a specified number of indents, four spaces per indent.
	@param n How many indents to write.
*/
static void indent( unsigned n ) {
	while( n ) {
		printf( "    " );
		--n;
	}
}

/**
	@brief Push the current state onto the stack.
	@param formatter Pointer to the current Formatter.

	We call this when we enter a JSON array or object.  Later, when we reach the end of the
	array or object, we'll call pop_context() to restore the saved state.
*/
static void push_context( Formatter* formatter ) {
	// Allocate a ContextNode; from the free list if possible,
	// or from the heap if necessary
	ContextNode* node = NULL;
	if( free_context ) {
		node = free_context;
		free_context = free_context->next;
	} else
		node = safe_malloc( sizeof( ContextNode ) );

	node->context = formatter->context;
	node->next = formatter->context_stack;
	formatter->context_stack = node;
}

/**
	@brief Pop a state off the stack.
	@param formatter Pointer to the current Formatter.

	We call this at the end of a JSON array or object, in order to restore the state saved
	when we entered the array or object.
*/
static void pop_context( Formatter* formatter ) {
	if( !formatter->context_stack )
		return;                    // shouldn't happen

	ContextNode* node = formatter->context_stack;
	formatter->context_stack = node->next;

	formatter->context = node->context;

	node->next = free_context;
	free_context = node;
}
