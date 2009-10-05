#ifndef OSRF_HASH_H
#define OSRF_HASH_H

/**
	@file osrf_hash.h
	@brief A hybrid between a hash table and a doubly linked list.

	The hash table supports random lookups by key.  The list supports iterative traversals.
	The sequence of entries in the list reflects the sequence in which the keys were added.

	osrfHashIterators are somewhat unusual in that, if an iterator is positioned on a given
	entry, deletion of that entry does not invalidate the iterator.  The entry to which it
	points is logically but not physically deleted.  You can still advance the iterator to the
	next entry in the list.
*/
#include <opensrf/utils.h>
#include <opensrf/string_array.h>
#include <opensrf/osrf_list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _osrfHashStruct;
typedef struct _osrfHashStruct osrfHash;

struct _osrfHashIteratorStruct;
typedef struct _osrfHashIteratorStruct osrfHashIterator;

osrfHash* osrfNewHash();

void osrfHashSetCallback( osrfHash* hash, void (*callback) (char* key, void* item) );

void* osrfHashSet( osrfHash* hash, void* item, const char* key, ... );

void* osrfHashRemove( osrfHash* hash, const char* key, ... );

void* osrfHashExtract( osrfHash* hash, const char* key, ... );

void* osrfHashGet( osrfHash* hash, const char* key );

void* osrfHashGetFmt( osrfHash* hash, const char* key, ... );

osrfStringArray* osrfHashKeys( osrfHash* hash );

void osrfHashFree( osrfHash* hash );

unsigned long osrfHashGetCount( osrfHash* hash );

osrfHashIterator* osrfNewHashIterator( osrfHash* hash );

int osrfHashIteratorHasNext( osrfHashIterator* itr );

void* osrfHashIteratorNext( osrfHashIterator* itr );

const char* osrfHashIteratorKey( const osrfHashIterator* itr );

void osrfHashIteratorFree( osrfHashIterator* itr );

void osrfHashIteratorReset( osrfHashIterator* itr );

#ifdef __cplusplus
}
#endif

#endif
