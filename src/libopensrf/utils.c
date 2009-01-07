/*
Copyright (C) 2005  Georgia Public Library Service 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <errno.h>

inline void* safe_malloc( int size ) {
	void* ptr = (void*) malloc( size );
	if( ptr == NULL ) {
		osrfLogError( OSRF_LOG_MARK, "Out of Memory" );
		exit(99);
	}
	memset( ptr, 0, size ); // remove this after safe_calloc transition
	return ptr;
}

inline void* safe_calloc( int size ) {
	void* ptr = (void*) calloc( 1, size );
	if( ptr == NULL ) {
		osrfLogError( OSRF_LOG_MARK, "Out of Memory" );
		exit(99);
	}
	return ptr;
}

/****************
 The following static variables, and the following two functions,
 overwrite the argv array passed to main().  The purpose is to
 change the program name as reported by ps and similar utilities.

 Warning: this code makes the non-portable assumption that the
 strings to which argv[] points are contiguous in memory.  The
 C Standard makes no such guarantee.
 ****************/
static char** global_argv = NULL;
static int global_argv_size = 0;

int init_proc_title( int argc, char* argv[] ) {

	global_argv = argv;

	int i = 0;
	while( i < argc ) {
		int len = strlen( global_argv[i]);
		osrf_clearbuf( global_argv[i], len );
		global_argv_size += len;
		i++;
	}

	global_argv_size -= 2;
	return 0;
}

int set_proc_title( const char* format, ... ) {
	VA_LIST_TO_STRING(format);
	osrf_clearbuf( *(global_argv), global_argv_size);
	return snprintf( *(global_argv), global_argv_size, VA_BUF );
}


/* utility method for profiling */
double get_timestamp_millis( void ) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	double time	= (int)tv.tv_sec	+ ( ((double)tv.tv_usec / 1000000) );
	return time;
}


/* setting/clearing file flags */
int set_fl( int fd, int flags ) {
	
	int val;

	if( (val = fcntl( fd, F_GETFL, 0) ) < 0 ) 
		return -1;

	val |= flags;

	if( fcntl( fd, F_SETFL, val ) < 0 ) 
		return -1;

	return 0;
}
	
int clr_fl( int fd, int flags ) {
	
	int val;

	if( (val = fcntl( fd, F_GETFL, 0) ) < 0 ) 
		return -1;

	val &= ~flags;

	if( fcntl( fd, F_SETFL, val ) < 0 ) 
		return -1;

	return 0;
}

long va_list_size(const char* format, va_list args) {
	int len = 0;
	len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	len += 2;
	return len;
}


char* va_list_to_string(const char* format, ...) {

	long len = 0;
	va_list args;
	va_list a_copy;

	va_copy(a_copy, args);

	va_start(args, format);
	len = va_list_size(format, args);

	char buf[len];
	osrf_clearbuf(buf, sizeof(buf));

	va_start(a_copy, format);
	vsnprintf(buf, len - 1, format, a_copy);
	va_end(a_copy);
	return strdup(buf);
}

// ---------------------------------------------------------------------------------
// Flesh out a ubiqitous growing string buffer
// ---------------------------------------------------------------------------------

growing_buffer* buffer_init(int num_initial_bytes) {

	if( num_initial_bytes > BUFFER_MAX_SIZE ) return NULL;

	size_t len = sizeof(growing_buffer);

	growing_buffer* gb;
	OSRF_MALLOC(gb, len);

	gb->n_used = 0;/* nothing stored so far */
	gb->size = num_initial_bytes;
	OSRF_MALLOC(gb->buf, gb->size + 1);

	return gb;
}


/* Expand the internal buffer of a growing_buffer so that it */
/* will accommodate a specified string length.  Return 0 if  */
/* successful, or 1 otherwise. */

/* Note that we do not check to see if the buffer is already */
/* big enough.  It is the responsibility of the calling      */
/* function to call this only when necessary. */

static int buffer_expand( growing_buffer* gb, size_t total_len ) {

	// Make sure the request is not excessive
	
	if( total_len >= BUFFER_MAX_SIZE ) {
		fprintf(stderr, "Buffer reached MAX_SIZE of %lu",
				(unsigned long) BUFFER_MAX_SIZE );
		buffer_free( gb );
		return 1;
	}

	// Pick a big enough buffer size, but don't exceed a maximum
	
	while( total_len >= gb->size ) {
		gb->size *= 2;
	}

	if( gb->size > BUFFER_MAX_SIZE )
		gb->size = BUFFER_MAX_SIZE;

	// Allocate and populate the new buffer
	
	char* new_data;
	OSRF_MALLOC( new_data, gb->size );
	memcpy( new_data, gb->buf, gb->n_used );
	new_data[ gb->n_used ] = '\0';

	// Replace the old buffer
	
	free( gb->buf );
	gb->buf = new_data;
	return 0;
}


int buffer_fadd(growing_buffer* gb, const char* format, ... ) {

	if(!gb || !format) return 0; 

	long len = 0;
	va_list args;
	va_list a_copy;

	va_copy(a_copy, args);

	va_start(args, format);
	len = va_list_size(format, args);

	char buf[len];
	osrf_clearbuf(buf, sizeof(buf));

	va_start(a_copy, format);
	vsnprintf(buf, len - 1, format, a_copy);
	va_end(a_copy);

	return buffer_add(gb, buf);

}


int buffer_add(growing_buffer* gb, const char* data) {
	if(!(gb && data)) return 0;

	int data_len = strlen( data );

	if(data_len == 0) return 0;

	int total_len = data_len + gb->n_used;

	if( total_len >= gb->size ) {
		if( buffer_expand( gb, total_len ) )
			return -1;
	}

	strcpy( gb->buf + gb->n_used, data );
	gb->n_used = total_len;
	return total_len;
}

/** Append a specified number of characters to a growing_buffer.
    If the characters so appended include an embedded nul, the results
    are likely to be unhappy.
*/
int buffer_add_n(growing_buffer* gb, const char* data, size_t n) {
	if(!(gb && data)) return 0;

	if(n == 0) return 0;

	int total_len = n + gb->n_used;

	if( total_len >= gb->size ) {
		if( buffer_expand( gb, total_len ) )
			return -1;
	}

	memcpy( gb->buf + gb->n_used, data, n );
	gb->buf[total_len] = '\0';
	gb->n_used = total_len;
	return total_len;
}


int buffer_reset( growing_buffer *gb){
	if( gb == NULL ) { return -1; }
	if( gb->buf == NULL ) { return -1; }
	osrf_clearbuf( gb->buf, gb->size );
	gb->n_used = 0;
	gb->buf[ 0 ] = '\0';
	return gb->n_used;
}

/* Return a pointer to the text within a growing_buffer, */
/* while destroying the growing_buffer itself.           */

char* buffer_release( growing_buffer* gb) {
	char* s = gb->buf;
	s[gb->n_used] = '\0';
	free( gb );
	return s;
}

/* Destroy a growing_buffer and the text it contains */

int buffer_free( growing_buffer* gb ) {
	if( gb == NULL ) 
		return 0;
	free( gb->buf );
	free( gb );
	return 1;
}

char* buffer_data( const growing_buffer *gb) {
	return strdup( gb->buf );
}

char buffer_chomp(growing_buffer* gb) {
	char c = '\0';
    if(gb && gb->n_used > 0) {
	    gb->n_used--;
		c = gb->buf[gb->n_used];
	    gb->buf[gb->n_used] = '\0';
    }
    return c;
}


int buffer_add_char(growing_buffer* gb, char c ) {
	if(gb && gb->buf) {

		int total_len = gb->n_used + 1;

		if( total_len >= gb->size ) {
			if( buffer_expand( gb, total_len ) )
				return -1;
		}
	
		gb->buf[ gb->n_used ]   = c;
		gb->buf[ ++gb->n_used ] = '\0';
	}
	
	return gb->n_used;
}


char* uescape( const char* string, int size, int full_escape ) {

	if( NULL == string )
		return NULL;
	
	growing_buffer* buf = buffer_init(size + 64);
	int clen = 0;
	int idx = 0;
	unsigned long int c = 0x0;

	while (string[idx]) {

		c = 0x0;

		if ((unsigned char)string[idx] >= 0x80) { // not ASCII

			if ((unsigned char)string[idx] >= 0xC0 && (unsigned char)string[idx] <= 0xF4) { // starts a UTF8 string

				clen = 1;
				if (((unsigned char)string[idx] & 0xF0) == 0xF0) {
					clen = 3;
					c = (unsigned char)string[idx] ^ 0xF0;

				} else if (((unsigned char)string[idx] & 0xE0) == 0xE0) {
					clen = 2;
					c = (unsigned char)string[idx] ^ 0xE0;

				} else if (((unsigned char)string[idx] & 0xC0) == 0xC0) {
					clen = 1;
					c = (unsigned char)string[idx] ^ 0xC0;
				}

				for (;clen;clen--) {

					idx++; // look at the next byte
					c = (c << 6) | ((unsigned char)string[idx] & 0x3F); // add this byte worth

				}

				buffer_fadd(buf, "\\u%04x", c);

			} else {
				buffer_free(buf);
				return NULL;
			}

		} else {
			c = string[idx];

			/* escape the usual suspects */
			if(full_escape) {
				switch(c) {
					case '"':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, '"');
						break;
	
					case '\b':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, 'b');
						break;
	
					case '\f':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, 'f');
						break;
	
					case '\t':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, 't');
						break;
	
					case '\n':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, 'n');
						break;
	
					case '\r':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, 'r');
						break;

					case '\\':
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						OSRF_BUFFER_ADD_CHAR(buf, '\\');
						break;

					default:
						if( c < 32 ) buffer_fadd(buf, "\\u%04x", c);
						else OSRF_BUFFER_ADD_CHAR(buf, c);
				}

			} else {
				OSRF_BUFFER_ADD_CHAR(buf, c);
			}
		}

		idx++;
	}

	return buffer_release(buf);
}


// A function to turn a process into a daemon 
int daemonize( void ) {
	pid_t f = fork();

	if (f == -1) {
		osrfLogError( OSRF_LOG_MARK, "Failed to fork!" );
		return -1;

	} else if (f == 0) { // We're in the child now...
		
		// Change directories.  Otherwise whatever directory
		// we're in couldn't be deleted until the program
		// terminated -- possibly causing some inconvenience.
		chdir( "/" );

		/* create new session */
		setsid();

		// Now that we're no longer attached to a terminal,
		// we don't want any traffic on the standard streams
		freopen( "/dev/null", "r", stdin );
		freopen( "/dev/null", "w", stdout );
		freopen( "/dev/null", "w", stderr );
		
		return 0;

	} else { // We're in the parent...
		_exit(0);
	}
}


/* Return 1 if the string represents an integer,  */
/* as recognized by strtol(); Otherwise return 0. */

int stringisnum(const char* s) {
	char* w;
	strtol(s, &w, 10);
	return *w ? 0 : 1;
}
	


char* md5sum( const char* text, ... ) {

	struct md5_ctx ctx;
	unsigned char digest[16];

	MD5_start (&ctx);

	VA_LIST_TO_STRING(text);

	int i;
	for ( i=0 ; i != strlen(VA_BUF) ; i++ )
		MD5_feed (&ctx, VA_BUF[i]);

	MD5_stop (&ctx, digest);

	char buf[16];
	char final[256];
	osrf_clearbuf(final, sizeof(final));

	for ( i=0 ; i<16 ; i++ ) {
		snprintf(buf, sizeof(buf), "%02x", digest[i]);
		strcat( final, buf );
	}

	return strdup(final);

}

int osrfUtilsCheckFileDescriptor( int fd ) {

	fd_set tmpset;
	FD_ZERO(&tmpset);
	FD_SET(fd, &tmpset);

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if( select(fd + 1, &tmpset, NULL, NULL, &tv) == -1 ) {
		if( errno == EBADF ) return -1;
	}

	return 0;
}

