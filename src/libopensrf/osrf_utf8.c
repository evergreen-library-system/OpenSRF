/*----------------------------------------------------
 Desc    : functions and macros for processing UTF-8
 Author  : Scott McKellar
 Notes   : Translate UTF-8 text to a JSON string

 Copyright 2008 Scott McKellar
 All Rights reserved

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the
 Free Software Foundation, Inc.,
 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

Date       Change
 ---------- -------------------------------------------------
 2008/11/20 Initial creation
 2008/11/27 Emit surrogate pairs for code points > 0xFFFF
 ----------------------------------------------------------*/
#include <opensrf/utils.h>
#include <opensrf/osrf_utf8.h>

static void append_surrogate_pair(growing_buffer * buf, unsigned long code_point);
static void append_uxxxx(growing_buffer * buf, unsigned long i);

unsigned char osrf_utf8_mask_[] =
{
	193,	/* 00000000	Control character */
	193,	/* 00000001	Control character */
	193,	/* 00000010	Control character */
	193,	/* 00000011	Control character */
	193,	/* 00000100	Control character */
	193,	/* 00000101	Control character */
	193,	/* 00000110	Control character */
	193,	/* 00000111	Control character */
	193,	/* 00001000	Control character */
	193,	/* 00001001	Control character */
	193,	/* 00001010	Control character */
	193,	/* 00001011	Control character */
	193,	/* 00001100	Control character */
	193,	/* 00001101	Control character */
	193,	/* 00001110	Control character */
	193,	/* 00001111	Control character */
	193,	/* 00010000	Control character */
	193,	/* 00010001	Control character */
	193,	/* 00010010	Control character */
	193,	/* 00010011	Control character */
	193,	/* 00010100	Control character */
	193,	/* 00010101	Control character */
	193,	/* 00010110	Control character */
	193,	/* 00010111	Control character */
	193,	/* 00011000	Control character */
	193,	/* 00011001	Control character */
	193,	/* 00011010	Control character */
	193,	/* 00011011	Control character */
	193,	/* 00011100	Control character */
	193,	/* 00011101	Control character */
	193,	/* 00011110	Control character */
	193,	/* 00011111	Control character */
	194,	/* 00100000	Printable ASCII */
	194,	/* 00100001	Printable ASCII */
	194,	/* 00100010	Printable ASCII */
	194,	/* 00100011	Printable ASCII */
	194,	/* 00100100	Printable ASCII */
	194,	/* 00100101	Printable ASCII */
	194,	/* 00100110	Printable ASCII */
	194,	/* 00100111	Printable ASCII */
	194,	/* 00101000	Printable ASCII */
	194,	/* 00101001	Printable ASCII */
	194,	/* 00101010	Printable ASCII */
	194,	/* 00101011	Printable ASCII */
	194,	/* 00101100	Printable ASCII */
	194,	/* 00101101	Printable ASCII */
	194,	/* 00101110	Printable ASCII */
	194,	/* 00101111	Printable ASCII */
	194,	/* 00110000	Printable ASCII */
	194,	/* 00110001	Printable ASCII */
	194,	/* 00110010	Printable ASCII */
	194,	/* 00110011	Printable ASCII */
	194,	/* 00110100	Printable ASCII */
	194,	/* 00110101	Printable ASCII */
	194,	/* 00110110	Printable ASCII */
	194,	/* 00110111	Printable ASCII */
	194,	/* 00111000	Printable ASCII */
	194,	/* 00111001	Printable ASCII */
	194,	/* 00111010	Printable ASCII */
	194,	/* 00111011	Printable ASCII */
	194,	/* 00111100	Printable ASCII */
	194,	/* 00111101	Printable ASCII */
	194,	/* 00111110	Printable ASCII */
	194,	/* 00111111	Printable ASCII */
	194,	/* 01000000	Printable ASCII */
	194,	/* 01000001	Printable ASCII */
	194,	/* 01000010	Printable ASCII */
	194,	/* 01000011	Printable ASCII */
	194,	/* 01000100	Printable ASCII */
	194,	/* 01000101	Printable ASCII */
	194,	/* 01000110	Printable ASCII */
	194,	/* 01000111	Printable ASCII */
	194,	/* 01001000	Printable ASCII */
	194,	/* 01001001	Printable ASCII */
	194,	/* 01001010	Printable ASCII */
	194,	/* 01001011	Printable ASCII */
	194,	/* 01001100	Printable ASCII */
	194,	/* 01001101	Printable ASCII */
	194,	/* 01001110	Printable ASCII */
	194,	/* 01001111	Printable ASCII */
	194,	/* 01010000	Printable ASCII */
	194,	/* 01010001	Printable ASCII */
	194,	/* 01010010	Printable ASCII */
	194,	/* 01010011	Printable ASCII */
	194,	/* 01010100	Printable ASCII */
	194,	/* 01010101	Printable ASCII */
	194,	/* 01010110	Printable ASCII */
	194,	/* 01010111	Printable ASCII */
	194,	/* 01011000	Printable ASCII */
	194,	/* 01011001	Printable ASCII */
	194,	/* 01011010	Printable ASCII */
	194,	/* 01011011	Printable ASCII */
	194,	/* 01011100	Printable ASCII */
	194,	/* 01011101	Printable ASCII */
	194,	/* 01011110	Printable ASCII */
	194,	/* 01011111	Printable ASCII */
	194,	/* 01100000	Printable ASCII */
	194,	/* 01100001	Printable ASCII */
	194,	/* 01100010	Printable ASCII */
	194,	/* 01100011	Printable ASCII */
	194,	/* 01100100	Printable ASCII */
	194,	/* 01100101	Printable ASCII */
	194,	/* 01100110	Printable ASCII */
	194,	/* 01100111	Printable ASCII */
	194,	/* 01101000	Printable ASCII */
	194,	/* 01101001	Printable ASCII */
	194,	/* 01101010	Printable ASCII */
	194,	/* 01101011	Printable ASCII */
	194,	/* 01101100	Printable ASCII */
	194,	/* 01101101	Printable ASCII */
	194,	/* 01101110	Printable ASCII */
	194,	/* 01101111	Printable ASCII */
	194,	/* 01110000	Printable ASCII */
	194,	/* 01110001	Printable ASCII */
	194,	/* 01110010	Printable ASCII */
	194,	/* 01110011	Printable ASCII */
	194,	/* 01110100	Printable ASCII */
	194,	/* 01110101	Printable ASCII */
	194,	/* 01110110	Printable ASCII */
	194,	/* 01110111	Printable ASCII */
	194,	/* 01111000	Printable ASCII */
	194,	/* 01111001	Printable ASCII */
	194,	/* 01111010	Printable ASCII */
	194,	/* 01111011	Printable ASCII */
	194,	/* 01111100	Printable ASCII */
	194,	/* 01111101	Printable ASCII */
	194,	/* 01111110	Printable ASCII */
	193,	/* 01111111	Control character */
	132,	/* 10000000	UTFR-8 continuation */
	132,	/* 10000001	UTFR-8 continuation */
	132,	/* 10000010	UTFR-8 continuation */
	132,	/* 10000011	UTFR-8 continuation */
	132,	/* 10000100	UTFR-8 continuation */
	132,	/* 10000101	UTFR-8 continuation */
	132,	/* 10000110	UTFR-8 continuation */
	132,	/* 10000111	UTFR-8 continuation */
	132,	/* 10001000	UTFR-8 continuation */
	132,	/* 10001001	UTFR-8 continuation */
	132,	/* 10001010	UTFR-8 continuation */
	132,	/* 10001011	UTFR-8 continuation */
	132,	/* 10001100	UTFR-8 continuation */
	132,	/* 10001101	UTFR-8 continuation */
	132,	/* 10001110	UTFR-8 continuation */
	132,	/* 10001111	UTFR-8 continuation */
	132,	/* 10010000	UTFR-8 continuation */
	132,	/* 10010001	UTFR-8 continuation */
	132,	/* 10010010	UTFR-8 continuation */
	132,	/* 10010011	UTFR-8 continuation */
	132,	/* 10010100	UTFR-8 continuation */
	132,	/* 10010101	UTFR-8 continuation */
	132,	/* 10010110	UTFR-8 continuation */
	132,	/* 10010111	UTFR-8 continuation */
	132,	/* 10011000	UTFR-8 continuation */
	132,	/* 10011001	UTFR-8 continuation */
	132,	/* 10011010	UTFR-8 continuation */
	132,	/* 10011011	UTFR-8 continuation */
	132,	/* 10011100	UTFR-8 continuation */
	132,	/* 10011101	UTFR-8 continuation */
	132,	/* 10011110	UTFR-8 continuation */
	132,	/* 10011111	UTFR-8 continuation */
	132,	/* 10100000	UTFR-8 continuation */
	132,	/* 10100001	UTFR-8 continuation */
	132,	/* 10100010	UTFR-8 continuation */
	132,	/* 10100011	UTFR-8 continuation */
	132,	/* 10100100	UTFR-8 continuation */
	132,	/* 10100101	UTFR-8 continuation */
	132,	/* 10100110	UTFR-8 continuation */
	132,	/* 10100111	UTFR-8 continuation */
	132,	/* 10101000	UTFR-8 continuation */
	132,	/* 10101001	UTFR-8 continuation */
	132,	/* 10101010	UTFR-8 continuation */
	132,	/* 10101011	UTFR-8 continuation */
	132,	/* 10101100	UTFR-8 continuation */
	132,	/* 10101101	UTFR-8 continuation */
	132,	/* 10101110	UTFR-8 continuation */
	132,	/* 10101111	UTFR-8 continuation */
	132,	/* 10110000	UTFR-8 continuation */
	132,	/* 10110001	UTFR-8 continuation */
	132,	/* 10110010	UTFR-8 continuation */
	132,	/* 10110011	UTFR-8 continuation */
	132,	/* 10110100	UTFR-8 continuation */
	132,	/* 10110101	UTFR-8 continuation */
	132,	/* 10110110	UTFR-8 continuation */
	132,	/* 10110111	UTFR-8 continuation */
	132,	/* 10111000	UTFR-8 continuation */
	132,	/* 10111001	UTFR-8 continuation */
	132,	/* 10111010	UTFR-8 continuation */
	132,	/* 10111011	UTFR-8 continuation */
	132,	/* 10111100	UTFR-8 continuation */
	132,	/* 10111101	UTFR-8 continuation */
	132,	/* 10111110	UTFR-8 continuation */
	132,	/* 10111111	UTFR-8 continuation */
	0,	/* 11000000	Invalid UTF-8 */
	0,	/* 11000001	Invalid UTF-8 */
	200,	/* 11000010	Header of 2-byte character */
	200,	/* 11000011	Header of 2-byte character */
	200,	/* 11000100	Header of 2-byte character */
	200,	/* 11000101	Header of 2-byte character */
	200,	/* 11000110	Header of 2-byte character */
	200,	/* 11000111	Header of 2-byte character */
	200,	/* 11001000	Header of 2-byte character */
	200,	/* 11001001	Header of 2-byte character */
	200,	/* 11001010	Header of 2-byte character */
	200,	/* 11001011	Header of 2-byte character */
	200,	/* 11001100	Header of 2-byte character */
	200,	/* 11001101	Header of 2-byte character */
	200,	/* 11001110	Header of 2-byte character */
	200,	/* 11001111	Header of 2-byte character */
	200,	/* 11010000	Header of 2-byte character */
	200,	/* 11010001	Header of 2-byte character */
	200,	/* 11010010	Header of 2-byte character */
	200,	/* 11010011	Header of 2-byte character */
	200,	/* 11010100	Header of 2-byte character */
	200,	/* 11010101	Header of 2-byte character */
	200,	/* 11010110	Header of 2-byte character */
	200,	/* 11010111	Header of 2-byte character */
	200,	/* 11011000	Header of 2-byte character */
	200,	/* 11011001	Header of 2-byte character */
	200,	/* 11011010	Header of 2-byte character */
	200,	/* 11011011	Header of 2-byte character */
	200,	/* 11011100	Header of 2-byte character */
	200,	/* 11011101	Header of 2-byte character */
	200,	/* 11011110	Header of 2-byte character */
	200,	/* 11011111	Header of 2-byte character */
	208,	/* 11100000	Header of 3-byte character */
	208,	/* 11100001	Header of 3-byte character */
	208,	/* 11100010	Header of 3-byte character */
	208,	/* 11100011	Header of 3-byte character */
	208,	/* 11100100	Header of 3-byte character */
	208,	/* 11100101	Header of 3-byte character */
	208,	/* 11100110	Header of 3-byte character */
	208,	/* 11100111	Header of 3-byte character */
	208,	/* 11101000	Header of 3-byte character */
	208,	/* 11101001	Header of 3-byte character */
	208,	/* 11101010	Header of 3-byte character */
	208,	/* 11101011	Header of 3-byte character */
	208,	/* 11101100	Header of 3-byte character */
	208,	/* 11101101	Header of 3-byte character */
	208,	/* 11101110	Header of 3-byte character */
	208,	/* 11101111	Header of 3-byte character */
	224,	/* 11110000	Header of 4-byte character */
	224,	/* 11110001	Header of 4-byte character */
	224,	/* 11110010	Header of 4-byte character */
	224,	/* 11110011	Header of 4-byte character */
	224,	/* 11110100	Header of 4-byte character */
	0,	/* 11110101	Invalid UTF-8 */
	0,	/* 11110110	Invalid UTF-8 */
	0,	/* 11110111	Invalid UTF-8 */
	0,	/* 11111000	Invalid UTF-8 */
	0,	/* 11111001	Invalid UTF-8 */
	0,	/* 11111010	Invalid UTF-8 */
	0,	/* 11111011	Invalid UTF-8 */
	0,	/* 11111100	Invalid UTF-8 */
	0,	/* 11111101	Invalid UTF-8 */
	0,	/* 11111110	Invalid UTF-8 */
	0	/* 11111111	Invalid UTF-8 */
};

// Functions equivalent to the corresponding macros, for cases
// where you need a function pointer

int is__utf8__control( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_CONTROL;
}

int is__utf8__print( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_PRINT;
}

int is__utf8__continue( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_CONTINUE;
}

int is__utf8__2_byte( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_2_BYTE;
}

int is__utf8__3_byte( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_3_BYTE;
}

int is__utf8__4_byte( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_4_BYTE;
}

int is__utf8__sync( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_SYNC;
}

int is__utf8( int c ) {
	return osrf_utf8_mask_[ c & 0xFF ] & UTF8_VALID;
}

typedef enum {
	S_BEGIN,   // Expecting nothing in particular
	S_2_OF_2,  // Expecting second of 2-byte character
	S_2_OF_3,  // Expecting second of 3-byte-character
	S_3_OF_3,  // Expecting third of 3-byte-character
	S_2_OF_4,  // Expecting second of 4-byte character
	S_3_OF_4,  // Expecting third of 4-byte-character
	S_4_OF_4,  // Expecting fourth of 4-byte-character
	S_ERROR,   // Looking for a valid byte to resync with
	S_END      // Found a terminal nul
} utf8_state;

/**
 Translate a UTF-8 input string into properly escaped text suitable
 for a JSON string -- including escaped hex values and surrogate
 pairs  where needed.  Append the result to a growing_buffer.
*/
int buffer_append_utf8( growing_buffer* buf, const char* string ) {
	utf8_state state = S_BEGIN;
	unsigned long utf8_char = 0;
	const unsigned char* s = (unsigned char *) string;
	int i = 0;
	int rc = 0;

	do
	{
		switch( state )
		{
			case S_BEGIN :

				while( s[i] && (s[i] < 0x80) ) {    // Handle ASCII
					if( is_utf8_print( s[i] ) ) {   // Printable
						switch( s[i] )
						{
							case '"' :
							case '\\' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
							default :
								OSRF_BUFFER_ADD_CHAR( buf, s[i] );
								break;
						}
					} else if( s[i] ) {   // Control character

						switch( s[i] )    // Escape some
						{
							case '\n' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
								OSRF_BUFFER_ADD_CHAR( buf, 'n' );
								break;
							case '\t' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
								OSRF_BUFFER_ADD_CHAR( buf, 't' );
								break;
							case '\r' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
								OSRF_BUFFER_ADD_CHAR( buf, 'r' );
								break;
							case '\f' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
								OSRF_BUFFER_ADD_CHAR( buf, 'f' );
								break;
							case '\b' :
								OSRF_BUFFER_ADD_CHAR( buf, '\\' );
								OSRF_BUFFER_ADD_CHAR( buf, 'b' );
								break;
							default : {   // Format the rest in hex
								append_uxxxx(buf, s[i]);
								break;
							}
						}
					}
					++i;
				}

				// If the next byte is the first of a multibyte sequence, we zero out
				// the length bits and store the rest.
				
				if( '\0' == s[i] )
					state = S_END;
				else if( 128 > s[i] )
					state = S_BEGIN;
				else if( is_utf8_2_byte( s[i] ) ) {
					utf8_char = s[i] ^ 0xC0;
					state = S_2_OF_2;   // Expect 1 continuation byte
				} else if( is_utf8_3_byte( s[i] ) ) {
					utf8_char = s[i] ^ 0xE0;
					state = S_2_OF_3;   // Expect 2 continuation bytes
				} else if( is_utf8_4_byte( s[i] ) ) {
					utf8_char = s[i] ^ 0xF0;
					state = S_2_OF_4;   // Expect 3 continuation bytes
				} else {
					if( 0 == rc )
						rc = i;
					state = S_ERROR;
				}
				
				++i;
				break;
			case S_2_OF_2 :  //Expect second byte of 1-byte character
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					append_uxxxx(buf, utf8_char);
					state = S_BEGIN;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_2_OF_3 :
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					state = S_3_OF_3;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_3_OF_3 :
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					if(utf8_char > 0xFFFF )
						append_surrogate_pair(buf, utf8_char);
					else
						append_uxxxx(buf, utf8_char);
					state = S_BEGIN;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_2_OF_4 :
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					state = S_3_OF_4;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_3_OF_4 :
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					state = S_4_OF_4;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_4_OF_4 :
				if( is_utf8_continue( s[i] ) ) {  // Append lower 6 bits
					utf8_char = (utf8_char << 6) | (s[i] & 0x3F);
					if(utf8_char > 0xFFFF )
						append_surrogate_pair(buf, utf8_char);
					else
						append_uxxxx(buf, utf8_char);
					state = S_BEGIN;
					++i;
				} else if( '\0' == s[i] ) {  // Unexpected end of string
					if( 0 == rc )
						rc = i;
					state = S_END;
				} else {   // Non-continuation character
					if( 0 == rc )
						rc = i;
					state = S_BEGIN;
				}
				break;
			case S_ERROR :
				if( '\0' == s[i] )
					state = S_END;
				else if( is_utf8_sync( s[i] ) )
					state = S_BEGIN;  // Resume translation
				else
					++i;

				break;
			default :
				state = S_END;
				break;
		}
	} while ( state != S_END );
	
	return rc;
}

/**
 Break a code point up into two pieces, and format each piec
 in hex. as a surrogate pair.  Append the results to a growing_buffer.

 This code is loosely based on a code snippet at:
 http://www.unicode.org/faq/utf_bom.html
 It isn't obvious how, why, or whether it works.
*/
static void append_surrogate_pair(growing_buffer * buf, unsigned long code_point) {
	unsigned int hi;   // high surrogate
	unsigned int low;  // low surrogate

	hi = 0xD7C0 + (code_point >> 10);
	append_uxxxx(buf, hi);

	low = 0xDC00 + (code_point & 0x3FF);
	append_uxxxx(buf, low);
}

/**
 Format the lower 16 bits of an unsigned long in hex,
 in the format "\uxxxx" where each x is a hex digit.
 Append the result to a growing_buffer.
*/
static void append_uxxxx( growing_buffer * buf, unsigned long i ) {
	static const char hex_chars[] = "0123456789abcdef";
	char hex_buf[7] = "\\u";

	hex_buf[2] = hex_chars[ (i >> 12) & 0x000F ];
	hex_buf[3] = hex_chars[ (i >>  8) & 0x000F ];
	hex_buf[4] = hex_chars[ (i >>  4) & 0x000F ];
	hex_buf[5] = hex_chars[ i         & 0x000F ];
	hex_buf[6] = '\0';

	OSRF_BUFFER_ADD(buf, hex_buf);
}
