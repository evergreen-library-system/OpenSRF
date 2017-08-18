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

/**
	@file utils.h

	@brief Prototypes for various low-level utility functions, and related macros.

	Many of these facilities concern the growing_buffer structure,
	a sort of poor man's string class that allocates more space for
	itself as needed.
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

#ifdef __cplusplus
extern "C" {
#endif

#include "md5.h"
/**
	@brief Macro version of safe_malloc()
	@param ptr Pointer to be updated to point to newly allocated memory
	@param size How many bytes to allocate
*/
#define OSRF_MALLOC(ptr, size) \
	do {\
			size_t _size = size; \
			void* p = malloc( _size ); \
			if( p == NULL ) { \
				perror("OSRF_MALLOC(): Out of Memory" ); \
				exit(99); \
			} \
			memset( p, 0, _size ); \
			(ptr) = p; \
		} while(0)

#ifdef NDEBUG
#define osrf_clearbuf( s, n ) memset( s, 0, n )
#else
/**
	 @brief Fills a buffer with binary zeros (normal mode) or exclamation points (debugging mode)
	 @param s Pointer to buffer
	 @param n Length of buffer

	 This macro is used to help ferret out code that inappropriately assumes that a newly
	 allocated buffer is filled with binary zeros.  No code should rely on it to do
	 anything in particular.  Someday it may turn into a no-op.
*/
#define osrf_clearbuf( s, n ) \
	do { \
		char * clearbuf_temp_s = (s); \
		size_t clearbuf_temp_n = (n); \
		memset( clearbuf_temp_s, '!', clearbuf_temp_n ); \
		clearbuf_temp_s[ clearbuf_temp_n - 1 ] = '\0'; \
	} while( 0 )
#endif

/**
	@brief Macro version of buffer_add()
	@param gb Pointer to a growing_buffer
	@param data Pointer to the string to be appended
*/

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

/**
	@brief Macro version of buffer_add_n()
	@param gb Pointer to a growing_buffer
	@param data Pointer to the bytes to be appended
	@param n How many characters to append
*/
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

/**
	@brief Macro version of buffer_add_char()
	@param gb Pointer to a growing buffer
	@param c Character to be appended
*/
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

/**
	@brief Macro version of buffer_reset()
	@param gb Pointer to the growing_buffer to be reset
*/
#define OSRF_BUFFER_RESET(gb) \
	do {\
		growing_buffer* _gb = gb;\
		memset(_gb->buf, 0, _gb->size);\
		_gb->n_used = 0;\
	}while(0)

/**
	@brief Resolves to a const pointer to the string inside a growing_buffer
	@param x Pointer to a growing_buffier
*/
#define OSRF_BUFFER_C_STR( x ) ((const char *) (x)->buf)


/**
	@brief Turn a printf-style format string and a va_list into a string.
	@param x A printf-style format string.

	This macro can work only in a variadic function.

	The resulting string is constructed in a local buffer, whose address is
	given by the pointer VA_BUF,  This buffer is NOT allocated dynamically,
	so don't try to free it.
*/
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

/**
	@brief Format a long into a string.
	@param l A long

	The long is formatted into a local buffer whose address is given by the pointer
	LONGSTR.  This buffer is NOT allocated dynamically, so don't try to free it.
*/
#define LONG_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%ld", l) + 2;\
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%ld", l); \
	char* LONGSTR = __b;

/**
	@brief Format a double into a string.
	@param l A double

	The double is formatted into a local buffer whose address is given by the pointer
	DOUBLESTR.  This buffer is NOT allocated dynamically, so don't try to free it.
*/
#define DOUBLE_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%f", l) + 2; \
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%f", l); \
	char* DOUBLESTR = __b;

/**
	@brief Format a long double into a string.
	@param l A long double

	The long double is formatted into a local buffer whose address is given by the pointer
	LONGDOUBLESTR.  This buffer is NOT allocated dynamically, so don't try to free it.
*/
#define LONG_DOUBLE_TO_STRING(l) \
	unsigned int __len = snprintf(NULL, 0, "%Lf", l) + 2; \
	char __b[__len]; \
	bzero(__b, __len); \
	snprintf(__b, __len - 1, "%Lf", l); \
	char* LONGDOUBLESTR = __b;


/**
	@brief Format an int into a string.
	@param l An int

	The int is formatted into a local buffer whose address is given by the pointer
	INTSTR.  This buffer is NOT allocated dynamically, so don't try to free it.
*/
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


/**
	@brief The maximum buffer size for a growing_buffer
*/
#define BUFFER_MAX_SIZE 10485760

/* these are evil and should be condemned
	! Only use these if you are done with argv[].
	call init_proc_title() first, then call
	set_proc_title.
	the title is only allowed to be as big as the
	initial process name of the process (full size of argv[]).
	truncation may occur.
*/
int init_proc_title( int argc, char* argv[] );
int set_proc_title( const char* format, ... );

int daemonizeWithCallback( void (*)(pid_t, int), int );
int daemonize( void );

void* safe_malloc(int size);
void* safe_calloc(int size);

// ---------------------------------------------------------------------------------
// Generic growing buffer. Add data all you want
// ---------------------------------------------------------------------------------
/**
	@brief A poor man's string class in C.

	A growing_buffer stores a character string.  Related functions append data
	and otherwise manage the string, allocating more memory automatically as needed
	when the string gets too big for its buffer.

	A growing_buffer is designed for text, not binary data.  In particular: if you
	try to store embedded nuls in one, something bad will almost certainly happen.
*/
struct growing_buffer_struct {
	/** @brief Pointer to the internal buffer */
	char *buf;
	/** @brief Length of the stored string */
	int n_used;
	/** @brief Size of the internal buffer */
	int size;
};
typedef struct growing_buffer_struct growing_buffer;

/**
	@brief The length of the string stored by a growing_buffer.
	@param x A pointer to the growing buffer.
*/
#define buffer_length(x) (x)->n_used

growing_buffer* buffer_init( int initial_num_bytes);

int buffer_add(growing_buffer* gb, const char* c);
int buffer_add_n(growing_buffer* gb, const char* data, size_t n);
int buffer_fadd(growing_buffer* gb, const char* format, ... );
int buffer_reset( growing_buffer* gb);
char* buffer_data( const growing_buffer* gb);
char* buffer_release( growing_buffer* gb );
int buffer_free( growing_buffer* gb );
int buffer_add_char(growing_buffer* gb, char c);
char buffer_chomp(growing_buffer* gb); // removes the last character from the buffer

/*
	returns the size needed to fill in the vsnprintf buffer.
	this calls va_end on the va_list argument*
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
	\thello,\n\t\tyou
*/
char* uescape( const char* string, int size, int full_escape );

/* utility methods */
int set_fl( int fd, int flags );
int clr_fl( int fd, int flags );


// Utility method
double get_timestamp_millis( void );


/* returns true if the whole string is a number */
int stringisnum(const char* s);


/*
	Calculates the md5 of the text provided.
	The returned string must be freed by the caller.
*/
char* md5sum( const char* text, ... );


/*
	Checks the validity of the file descriptor
	returns -1 if the file descriptor is invalid
	returns 0 if the descriptor is OK
*/
int osrfUtilsCheckFileDescriptor( int fd );

/*
	Returns the approximate additional length of
	a string after XML escaping <, >, &, and ".
*/
size_t osrfXmlEscapingLength ( const char* str );

#ifdef __cplusplus
}
#endif

#endif
