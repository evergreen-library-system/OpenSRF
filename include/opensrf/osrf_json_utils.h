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

#ifndef OSRF_JSON_UTILS_H
#define OSRF_JSON_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------- */
/* Clients need not include this file.  These are internal utilities only	*/
/* ----------------------------------------------------------------------- */

#define JSON_EAT_WS(ctx)	\
	while( ctx->index < ctx->chunksize ) {	\
		if(!isspace(ctx->chunk[ctx->index])) break; \
		ctx->index++;	\
	} \
	if( ctx->index >= ctx->chunksize ) return 0; \
	c = ctx->chunk[ctx->index];

#define JSON_CACHE_DATA(ctx, buf, size) \
	while( (buf->n_used < size) && (ctx->index < ctx->chunksize) ) \
		buffer_add_char(buf, ctx->chunk[ctx->index++]); 

#define JSON_LOG_MARK __FILE__,__LINE__

#define JSON_NUMBER_CHARS "0123456789.+-eE"

/**
 * These are the callbacks through which the top level parser 
 * builds objects via the push parser
 */
void _jsonHandleStartObject(void*);
void _jsonHandleObjectKey(void*, char* key);
void _jsonHandleEndObject(void*);
void _jsonHandleStartArray(void*);
void _jsonHandleEndArray(void*);
void _jsonHandleNull(void*);
void _jsonHandleString(void*, char* string);
void _jsonHandleBool(void*, int boolval);
void _jsonHandleNumber(void*, const char* numstr);
void _jsonHandleError(void*, char* str, ...);

struct jsonInternalParserStruct {
	jsonParserContext* ctx;
	jsonObject* obj;
	jsonObject* current;
	char* lastkey;
	void (*handleError) (const char*);
};
typedef struct jsonInternalParserStruct jsonInternalParser;

jsonInternalParser* _jsonNewInternalParser();
void _jsonInternalParserFree(jsonInternalParser* p);

/**
 * Calls the defined error handler with the given error message.
 * @return -1
 */
int _jsonParserError( jsonParserContext* ctx, char* err, ... );


/**
 *
 * @return 0 on continue, 1 if it goes past the end of the string, -1 on error
 */
int _jsonParserHandleUnicode( jsonParserContext* ctx );


/**
 * @param type 0 for null, 1 for true, 2 for false
 * @return 0 on continue, 1 if it goes past the end of the string, -1 on error
 */
int _jsonParserHandleMatch( jsonParserContext* ctx, int type );

/**
 * @return 0 on continue, 1 on end of chunk, -1 on error 
 */
int _jsonParserHandleString( jsonParserContext* ctx );

/**
 * @return 0 on continue, 1 on end of chunk, -1 on error 
 */
int _jsonParserHandleNumber( jsonParserContext* ctx );


void _jsonInsertParserItem( jsonInternalParser* p, jsonObject* newo );

#ifdef __cplusplus
}
#endif

#endif

