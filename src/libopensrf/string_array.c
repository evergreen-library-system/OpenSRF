/**
	@file string_array.c
	@brief Implement osrfStringArray, a vector of character strings.

	An osrfStringArray is implemented as a thin wrapper around an osrfList.  The latter is
	incorporated bodily in the osrfStringArray structure, not as a pointer, so as to avoid
	a layer of malloc() and free().

	Operations on the osrfList are restricted so as not to leave NULL pointers in the middle.
*/

#include <opensrf/string_array.h>

/**
	@brief Create and initialize an osrfStringArray.
	@param size How many strings to allow space for initially.
	@return Pointer to a newly created osrfStringArray.

	If the size parameter is zero, osrfNewStringArray uses a default value.  If the initial
	allocation isn't big enough, more space will be added as needed.

	The calling code is responsible for freeing the osrfStringArray by calling
	osrfStringArrayFree().
*/
osrfStringArray* osrfNewStringArray(int size) {
	if(size > STRING_ARRAY_MAX_SIZE)
		osrfLogError( OSRF_LOG_MARK, "osrfNewStringArray size is too large");

	osrfStringArray* arr;
	OSRF_MALLOC(arr, sizeof(osrfStringArray));
	arr->size = 0;

	// Initialize list
	arr->list.size = 0;
	arr->list.freeItem = NULL;
	if( size <= 0 )
		arr->list.arrsize = 16;
	else
		arr->list.arrsize = size;
	OSRF_MALLOC( arr->list.arrlist, arr->list.arrsize * sizeof(void*) );

	// Nullify all pointers in the array

	int i;
	for( i = 0; i < arr->list.arrsize; ++i )
		arr->list.arrlist[ i ] = NULL;

	osrfListSetDefaultFree(&arr->list);
	return arr;
}

/**
	@brief Add a string to the end of an osrfStringArray.
	@param arr Pointer to the osrfStringArray to which the string will be added.
	@param string Pointer to the character string to be added.

	The string stored is a copy; the original is not affected.

	If either parameter is NULL, nothing happens.
*/
void osrfStringArrayAdd( osrfStringArray* arr, const char* string ) {
	if(arr == NULL || string == NULL ) return;
	if( arr->list.size > STRING_ARRAY_MAX_SIZE )
		osrfLogError( OSRF_LOG_MARK, "osrfStringArrayAdd size is too large" );
    osrfListPush(&arr->list, strdup(string));
    arr->size = arr->list.size;
}

/**
	@brief Return a pointer to the string stored at a specified position in an osrfStringArray.
	@param arr Pointer to the osrfStringArray.
	@param index A zero-based index into the array
	@return A pointer to the string stored at the specified position. if it exists;
		or else NULL.
*/
const char* osrfStringArrayGetString( const osrfStringArray* arr, int index ) {
    if(!arr) return NULL;
	return OSRF_LIST_GET_INDEX(&arr->list, index);
}

/**
	@brief Render an osrfStringArray empty.
	@param arr Pointer to the osrfStringArray to be emptied.
*/
void osrfStringArrayClear( osrfStringArray* arr ) {
	if( arr ) {
		osrfListClear( &arr->list );
		arr->size = 0;
	}
}

/**
	@brief Exchange the contents of two osrfStringArrays.
	@param one Pointer to the first osrfStringArray.
	@param two Pointer to the second osrfStringArray.

	After the swap, the first osrfStringArray contains what had been the contents of the
	second, and vice versa.  The swap also works if both parameters point to the same
	osrfStringArray; i.e. there is no net change.

	If either parameter is NULL, nothing happens.
*/
void osrfStringArraySwap( osrfStringArray* one, osrfStringArray* two ) {
	if( one && two ) {
		osrfListSwap( &one->list, &two->list );
		int temp = one->size;
		one->size = two->size;
		two->size = temp;
	}
}

/**
	@brief Free an osrfStringArray, and all the strings inside it.
	@param arr Pointer to the osrfStringArray to be freed.
*/
void osrfStringArrayFree(osrfStringArray* arr) {

	// This function is a sleazy hack designed to avoid the
	// need to duplicate the code in osrfListFree().  It
	// works because:
	//
	// 1. The osrfList is the first member of an
	//    osrfStringArray.  C guarantees that a pointer
	//    to the one is also a pointer to the other.
	//
	// 2. The rest of the osrfStringArray owns no memory
	//    and requires no special attention when freeing.
	//
	// If these facts ever cease to be true, we'll have to
	// revisit this function.

	osrfListFree( (osrfList*) arr );
}

/**
	@brief Determine whether an osrfStringArray contains a specified string.
	@param arr Pointer to the osrfStringArray.
	@param string Pointer to the string to be sought.
	@return A boolean: 1 if the string is present in the osrfStringArray, or 0 if it isn't.

	The search is case-sensitive.
*/
int osrfStringArrayContains(
	const osrfStringArray* arr, const char* string ) {
	if(!(arr && string)) return 0;
	int i;
	for( i = 0; i < arr->size; i++ ) {
        char* str = OSRF_LIST_GET_INDEX(&arr->list, i);
		if(str && !strcmp(str, string))
            return 1;
	}

	return 0;
}

/**
	@brief Remove the first occurrence, if any, of a specified string from an osrfStringArray.
	@param arr Pointer to the osrfStringArray.
	@param tstr Pointer to a string to be removed.
*/
void osrfStringArrayRemove( osrfStringArray* arr, const char* tstr ) {
	if(!(arr && tstr)) return;
	int i;
    char* str;
	int removed = 0;  // boolean

	for( i = 0; i < arr->size; i++ ) {
		/* find and remove the string */
		str = OSRF_LIST_GET_INDEX(&arr->list, i);
		if(str && !strcmp(str, tstr)) {
			osrfListRemove(&arr->list, i);
			removed = 1;
			break;
		}
	}

	if( ! removed )
		return;         // Nothing was removed

    /* disable automatic item freeing on delete, and shift
     * items down in the array to fill in the gap
     */
    arr->list.freeItem = NULL;
	for( ; i < arr->size - 1; i++ )
        osrfListSet(&arr->list, OSRF_LIST_GET_INDEX(&arr->list, i+1), i);

    /* remove the last item since it was shifted up */
    osrfListRemove(&arr->list, i);

    /* re-enable automatic item freeing in delete */
    osrfListSetDefaultFree(&arr->list);
	arr->size--;
}

/**
	@brief Tokenize a string into an array of substrings separated by a specified delimiter.
	@param src A pointer to the string to be parsed.
	@param delim The delimiter character.
	@return Pointer to a newly constructed osrfStringArray containing the tokens.

	A series of consecutive delimiter characters is treated as a single separator.

	The input string is left unchanged.

	The calling code is responsible for freeing the osrfStringArray by calling
	osrfStringArrayFree().
*/
osrfStringArray* osrfStringArrayTokenize( const char* src, char delim )
{
	// Take the length so that we know how big a buffer we need,
	// in the worst case.  Estimate the number of tokens, assuming
	// 5 characters per token, and add a few for a pad.

	if( NULL == src || '\0' == *src )		// Got nothing?
		return osrfNewStringArray( 1 );		// Return empty array

	size_t src_len = strlen( src );
	size_t est_count = src_len / 6 + 5;
	int in_token = 0;     // boolean
	char buf[ src_len + 1 ];
	char* out = buf;
	osrfStringArray* arr = osrfNewStringArray( est_count );

	for( ;; ++src ) {
		if( in_token ) {	// We're building a token
			if( *src == delim ) {
				*out = '\0';
				osrfStringArrayAdd( arr, buf );
				in_token = 0;
			}
			else if( '\0' == *src ) {
				*out = '\0';
				osrfStringArrayAdd( arr, buf );
				break;
			}
			else {
				*out++ = *src;
			}
		}
		else {				// We're between tokens
			if( *src == delim ) {
				;			// skip it
			}
			else if( '\0' == *src ) {
				break;
			}
			else {
				out = buf;		// Start the next one
				*out++ = *src;
				in_token = 1;
			}
		}
	}

	return arr;
}
