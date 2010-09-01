/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (C) 2009 Jason J.A. Stephenson <jason@sigio.com>
 * Extensively modified by Scott McKellar <scott@esilibrary.com>
 * Copyright 2009 Equinox Software Inc.
 */

/**
	@file osrf_digest.c
	@brief Routines to calculate SHA1 and MD5 digests of strings.
*/

#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>
#include "opensrf/utils.h"
#include "opensrf/osrf_digest.h"

static void format_hex( char* buf, const unsigned char* s, size_t n );

/**
	@brief Calculate an SHA1 digest for a specified string.
	@param result Pointer to an osrfSHA1Buffer to receive the result.
	@param str Pointer to a nul-terminated string to be digested.
*/
void osrf_sha1_digest( osrfSHA1Buffer* result, const char *str ) {
	if( !result )
		return;

	result->hex[0] = '\0';

	if( str ) {
		size_t out_size = sizeof( result->binary ); /* SHA1 is 160 bits output. */

		gnutls_datum_t in;
		in.data = (unsigned char*) str;
		in.size = strlen( str );
		if( gnutls_fingerprint( GNUTLS_DIG_SHA1, &in, result->binary, &out_size )
				== GNUTLS_E_SUCCESS ) {
			format_hex( result->hex, result->binary, out_size );
		}
	}
}

/**
	@brief Calculate an SHA1 digest for a formatted string.
	@param result Pointer to an osrfSHA1Buffer to receive the result.
	@param str Pointer to a printf-style format string.  Subsequent arguments, if any, are
	formatted and inserted into the string to be digested.
*/
void osrf_sha1_digest_fmt( osrfSHA1Buffer* result, const char* str, ... ) {
	if( str ) {
		VA_LIST_TO_STRING( str );
		osrf_sha1_digest( result, VA_BUF );
	} else if( result )
		result->hex[0] = '\0';
}

/**
	@brief Calculate an MD5 digest for a specified string.
	@param result Pointer to an osrfMD5Buffer to receive the result.
	@param str Pointer to a nul-terminated string to be digested.
*/
void osrf_md5_digest( osrfMD5Buffer* result, const char *str )  {
	if( !result )
		return;

	result->hex[0] = '\0';

	if( str ) {
		size_t out_size = sizeof( result->binary ); /* MD5 is 128 bits output. */

		gnutls_datum_t in;
		in.data = (unsigned char*) str;
		in.size = strlen( str );
		if( gnutls_fingerprint( GNUTLS_DIG_MD5, &in, result->binary, &out_size )
				  == GNUTLS_E_SUCCESS ) {
			format_hex( result->hex, result->binary, out_size );
		}
	}
}

/**
	@brief Calculate an MD5 digest for a formatted string.
	@param result Pointer to an osrfMD5Buffer to receive the result.
	@param str Pointer to a printf-style format string.  Subsequent arguments, if any, are
	formatted and inserted into the string to be digested.
*/
void osrf_md5_digest_fmt( osrfMD5Buffer* result, const char* str, ... ) {
	if( str ) {
		VA_LIST_TO_STRING( str );
		osrf_md5_digest( result, VA_BUF );
	} else if( result )
		result->hex[0] = '\0';
}

/**
	@brief Translate a series of bytes to the corresponding hexadecimal representation.
	@param buf Pointer to the buffer that will receive the output hex characters.
	@param s Pointer to the input characters to be translated.
	@param n How many input characters to translate.

	The calling code is responsible for providing a large enough output buffer.  It should
	be twice as large as the buffer to be translated, plus one for a terminal nul.
*/
static void format_hex( char* buf, const unsigned char* s, size_t n ) {
	int i;
	for( i = 0; i < n; ++i ) {
		unsigned char c = s[i];

		// Format high nybble
		unsigned char fc = ( c >> 4 ) & 0x0F;
		fc += (fc > 9) ? 'a' - 10 : '0';
		*buf++ = fc;

		// Format low nybble
		fc = c & 0x0F;
		fc += (fc > 9) ? 'a' - 10 : '0';
		*buf++ = fc;
	}
	*buf = '\0';
}
