#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#include <stdio.h>

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRING_ARRAY_MAX_SIZE 4096

#define OSRF_STRING_ARRAY_FREE(arr) osrfListFree( (osrfList*) (arr) )

typedef struct {
    osrfList list;
	int size;    // redundant with osrfList.size
} osrfStringArray;

osrfStringArray* osrfNewStringArray( int size );

void osrfStringArrayAdd( osrfStringArray*, const char* str );

char* osrfStringArrayGetString( osrfStringArray* arr, int index );

/* returns true if this array contains the given string */
int osrfStringArrayContains(
	const osrfStringArray* arr, const char* string );

void osrfStringArrayFree( osrfStringArray* );

void osrfStringArrayRemove( osrfStringArray* arr, const char* str );

/**
  Parse a string into tokens separated by a specified delimiter,
  as if by strtok() or strtok_r().  Load the tokens into an
  osrfStringArray.
  */
osrfStringArray* osrfStringArrayTokenize( const char* src, char delim );

#ifdef __cplusplus
}
#endif

#endif
