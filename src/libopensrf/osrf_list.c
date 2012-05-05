/**
	@file osrf_list.c
	@brief Implementation of osrfList, sort of like a vector class.
*/

#include <opensrf/osrf_list.h>
/** @brief The initial size of the array when none is specified */
#define OSRF_LIST_DEFAULT_SIZE 48 /* most opensrf lists are small... */
/** @brief How many slots to add at a time when the array grows */
#define OSRF_LIST_INC_SIZE 256
//#define OSRF_LIST_MAX_SIZE 10240

/**
	@brief  Create a new osrfList with OSRF_LIST_DEFAULT_SIZE slots.
	@return A pointer to the new osrfList.

	The calling code is responsible for freeing the osrfList by calling osrfListFree().
*/
osrfList* osrfNewList() {
	return osrfNewListSize( OSRF_LIST_DEFAULT_SIZE );
}

/**
	@brief Create a new osrfList with a specified number of slots.
	@param size How many pointers to store initially.
	@return A pointer to the new osrfList.

	The calling code is responsible for freeing the osrfList by calling osrfListFree().
*/
osrfList* osrfNewListSize( unsigned int size ) {
	osrfList* list;
	OSRF_MALLOC(list, sizeof(osrfList));
	list->size = 0;
	list->freeItem = NULL;
	if( size <= 0 ) size = 16;
	list->arrsize = size;
	OSRF_MALLOC( list->arrlist, list->arrsize * sizeof(void*) );

	// Nullify all pointers in the array

	int i;
	for( i = 0; i < list->arrsize; ++i )
		list->arrlist[ i ] = NULL;

	return list;
}


/**
	@brief Add a pointer to the end of the array.
	@param list A pointer to the osrfList
	@param item A pointer to be added to the list.
	@return Zero if successful, or -1 if the list parameter is NULL.

	The item pointer is stored one position past the last slot that might be in use.  The calling
	code should, in general, make no assumptions about where that position is.  See the discussion
	of osrfListGetCount() for the dirty details.
*/
int osrfListPush( osrfList* list, void* item ) {
	if(!(list)) return -1;
	osrfListSet( list, item, list->size );
	return 0;
}

/**
	@brief Store a pointer in the first unoccupied slot.
	@param list A pointer to the osrfList.
	@param item The pointer to be stored.
	@return If successful, the number of slots currently in use, or -1 if either of the
		parameters is NULL.

	Find the lowest-numbered position holding a NULL and overwrite it with the specified
	item pointer.

	The meaning of the return value (other than -1) is fuzzy and probably not useful,
	because some of the slots counted as "in use" may in fact hold NULLs.
*/
int osrfListPushFirst( osrfList* list, void* item ) {
	if(!(list && item)) return -1;
	int i;
	for( i = 0; i < list->size; i++ )
		if(!list->arrlist[i]) break;
	osrfListSet( list, item, i );
	return list->size;
}

/**
	@brief Store a pointer at a specified position in an osrfList.
	@param list A pointer to the osrfList.
	@param item The pointer to be stored.
	@param position Where to store it (a zero-based subscript).
	@return A pointer to the previous occupant of the specified slot, if any, or NULL if
		it was unoccupied, or if we have freed the occupant.

	If the pointer previously stored in the specified slot is not NULL, then the behavior
	depends on whether the calling code has provided a callback function for freeing stored
	items.  If there is such a callback function, we call it for the previously stored
	pointer, and return NULL.  Otherwise we return the previously stored pointer, so that
	the calling code can free it if it needs to.

	If the specified position is beyond the physical bounds of the array, we replace the
	existing array with one that's big enough.  This replacement is transparent to the
	calling code.
*/
void* osrfListSet( osrfList* list, void* item, unsigned int position ) {
	if(!list) return NULL;

	int newsize = list->arrsize;

	while( position >= newsize )
		newsize += OSRF_LIST_INC_SIZE;

	if( newsize > list->arrsize ) { /* expand the list if necessary */
		void** newarr;
		OSRF_MALLOC(newarr, newsize * sizeof(void*));

		// Copy the old pointers, and nullify the new ones

		int i;
		for( i = 0; i < list->arrsize; i++ )
			newarr[i] = list->arrlist[i];
		for( ; i < newsize; i++ )
			newarr[i] = NULL;
		free(list->arrlist);
		list->arrlist = newarr;
		list->arrsize = newsize;
	}

	void* olditem = osrfListRemove( list, position );
	list->arrlist[position] = item;
	if( list->size <= position ) list->size = position + 1;
	return olditem;
}


/**
	@brief Fetch the pointer stored at a specified position.
	@param list A pointer to an osrfList.
	@param position A zero-based index specifying which item to fetch.
	@return The pointer stored at the specified position, if any, or NULL.

	The contents of the osrfList are unchanged.

	If either parameter is invalid, the return value is NULL.
*/
void* osrfListGetIndex( const osrfList* list, unsigned int position ) {
	if(!list || position >= list->size) return NULL;
	return list->arrlist[position];
}

/**
	@brief Free an osrfList, and, optionally, everything in it.
	@param list A pointer to the osrfList to be freed.

	If the calling code has specified a function for freeing items, it is called for every
	non-NULL pointer in the array.
*/
void osrfListFree( osrfList* list ) {
	if(!list) return;

	if( list->freeItem ) {
		int i; void* val;
		for( i = 0; i < list->size; i++ ) {
			if( (val = list->arrlist[i]) )
				list->freeItem(val);
		}
	}

	free(list->arrlist);
	free(list);
}

/**
	@brief Make an osrfList empty.
	@param list Pointer to the osrfList to be cleared.

	Delete every item in the list.  If a callback function is defined for freeing the items,
	call it for every item.
*/
void osrfListClear( osrfList* list ) {
	if(!list) return;

	unsigned int i;
	for( i = 0; i < list->size; ++i ) {
		if( list->freeItem && list->arrlist[ i ] )
			list->freeItem( list->arrlist[ i ] );
		list->arrlist[ i ] = NULL;
	}

	list->size = 0;
}

/**
	@brief Exchange the contents of two osrfLists.
	@param one Pointer to the first osrfList.
	@param two Pointer to the second osrfList.

	After the call, the first list contains what had been the contents of the second,
	and vice versa.  This swap also works if the two parameters point to the same
	list; i.e. there is no net effect.

	If either parameter is NULL, nothing happens.
*/
void osrfListSwap( osrfList* one, osrfList* two ) {
	if( one && two ) {
		osrfList temp = *one;
		*one = *two;
		*two = temp;
	}
}

/**
	@brief Remove the pointer from a specified position.
	@param list A pointer to the osrfList.
	@param position A zero-based index identifying the pointer to be removed.
	@return A copy of the pointer removed, or NULL if the item was freed.

	Returns NULL if either parameter is invalid.  Otherwise:

	If a callback function has been installed for freeing the item to which the pointer
	points, it is called, and osrfListRemove returns NULL.  Otherwise it returns the pointer
	at the specified position in the array.  In either case it overwrites the pointer in
	the array with NULL.

	Note that other positions in the array are not affected; i.e. osrfListRemove does NOT
	shift other pointers down to fill in the hole left by the removal.
*/
void* osrfListRemove( osrfList* list, unsigned int position ) {
	if(!list || position >= list->size) return NULL;

	void* olditem = list->arrlist[position];
	list->arrlist[position] = NULL;
	if( list->freeItem ) {
		list->freeItem(olditem);
		olditem = NULL;
	}

	if( position == list->size - 1 ) list->size--;
	return olditem;
}

/**
	@brief Remove a pointer from a specified position and return it.
	@param list A pointer to the osrfList.
	@param position A zero-based subscript identifying the pointer to be extracted.
	@return The pointer at the specified position.

	This function is identical to osrfListRemove(), except that it never frees the item
	to which the pointer points, even if an item-freeing function has been designated.
*/
void* osrfListExtract( osrfList* list, unsigned int position ) {
	if(!list || position >= list->size) return NULL;

	void* olditem = list->arrlist[position];
	list->arrlist[position] = NULL;

	if( position == list->size - 1 ) list->size--;
	return olditem;
}


/**
	@brief Find the position where a given pointer is stored.
	@param list A pointer to the osrfList.
	@param addr A void pointer possibly stored in the osrfList.
	@return A zero-based index indicating where the specified pointer resides in the array,
		or -1 if no such pointer is found.

	If either parameter is NULL, osrfListFind returns -1.

	If the pointer in question is stored in more than one slot, osrfListFind returns the
		lowest applicable index.
*/
int osrfListFind( const osrfList* list, void* addr ) {
	if(!(list && addr)) return -1;
	int index;
	for( index = 0; index < list->size; index++ ) {
		if( list->arrlist[index] == addr )
			return index;
	}
	return -1;
}


/**
	@brief Return the number of slots in use.
	@param list A pointer to an osrfList.
	@return The number of slots in use.

	The concept of "in use" is highly counter-intuitive and not, in general, very useful for the
	calling code.  It is an internal optimization trick: it keeps track of how many slots *might*
	contain non-NULL pointers, not how many slots *do* contain non_NULL pointers.  It
	represents how many slots certain operations need to look at before they can safely stop.

	Extreme example: starting with an empty osrfList, call osrfListSet()
	to add a pointer at slot 15.  If you then call osrfListGetCount(),
	it returns 16, because you installed the pointer in the sixteenth slot (counting from 1),
	even though the array contains only one non-NULL pointer.

	Now call osrfListRemove() to remove the pointer you just added. If you then call
	osrfListGetCount(), it returns 15, even though all the pointers in the array are NULL,
	because osrfListRemove() simply decremented the counter from its previous value of 16.

	Conclusion: osrfListGetCount() tells you next to nothing about the contents of the array.
	The value returned reflects not only the current state of the array but also the history
	and sequence of previous operations.

	If you have changed the contents of the array only by calls to osrfListPush() and/or
	osrfListPushFirst(), thereby leaving no holes in the array at any time, then
	osrfListGetCount() will return the answer you expect.  Otherwise all bets are off.
*/
unsigned int osrfListGetCount( const osrfList* list ) {
	if(!list) return -1;
	return list->size;
}


/**
	@brief Remove the last pointer from the array.
	@param list A pointer to an osrfList.
	@return The pointer so removed, or NULL.

	As with osrfListRemove(), if a callback function has been defined, osrfListPop() calls it
	for the pointer it removes, and returns NULL.  Otherwise it returns the removed pointer.

	The concept of "last pointer" is non-intuitive.  It reflects, in part, the history and
	sequence of previous operations on the osrfList.  The pointer removed is the one with
	subscript n - 1, where n is the value returned by osrfListGetCount().  See the discussion
	of the latter function for the dirty details.

	In brief, osrfListPop() will behave in the obvious and expected way as long as you never
	create holes in the array via calls to osrfListSet(), osrfListRemove(), or osrfListExtract().
*/
void* osrfListPop( osrfList* list ) {
	if(!list) return NULL;
	return osrfListRemove( list, list->size - 1 );
}


/**
	@brief Create and initialize an osrfListIterator for a given osrfList.
	@param list A pointer to an osrfList.
	@return A pointer to the newly constructed osrfListIterator.

	The calling code is responsible for freeing the osrfListIterator by calling
		osrfListIteratorFree().
*/
osrfListIterator* osrfNewListIterator( const osrfList* list ) {
	if(!list) return NULL;
	osrfListIterator* itr;
	OSRF_MALLOC(itr, sizeof(osrfListIterator));
	itr->list = list;
	itr->current = 0;
	return itr;
}

/**
	@brief Advance an osrfListIterator to the next position in the array.
	@param itr A pointer to the osrfListIterator to be advanced.
	@return The pointer at the next position in the array; or NULL, if the osrfIterator
		is already past the end.

	The first call to osrfListIteratorNext() for a given osrfListIterator returns the first
	pointer in the array (i.e slot zero, counting from zero).  Subsequent calls return successive
	pointers from the array.  Once it has returned the last pointer, the iterator responds to
	subsequent calls by returning NULL, unless it is restored to an initial state by a call to
	osrfListIteratorReset().

	A return value of NULL does not necessarily indicate that the iterator has reached the end
	of the array.  Depending on the history and sequence of previous operations, the array may
	include NULLs that are regarded as part of the array.  See the discussions of osrfListPop()
	and osrfListGetCount().
*/
void* osrfListIteratorNext( osrfListIterator* itr ) {
	if(!(itr && itr->list)) return NULL;
	if(itr->current >= itr->list->size) return NULL;
	return itr->list->arrlist[itr->current++];
}

/**
	@brief Free an osrfListIterator.
	@param itr A pointer to the osrfListIterator to be freed.
*/
void osrfListIteratorFree( osrfListIterator* itr ) {
	if(!itr) return;
	free(itr);
}


/**
	@brief Reset an osrfListIterator to the beginning of the associated osrfList.
	@param itr A pointer to the osrfListIterator to be reset.
*/
void osrfListIteratorReset( osrfListIterator* itr ) {
	if(!itr) return;
	itr->current = 0;
}


/**
	@brief Install a default item-freeing callback function (the ANSI standard free() function).
	@param list A pointer to the osrfList where the callback function will be installed.
*/
void osrfListSetDefaultFree( osrfList* list ) {
	if(!list) return;
	list->freeItem = free;
}
