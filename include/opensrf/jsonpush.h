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
	@file jsonpush.h
	@brief Push parser for JSON.

	This parser provides a way to parse JSON incrementally, without necessarily holding the
	entire JSON string (or any representation thereof) in memory at once.  It can therefore
	be used, for example, to parse large input files.

	How to use it:

	1. Call jsonNewPushParser() to create a parser, designating a series of callback
	functions to be called when the parser encounters various syntactic features.

	2. Pass one or more buffers to jsonPush() for parsing.

	3. When the last buffer has been parsed, call jsonPushParserFinish() to tell the parser
	that no more input will be forthcoming.

	4. Call jsonPushParserFree() to free the parser when you're done with it.

	By using jsonPushParserReset(), you can reuse a parser for multiple streams, without
	having to free and recreate it.

	By using jsonPushParserResume(), you can accept multiple JSON values in the same stream.
	It is identical to jsonPushParserReset(), except that it does not reset the line number
	and column number used in error messages.

	This parser does @em not give any special attention to OSRF-specific conventions for
	encoding class information.
*/

#ifndef JSONPUSH_H
#define JSONPUSH_H

#ifdef __cplusplus
extern "C" {
#endif

struct JSONPushParserStruct;
typedef struct JSONPushParserStruct JSONPushParser;

/** @brief A collection of callback pointers */
typedef struct {

	int (*handleString)( void* blob, const char* str );
	int (*handleNumber)( void* blob, const char* str );
	int (*handleBeginArray )( void* blob );
	int (*handleEndArray )( void* blob );
	int (*handleBeginObj )( void* blob );
	int (*handleObjKey )( void* blob, const char* key );
	int (*handleEndObj )( void* blob );
	int (*handleBool)   ( void* blob, int b );
	int (*handleNull)   ( void* blob );
	void (*handleEndJSON )( void* blob );
	void (*handleError)( void* blob, const char* msg, unsigned line, unsigned pos );

} JSONHandlerMap;

JSONPushParser* jsonNewPushParser( const JSONHandlerMap* map, void* blob );

void jsonPushParserReset( JSONPushParser* parser );

void jsonPushParserResume( JSONPushParser* parser );

int jsonPushParserFinish( JSONPushParser* parser );

void jsonPushParserFree( JSONPushParser* parser );

int jsonPush( JSONPushParser* parser, const char* str, size_t length );

#ifdef __cplusplus
}
#endif

#endif
