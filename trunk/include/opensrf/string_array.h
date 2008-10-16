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
typedef struct string_array_struct string_array;
typedef struct string_array_struct osrfStringArray;

osrfStringArray* init_string_array(int size);
osrfStringArray* osrfNewStringArray(int size);

void string_array_add(osrfStringArray*, char* string);
void osrfStringArrayAdd(osrfStringArray*, char* string);

char* string_array_get_string(osrfStringArray* arr, int index);
char* osrfStringArrayGetString(osrfStringArray* arr, int index);

/* returns true if this array contains the given string */
int osrfStringArrayContains( osrfStringArray* arr, char* string );


void string_array_destroy(osrfStringArray*);
void osrfStringArrayFree(osrfStringArray*);

/* total size of all included strings */
int string_array_get_total_size(osrfStringArray* arr);

void osrfStringArrayRemove( osrfStringArray* arr, char* str);

#endif
