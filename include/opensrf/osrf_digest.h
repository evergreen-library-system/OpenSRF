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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2009 Equinox Software Inc.
 */

/**
	@file osrf_digest.h
	@brief Header for digest functions.

	In each case, the input is a nul-terminated character string.  The result is returned
	in a structure provided by the calling code, both in a binary buffer and in a
	nul-terminated string of hex characters encoding the same value.
*/

#ifndef OSRF_DIGEST_H
#define OSRF_DIGEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
	@brief Contains an SHA1 digest.
*/
typedef struct {
	unsigned char binary[ 20 ];  /**< Binary SHA1 digest. */
	char hex[ 41 ];              /**< Same digest, in the form of a hex string. */
} osrfSHA1Buffer;

/**
	@brief Contains an MD5 digest.
*/
typedef struct {
	unsigned char binary[ 16 ];  /**< Binary MD5 digest. */
	char hex[ 33 ];              /**< Same digest, in the form of a hex string. */
} osrfMD5Buffer;

void osrf_sha1_digest( osrfSHA1Buffer* result, const char *str );

void osrf_sha1_digest_fmt( osrfSHA1Buffer* result, const char* str, ... );

void osrf_md5_digest( osrfMD5Buffer* result, const char *str );

void osrf_md5_digest_fmt( osrfMD5Buffer* result, const char* str, ... );

#ifdef __cplusplus
}
#endif

#endif
