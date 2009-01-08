/*----------------------------------------------------
 Desc    : functions and macros for processing UTF-8
 Author  : Scott McKellar
 Notes   : 

 Copyright 2008 Scott McKellar
 All Rights reserved
 
 Date       Change
 ---------- -----------------------------------------
 2008/11/20 Initial creation
 ---------------------------------------------------*/

#ifndef OSRF_UTF8_H
#define OSRF_UTF8_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char osrf_utf8_mask_[];  // Lookup table of bitmasks

// Meanings of the various bit switches:

#define UTF8_CONTROL  0x01
#define UTF8_PRINT    0x02
#define UTF8_CONTINUE 0x04
#define UTF8_2_BYTE   0x08
#define UTF8_3_BYTE   0x10
#define UTF8_4_BYTE   0x20
#define UTF8_SYNC     0x40
#define UTF8_VALID    0x80

// macros:

#define is_utf8_control( x )  ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_CONTROL )
#define is_utf8_print( x )    ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_PRINT )
#define is_utf8_continue( x ) ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_CONTINUE )
#define is_utf8_2_byte( x )   ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_2_BYTE )
#define is_utf8_3_byte( x )   ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_3_BYTE )
#define is_utf8_4_byte( x )   ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_4_BYTE )
#define is_utf8_sync( x )     ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_SYNC )
#define is_utf8( x )          ( osrf_utf8_mask_[ (x) & 0xFF ] & UTF8_VALID )

// Equivalent functions, for when you need a function pointer

int is__utf8__control( int c );
int is__utf8__print( int c );
int is__utf8__continue( int c );
int is__utf8__2_byte( int c );
int is__utf8__3_byte( int c );
int is__utf8__4_byte( int c );
int is__utf8__sync( int c );
int is__utf8( int c );

// Translate a string, escaping as needed, and append the
// result to a growing_buffer

int buffer_append_utf8( growing_buffer* buf, const char* string );

#ifdef __cplusplus
}
#endif

#endif
