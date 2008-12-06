#include <stdio.h>

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/osrf_list.h>

#define STRING_ARRAY_MAX_SIZE 4096

#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#define OSRF_STRING_ARRAY_FREE(arr)\
    if(arr) {osrfListFree(arr->list); free(arr);}
        

struct string_array_struct {
    osrfList* list;
    int size;
};
typedef struct string_array_struct osrfStringArray;

osrfStringArray* osrfNewStringArray(int size);

void osrfStringArrayAdd(osrfStringArray*, char* str);

char* osrfStringArrayGetString(osrfStringArray* arr, int index);

/* returns true if this array contains the given string */
int osrfStringArrayContains( osrfStringArray* arr, char* string );

void osrfStringArrayFree(osrfStringArray*);

/* total size of all included strings */
int string_array_get_total_size(osrfStringArray* arr);

void osrfStringArrayRemove( osrfStringArray* arr, char* str);

#endif
