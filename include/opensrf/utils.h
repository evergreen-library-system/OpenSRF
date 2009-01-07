/*
Copyright (C) 2005  Georgia Public Library Service 
Bill Erickson <highfalutin@gmail.com>
Mike Rylander <mrylander@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/timeb.h>

#include "md5.h"

#define OSRF_MALLOC(ptr, size) \
	do {\
		ptr = (void*) malloc( size ); \
		if( ptr == NULL ) { \
			perror("OSRF_MALLOC(): Out of Memory" );\
			exit(99); \
		} \
		memset( ptr, 0, size );\
	} while(0)

#ifdef NDEBUG
// The original ... replace with noop once no more errors occur in NDEBUG mode
#define osrf_clearbuf( s, n ) memset( s, 0, n )
#else
#define osrf_clearbuf( s, n ) \
	do { \
		char * clearbuf_temp_s = (s); \
		size_t clearbuf_temp_n = (n); \
		memset( clearbuf_temp_s, '!', clearbuf_temp_n ); \
		clearbuf_temp_s[ clearbuf_temp_n - 1 ] = '\0'; \
	} while( 0 )
#endif

#define OSRF_BUFFER_ADD(gb, data) \
	do {\
		int _tl; \
		growing_buffer* _gb = gb; \
		const char* _data = data; \
		if(_gb && _data) {\
			_tl = strlen(_data) + _gb->n_used;\
			if( _tl < _gb->size ) {\
				strcpy( _gb->buf + _gb->n_used, _data ); \
				_gb->n_used = _tl; \
			} else { buffer_add(_gb, _data); }\
		}\
	} while(0)

#define OSRF_BUFFER_ADD_N(gb, data, n) \
	do {\
		growing_buffer* gb__ = gb; \
		const char* data__ = data; \
		size_t n__ = n; \
		if(gb__ && data__) {\
			int tl__ = n__ + gb__->n_used;\
			if( tl__ < gb__->size ) {\
				memcpy( gb__->buf + gb__->n_used, data__, n__ ); \
				gb__->buf[tl__] = '\0'; \
				gb__->n_used = tl__; \
} else { buffer_add_n(gb__, data__, n__); }\
}\
} while(0)

#define OSRF_BUFFER_ADD_CHAR(gb, c)\
	do {\
		growing_buffer* _gb = gb;\
		char _c = c;\
		if(_gb) {\
			if(_gb->n_used < _gb->size - 1) {\
				_gb->buf[_gb->n_used++] = _c;\
				_gb->buf[_gb->n_used]   = '\0';\
			}\
			else\
				buffer_add_char(_gb, _c);\
		}\
	}while(0)

#define OSRF_BUFFER_RESET(gb) \
	do {\
		growing_buffer* _gb = gb;\
    	memset(_gb->buf, 0, _gb->size);\
    	_gb->n_used = 0;\
	}while(0)

#define OSRF_BUFFER_C_STR( x ) ((const char *) (x)->buf)


/* turns a va_list into a string */
#define VA_LIST_TO_STRING(x) \
	unsigned long __len = 0;\
	va_list args; \
	va_list a_copy;\
	va_copy(a_copy, args); \
	va_start(args, x); \
	__len = vsnprintf(NULL, 0, x, args); \
	va_end(args); \
	__len += 2; \
	char _b[__len]; \
	bzero(_b, __len); \
	va_start(a_copy, x); \
	vsnprintf(_b, __len - 1, x, a_copy); \
	va_end(a_copy); \
	char* VA_BUF = _b; \

/* turns a long into a string */
#define LONG_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%ld", l) + 2;\
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%ld", l); \
	char* LONGSTR = __b;

#define DOUBLE_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%f", l) + 2; \
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%f", l); \
	char* DOUBLESTR = __b;

#define LONG_DOUBLE_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%Lf", l) + 2; \
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%Lf", l); \
	char* LONGDOUBLESTR = __b;


#define INT_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%d", l) + 2; \
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%d", l); \
	char* INTSTR = __b;


/*
#define MD5SUM(s) \
	struct md5_ctx ctx; \
	unsigned char digest[16];\
	MD5_start (&ctx);\
	int i;\
	for ( i=0 ; i != strlen(text) ; i++ ) MD5_feed (&ctx, text[i]);\
	MD5_stop (&ctx, digest);\
	char buf[16];\
	memset(buf,0,16);\
	char final[256];\
	memset(final,0,256);\
	for ( i=0 ; i<16 ; i++ ) {\
		sprintf(buf, "%02x", digest[i]);\
		strcat( final, buf );\
	}\
	char* MD5STR = final;
	*/


	


#define BUFFER_MAX_SIZE 10485760 

/* these are evil and should be condemned 
	! Only use these if you are done with argv[].
	call init_proc_title() first, then call
	set_proc_title. 
	the title is only allowed to be as big as the
	initial process name of the process (full size of argv[]).
	truncation may occurr.
 */
int init_proc_title( int argc, char* argv[] );
int set_proc_title( const char* format, ... );


int daemonize( void );

void* safe_malloc(int size);
void* safe_calloc(int size);

// ---------------------------------------------------------------------------------
// Generic growing buffer. Add data all you want
// ---------------------------------------------------------------------------------
struct growing_buffer_struct {
	char *buf;
	int n_used;
	int size;
};
typedef struct growing_buffer_struct growing_buffer;

#define buffer_length(x) (x)->n_used

growing_buffer* buffer_init( int initial_num_bytes);

// XXX This isn't defined in utils.c!! removing for now...
//int buffer_addchar(growing_buffer* gb, char c);

int buffer_add(growing_buffer* gb, const char* c);
int buffer_add_n(growing_buffer* gb, const char* data, size_t n);
int buffer_fadd(growing_buffer* gb, const char* format, ... );
int buffer_reset( growing_buffer* gb);
char* buffer_data( const growing_buffer* gb);
char* buffer_release( growing_buffer* gb );
int buffer_free( growing_buffer* gb );
int buffer_add_char(growing_buffer* gb, char c);
char buffer_chomp(growing_buffer* gb); // removes the last character from the buffer

/* returns the size needed to fill in the vsnprintf buffer.  
	* ! this calls va_end on the va_list argument*
	*/
long va_list_size(const char* format, va_list);

/* turns a va list into a string, caller must free the 
	allocated char */
char* va_list_to_string(const char* format, ...);


/* string escape utility method.  escapes unicode embedded characters.
	escapes the usual \n, \t, etc. 
	for example, if you provide a string like so:

	hello,
		you

	you would get back:
	hello,\n\tyou
 
 */
char* uescape( const char* string, int size, int full_escape );

/* utility methods */
int set_fl( int fd, int flags );
int clr_fl( int fd, int flags );



// Utility method
double get_timestamp_millis( void );


/* returns true if the whole string is a number */
int stringisnum(const char* s);


/** 
  Calculates the md5 of the text provided.
  The returned string must be freed by the caller.
  */
char* md5sum( const char* text, ... );


/**
  Checks the validity of the file descriptor
  returns -1 if the file descriptor is invalid
  returns 0 if the descriptor is OK
  */
int osrfUtilsCheckFileDescriptor( int fd );

#endif
