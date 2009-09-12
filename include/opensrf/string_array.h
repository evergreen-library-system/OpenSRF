/**
	@file string_array.h
	@brief Header for osrfStringArray, a vector of strings.

	An osrfStringArray manages an array of character pointers pointing to nul-terminated
	strings.  New entries are added at the end.  When a string is removed, entries above
	it are shifted down to fill in the gap.
*/

#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#include <stdio.h>

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_list.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@brief Maximum number of strings in an osrfStringArray.

	This ostensible limit is not enforced.  If you exceed it, you'll get an error message,
	but only as a slap on the wrist.  You'll still get your big list of strings.
*/
#define STRING_ARRAY_MAX_SIZE 4096

/** @brief Macro version of osrfStringArrayFree() */
#define OSRF_STRING_ARRAY_FREE(arr) osrfListFree( (osrfList*) (arr) )

/**
	@brief Structure of an osrfStringArray.
*/
typedef struct {
	/** @brief The underlying container of pointers. */
    osrfList list;
	/** @brief The number of strings stored. */
	int size;    // redundant with list.size
} osrfStringArray;

osrfStringArray* osrfNewStringArray( int size );

void osrfStringArrayAdd( osrfStringArray*, const char* str );

char* osrfStringArrayGetString( osrfStringArray* arr, int index );

int osrfStringArrayContains(
	const osrfStringArray* arr, const char* string );

void osrfStringArrayFree( osrfStringArray* );

void osrfStringArrayRemove( osrfStringArray* arr, const char* str );

osrfStringArray* osrfStringArrayTokenize( const char* src, char delim );

#ifdef __cplusplus
}
#endif

#endif
