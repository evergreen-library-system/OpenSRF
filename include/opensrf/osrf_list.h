/**
	@file osrf_list.h
	@brief Header for osrfList, like a vector class in C

 	An osrfList manages an array of void pointers, allocating additional memory as
	needed when the array needs to grow.  Optionally, it may call a designated callback
	function as a destructor when one of the void pointers is removed, or when the
	array itself is destroyed.

	The callback function must be of type void and accept a void pointer as its parameter.
	There is no specific function for installing or uninstalling such a callback; just assign
	directly to the freeItem member of the osrfList structure.

	Unlike a typical vector class in, or example, C++, an osrfList does NOT shift items
	around to fill in the gap when you remove an item.  Items stay put.

	The use of void pointers involves the usual risk of type errors, so be careful.

	NULL pointers are a special case.  On the one hand an osrfList will happily store
	a NULL if you ask it to.  On the other hand it may overwrite a NULL pointer previously
	stored, treating it as disposable.  Conclusion: you can store NULLs in an osrfList, but
	not safely, unless you are familiar with the internal details of the implementation and
	work around them accordingly.
 */

#ifndef OSRF_LIST_H
#define OSRF_LIST_H

#include <opensrf/utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@brief Macro version of osrfListGetIndex().
	@param l A pointer to the osrfList.
	@param i The zero-based index of the requested pointer.
*/
#define OSRF_LIST_GET_INDEX(l, i) (!(l) || (i) >= (l)->size) ? NULL: (l)->arrlist[(i)]

/**
	@brief Structure for managing an array of pointers.
*/
struct _osrfListStruct {
	/** @brief How many slots are in use (possibly including some NULLs). */
	unsigned int size;
	/** @brief Callback function pointer for freeing stored items. */
	void (*freeItem) (void* item);
	/** @brief Pointer to array of void pointers. */
	void** arrlist;
	/** @brief Capacity of the currently allocated array. */
	int arrsize;
};
typedef struct _osrfListStruct osrfList;


/**
	@brief Iterator for traversing an osrfList.
*/
struct _osrfListIteratorStruct {
	/** @brief Pointer to the associated osrfList. */
	const osrfList* list;
	/** @brief Index of the next pointer to be returned by osrfListIteratorNext(). */
	unsigned int current;
};
typedef struct _osrfListIteratorStruct osrfListIterator;

osrfList* osrfNewListSize( unsigned int size );

osrfListIterator* osrfNewListIterator( const osrfList* list );

void* osrfListIteratorNext( osrfListIterator* itr );

void osrfListIteratorFree( osrfListIterator* itr );

void osrfListIteratorReset( osrfListIterator* itr );

osrfList* osrfNewList();

int osrfListPush( osrfList* list, void* item );

void* osrfListPop( osrfList* list );

void* osrfListSet( osrfList* list, void* item, unsigned int position );

void* osrfListGetIndex( const osrfList* list, unsigned int  position );

void osrfListFree( osrfList* list );

void* osrfListRemove( osrfList* list, unsigned int position );

void* osrfListExtract( osrfList* list, unsigned int position );
		
int osrfListFind( const osrfList* list, void* addr );

unsigned int osrfListGetCount( const osrfList* list );

void osrfListSetDefaultFree( osrfList* list );

int osrfListPushFirst( osrfList* list, void* item );

#ifdef __cplusplus
}
#endif

#endif
