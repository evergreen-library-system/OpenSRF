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

/**
	@file utils.c
	
	@brief A collection of various low-level utility functions.
	
	About half of these functions concern the growing_buffer structure,
	a sort of poor man's string class that allocates more space for
	itself as needed.
*/
#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <errno.h>

/**
	@brief A thin wrapper for malloc().
	
	@param size How many bytes to allocate.
	@return a pointer to the allocated memory.

	If the allocation fails, safe_malloc calls exit().  Consequently the
	calling code doesn't have to check for NULL.

	Currently safe_malloc() initializes the allocated buffer to all-bits-zero.
	However the calling code should not rely on this behavior, because it is
	likely to change.  If you need your buffer filled with all-bits-zero, then
	call safe_calloc() instead, or fill it yourself by calling memset().
*/
inline void* safe_malloc( int size ) {
	void* ptr = (void*) malloc( size );
	if( ptr == NULL ) {
		osrfLogError( OSRF_LOG_MARK, "Out of Memory" );
		exit(99);
	}
	memset( ptr, 0, size ); // remove this after safe_calloc transition
	return ptr;
}

/**
	@brief A thin wrapper for calloc().
	
	@param size How many bytes to allocate.
	@return a pointer to the allocated memory.

	If the allocation fails, safe_calloc calls exit().  Consequently the
	calling code doesn't have to check for NULL.
 */
inline void* safe_calloc( int size ) {
	void* ptr = (void*) calloc( 1, size );
	if( ptr == NULL ) {
		osrfLogError( OSRF_LOG_MARK, "Out of Memory" );
		exit(99);
	}
	return ptr;
}

/**
	@brief Saves a pointer to the beginning of the argv[] array from main().  See init_proc_title().
*/
static char** global_argv = NULL;
/**
	@brief Saves the length of the argv[] array from main().  See init_proc_title().
 */
static int global_argv_size = 0;

/**
	@brief Save the size and location of the argv[] array.
	@param argc The argc parameter to main().
	@param argv The argv parameter to main().
	@return zero.

	Save a pointer to the argv[] array in the local static variable
	global_argv.  Add up the lengths of the strings in the argv[]
	array, subtract 2, and store the result in the local variable
	global_argv_size.

	The return value, being invariant, is useless.

	This function prepares for a subsequent call to set_proc_title().
*/
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

/**
	@brief Replace the name of the running executable.
	@param format A printf-style format string.  Subsequent parameters, if any,
		provide values to be formatted and inserted into the format string.
	@return Length of the resulting string (or what the length would be if the
		receiving buffer were big enough to hold it), or -1 in case of an encoding
		error.  Note: because some older versions of snprintf() don't work correctly,
		this function may return -1 if the string is truncated for lack of space.

	Formats a string as directed, and uses it to replace the name of the
	currently running executable.  This replacement string is what will
	be seen and reported by utilities such as ps and top.

	The replacement string goes into a location identified by a previous call
	to init_proc_title().

	WARNING: this function makes assumptions about the memory layout of
	the argv[] array.  ANSI C does not guarantee that these assumptions
	are correct.
*/
int set_proc_title( const char* format, ... ) {
	VA_LIST_TO_STRING(format);
	osrf_clearbuf( *(global_argv), global_argv_size);
	return snprintf( *(global_argv), global_argv_size, VA_BUF );
}

/**
	@brief Determine current date and time to high precision.
	@return Current date and time as seconds since the Epoch.

	Used for profiling.  The time resolution is system-dependent but is no finer
	than microseconds.
*/
double get_timestamp_millis( void ) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	double time	= (int)tv.tv_sec	+ ( ((double)tv.tv_usec / 1000000) );
	return time;
}


/**
	@brief Set designated file status flags for an open file descriptor.
	@param fd The file descriptor to be tweaked.
	@param flags A set of bitflags.
	@return 0 if successful, -1 or if not.

	Whatever bits are set in the flags parameter become set in the file status flags of
	the file descriptor -- subject to the limitation that the only bits affected (at
	least on Linux) are O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and O_NONBLOCK.

	See also clr_fl().
*/
int set_fl( int fd, int flags ) {
	
	int val;

	if( (val = fcntl( fd, F_GETFL, 0) ) < 0 ) 
		return -1;

	val |= flags;

	if( fcntl( fd, F_SETFL, val ) < 0 ) 
		return -1;

	return 0;
}
	
/**
	@brief Clear designated file status flags for an open file descriptor.
	@param fd The file descriptor to be tweaked.
	@param flags A set of bitflags.
	@return 0 if successful, or -1 if not.

	Whatever bits are set in the flags parameter become cleared in the file status flags
	of the file descriptor -- subject to the limitation that the only bits affected (at
	least on Linux) are O_APPEND, O_ASYNC, O_DIRECT, O_NOATIME, and O_NONBLOCK.

	See also set_fl().
 */
int clr_fl( int fd, int flags ) {
	
	int val;

	if( (val = fcntl( fd, F_GETFL, 0) ) < 0 ) 
		return -1;

	val &= ~flags;

	if( fcntl( fd, F_SETFL, val ) < 0 ) 
		return -1;

	return 0;
}

/**
	@brief Determine how lohg a string will be after printf-style formatting.
	@param format The format string.
	@param args The variable-length list of arguments
	@return If successful: the length of the string that would be created, plus 1 for
		a terminal nul, plus another 1, presumably to be on the safe side.  If unsuccessful
		due to a formatting error: 1.

	WARNINGS: the first parameter is not checked for NULL.  The return value in case of an
	error is not obviously sensible.
*/
long va_list_size(const char* format, va_list args) {
	int len = 0;
	len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	return len + 2;
}


/**
	@brief Format a printf-style string into a newly allocated buffer.
	@param format The format string.  Subsequent parameters, if any, will be
		formatted and inserted into the resulting string.
	@return A pointer to the string so created.

	The calling code is responsible for freeing the string.
*/
char* va_list_to_string(const char* format, ...) {

	long len = 0;
	va_list args;
	va_list a_copy;

	va_copy(a_copy, args);
	va_start(args, format);

	char* buf = safe_malloc( va_list_size(format, args) );
	*buf = '\0';

	va_start(a_copy, format);
	vsnprintf(buf, len - 1, format, a_copy);
	va_end(a_copy);
	return buf;
}

// ---------------------------------------------------------------------------------
// Flesh out a ubiqitous growing string buffer
// ---------------------------------------------------------------------------------

/**
	@brief Create a growing_buffer containing an empty string.
	@param num_initial_bytes The initial size of the internal buffer, not counting the
		terminal nul.
	@return A pointer to the newly created growing_buffer.

	The value of num_initial_bytes should typically be a plausible guess of how big
	the string will ever be.  However the guess doesn't have to accurate, because more
	memory will be allocated as needed.

	The calling code is responsible for freeing the growing_buffer by calling buffer_free()
	or buffer_release().
*/
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


/**
	@brief Allocate more memory for a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@param total_len How much total memory we need for the buffer.
	@return 0 if successful, or 1 if not.

	This function fails if it is asked to allocate BUFFER_MAX_SIZE
	or more bytes.
*/
static int buffer_expand( growing_buffer* gb, size_t total_len ) {

	// We do not check to see if the buffer is already big enough.  It is the
	//responsibility of the calling function to call this only when necessary.

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


/**
	@brief Append a formatted string to a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@param format A printf-style format string.  Subsequent parameters, if any, will be
		formatted and inserted into the resulting string.
	@return If successful,the length of the resulting string; otherwise -1.

	This function fails if either of the first two parameters is NULL,
	or if the resulting string requires BUFFER_MAX_SIZE or more bytes.
*/
int buffer_fadd(growing_buffer* gb, const char* format, ... ) {

	if(!gb || !format) return -1; 

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


/**
	@brief Appends a string to a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@param data A pointer to the string to be appended.
	@return If successful, the length of the resulting string; or if not, -1.
*/
int buffer_add(growing_buffer* gb, const char* data) {
	if(!(gb && data)) return -1;

	int data_len = strlen( data );

	if(data_len == 0) return gb->n_used;

	int total_len = data_len + gb->n_used;

	if( total_len >= gb->size ) {
		if( buffer_expand( gb, total_len ) )
			return -1;
	}

	strcpy( gb->buf + gb->n_used, data );
	gb->n_used = total_len;
	return total_len;
}

/**
	@brief Append a specified number of characters to a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@param data A pointer to the characters to be appended.
	@param n How many characters to be append.
	@return If sccessful, the length of the resulting string; or if not, -1.

	If the characters to be appended include an embedded nul byte, it will be appended
	along with the others.  The results are likely to be unpleasant.
*/
int buffer_add_n(growing_buffer* gb, const char* data, size_t n) {
	if(!(gb && data)) return -1;

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


/**
	@brief Reset a growing_buffer so that it contains an empty string.
	@param gb A pointer to the growing_buffer.
	@return 0 if successful, -1 if not.
*/
int buffer_reset( growing_buffer *gb){
	if( gb == NULL ) { return -1; }
	if( gb->buf == NULL ) { return -1; }
	osrf_clearbuf( gb->buf, gb->size );
	gb->n_used = 0;
	gb->buf[ 0 ] = '\0';
	return gb->n_used;
}

/**
	@brief Free a growing_buffer and return a pointer to the string inside.
	@param gb A pointer to the growing_buffer.
	@return A pointer to the string previously contained by the growing buffer.

	The calling code is responsible for freeing the string.

	This function is equivalent to buffer_data() followed by buffer_free().  However
	it is more efficient, because it avoids calls to strudup and free().
*/
char* buffer_release( growing_buffer* gb) {
	char* s = gb->buf;
	s[gb->n_used] = '\0';
	free( gb );
	return s;
}

/**
	@brief Free a growing_buffer and its contents.
	@param gb A pointer to the growing_buffer.
	@return 1 if successful, or 0 if not (because the input parameter is NULL).
*/
int buffer_free( growing_buffer* gb ) {
	if( gb == NULL )
		return 0;
	free( gb->buf );
	free( gb );
	return 1;
}

/**
	@brief Create a copy of the string inside a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@return A pointer to the newly created copy.

	The growing_buffer itself is not affected.

	The calling code is responsible for freeing the string.
*/
char* buffer_data( const growing_buffer *gb) {
	return strdup( gb->buf );
}

/**
	@brief Remove the last character from a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@return The character removed (or a nul byte if the string is already empty).
*/
char buffer_chomp(growing_buffer* gb) {
	char c = '\0';
    if(gb && gb->n_used > 0) {
	    gb->n_used--;
		c = gb->buf[gb->n_used];
	    gb->buf[gb->n_used] = '\0';
    }
    return c;
}


/**
	@brief Append a single character to a growing_buffer.
	@param gb A pointer to the growing_buffer.
	@param c The character to be appended.
	@return The length of the resulting string.

	If the character appended is a nul byte it will still be appended as if
	it were a normal character.  The results are likely to be unpleasant.
*/
int buffer_add_char(growing_buffer* gb, char c ) {
	if(gb && gb->buf) {

		int total_len = gb->n_used + 1;

		if( total_len >= gb->size ) {
			if( buffer_expand( gb, total_len ) )
				return -1;
		}
	
		gb->buf[ gb->n_used ]   = c;
		gb->buf[ ++gb->n_used ] = '\0';
		return gb->n_used;
	} else
		return 0;
}


/**
	@brief Translate a UTF8 string into escaped ASCII, suitable for JSON.
	@param string The input string to be translated.
	@param size The length of the input string (need not be accurate).
	@param full_escape Boolean; true turns on the escaping of certain
		special characters.
	@return A pointer to the translated version of the string.

	Deprecated.  Use buffer_append_utf8() instead.

	If full_escape is non-zero, the translation will escape certain
	certain characters with a backslash, according to the conventions
	used in C and other languages: quotation marks, bell characters,
	form feeds, horizontal tabs, carriage returns, line feeds, and
	backslashes.  A character with a numerical value less than 32, and
	not one of the special characters mentioned above, will be
	translated to a backslash followed by four hexadecimal characters.

	If full_escape is zero, the translation will (incorrectly) leave
	these characters unescaped and unchanged.

	The calling code is responsible for freeing the returned string.
*/
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


/**
	@brief Become a proper daemon.
	@return 0 if successful, or -1 if not.

	Call fork().  The parent exits.  The child moves to the root directory, detaches from
	the terminal, and redirects the standard streams (stdin, stdout, stderr) to /dev/null.
*/
int daemonize( void ) {
	return daemonize_write_pid( NULL );
}
/**
	@brief Become a proper daemon, and report the childs process ID.
	@return 0 if successful, or -1 if not.

	Call fork().  If pidfile is not NULL, the parent writes the process ID of the child
	process to the specified file.  Then it exits.  The child moves to the root
	directory, detaches from the terminal, and redirects the standard streams (stdin,
	stdout, stderr) to /dev/null.
 */
int daemonize_write_pid( FILE* pidfile ) {
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
		if( pidfile ) {
			fprintf( pidfile, "%ld\n", (long) f );
			fclose( pidfile );
		}
		_exit(0);
	}
}


/**
	@brief Determine whether a string represents a decimal integer.
	@param s A pointer to the string.
	@return 1 if the string represents a decimal integer, or 0 if it
	doesn't.

	To qualify as a decimal integer, the string must consist entirely
	of optional leading white space, an optional leading sign, and
	one or more decimal digits.  In addition, the number must be
	representable as a long.
*/
int stringisnum(const char* s) {
	char* w;
	strtol(s, &w, 10);
	return *w ? 0 : 1;
}
	


/**
	@brief Translate a printf-style formatted string into an MD5 message digest.
	@param text The format string.  Subsequent parameters, if any, provide values to be
		formatted and inserted into the format string.
	@return A pointer to a string of 32 hexadecimal characters.

	The calling code is responsible for freeing the returned string.

	This function is a wrapper for some public domain routines written by David Madore,
	Ron Rivest, and Colin Plumb.
*/
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
	char final[ 1 + 2 * sizeof( digest ) ];
	final[0] = '\0';

	for ( i=0 ; i<16 ; i++ ) {
		snprintf(buf, sizeof(buf), "%02x", digest[i]);
		strcat( final, buf );
	}

	return strdup(final);

}

/**
	@brief Determine whether a given file descriptor is valid.
	@param fd The file descriptor to be checked.
	@return 0 if the file descriptor is valid, or -1 if it isn't.

	The most likely reason a file descriptor would be invalid is if it isn't open.
*/
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

