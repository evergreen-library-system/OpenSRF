#include <opensrf/string_array.h>

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

void osrfStringArrayAdd( osrfStringArray* arr, const char* string ) {
	if(arr == NULL || string == NULL ) return;
	if( arr->list.size > STRING_ARRAY_MAX_SIZE )
		osrfLogError( OSRF_LOG_MARK, "osrfStringArrayAdd size is too large" );
    osrfListPush(&arr->list, strdup(string));
    arr->size = arr->list.size;
}

char* osrfStringArrayGetString( osrfStringArray* arr, int index ) {
    if(!arr) return NULL;
	return OSRF_LIST_GET_INDEX(&arr->list, index);
}

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

void osrfStringArrayRemove( osrfStringArray* arr, const char* tstr ) {
	if(!(arr && tstr)) return;
	int i;
    char* str;

	for( i = 0; i < arr->size; i++ ) {
        /* find and remove the string */
        str = OSRF_LIST_GET_INDEX(&arr->list, i);
		if(str && !strcmp(str, tstr)) {
            osrfListRemove(&arr->list, i);
			break;
		}
	}

    /* disable automatic item freeing on delete and shift
     * items up in the array to fill in the gap
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
