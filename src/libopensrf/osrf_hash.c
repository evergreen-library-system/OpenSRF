/**
	@file osrf_hash.c
	@brief A hybrid between a hash table and a doubly linked list.
*/

/*
Copyright (C) 2007, 2008  Georgia Public Library Service
Bill Erickson <erickson@esilibrary.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <opensrf/osrf_hash.h>

/**
	@brief A node storing a single item within an osrfHash.
*/
struct _osrfHashNodeStruct {
	/** @brief String containing the key for the item */
	char* key;
	/** @brief Pointer to the stored item data */
	void* item;
	/** @brief Pointer to the next node in a doubly linked list */
	struct _osrfHashNodeStruct* prev;
	/** @brief Pointer to the previous node in a doubly linked list */
	struct _osrfHashNodeStruct* next;
};
typedef struct _osrfHashNodeStruct osrfHashNode;

/**
	@brief osrfHash structure

	An osrfHash is partly a key/value store based on a hash table, and partly a linear
	structure implemented as a linked list.

	The hash table is an array of pointers, managed by an osrfList.  Each pointer in that array
	points to another layer of osrfList, which stores pointers to osrfHashNodes for keys that
	hash to the same value.

	Besides residing in this hash table structure, each osrfHashNode resides in a doubly
	linked list that includes all the osrfHashNodes in the osrfHash (except for any nodes
	that have been logically deleted).  New nodes are added to the end of the list.

	The hash table supports lookups based on a key.

	The linked list supports sequential traversal, reflecting the sequence in which the nodes
	were added.
*/
struct _osrfHashStruct {
	/** @brief Pointer to the osrfList used as a hash table */
	osrfList* hash;
	/** @brief Callback function for freeing stored items */
	void (*freeItem) (char* key, void* item);
	/** @brief How many items are in the osrfHash */
	unsigned int size;
	/** @brief Pointer to the first node in the linked list */
	osrfHashNode* first_key;
	/** @brief Pointer to the last node in the linked list */
	osrfHashNode* last_key;
};

/**
	@brief Maintains a position in an osrfHash, for traversing the linked list
*/
struct _osrfHashIteratorStruct {
	/** @brief Pointer to the associated osrfHash */
	osrfHash* hash;
	/** @brief Pointer to the current osrfHashNode (the one previously returned, if any) */
	osrfHashNode* curr_node;
};

/**
	@brief How many slots in the hash table.

	Must be a power of 2, or the hashing algorithm won't work properly.
*/
/* 0x100 is a good size for small hashes */
//#define OSRF_HASH_LIST_SIZE 0x100  /* size of the main hash list */
#define OSRF_HASH_LIST_SIZE 0x10  /* size of the main hash list */


/* used internally */
/**
	@brief Free an osrfHashNode, and its cargo, from within a given osrfHash.
	@param h Pointer to the osrfHash
	@param n Pointer to the osrfHashNode

	If there is a callback function for freeing the item, call it.

	We use this macro only when freeing an entire osrfHash.
*/
#define OSRF_HASH_NODE_FREE(h, n) \
	if(h && n) { \
		if(h->freeItem && n->key) h->freeItem(n->key, n->item);\
		free(n->key); free(n); \
}

/**
	@brief Create and initialize a new (and empty) osrfHash.
	@return Pointer to the newly created osrfHash.

	The calling code is responsible for freeing the osrfHash.
*/
osrfHash* osrfNewHash() {
	osrfHash* hash;
	OSRF_MALLOC(hash, sizeof(osrfHash));
	hash->hash		= osrfNewListSize( OSRF_HASH_LIST_SIZE );
	hash->first_key = NULL;
	hash->last_key  = NULL;
	return hash;
}

static osrfHashNode* osrfNewHashNode(char* key, void* item);

/*
static unsigned int osrfHashMakeKey(char* str) {
	if(!str) return 0;
	unsigned int len = strlen(str);
	unsigned int h = len;
	unsigned int i = 0;
	for(i = 0; i < len; str++, i++)
		h = ((h << 5) ^ (h >> 27)) ^ (*str);
	return (h & (OSRF_HASH_LIST_SIZE-1));
}
*/

/* macro version of the above function */
/**
	@brief Hashing algorithm: derive a mangled number from a string.
	@param str Pointer to the string to be hashed.
	@param num A numeric variable to receive the resulting value.

	This macro implements an algorithm proposed by Donald E. Knuth
	in The Art of Computer Programming Volume 3 (more or less..)
*/
#define OSRF_HASH_MAKE_KEY(str,num) \
   do {\
      const char* k__ = str;\
      unsigned int len__ = strlen(k__); \
      unsigned int h__ = len__;\
      unsigned int i__ = 0;\
      for(i__ = 0; i__ < len__; k__++, i__++)\
         h__ = ((h__ << 5) ^ (h__ >> 27)) ^ (*k__);\
      num = (h__ & (OSRF_HASH_LIST_SIZE-1));\
   } while(0)

/**
	@brief Install a callback function for freeing a stored item.
	@param hash The osrfHash in which the callback function is to be installed.
	@param callback A function pointer.

	The callback function must accept a key value and a void pointer, and return void.  The
	key value enables the callback function to treat different kinds of items differently,
	provided that it can distinguish them by key.
*/
void osrfHashSetCallback( osrfHash* hash, void (*callback) (char* key, void* item) )
{
	if( hash ) hash->freeItem = callback;
}

/* Returns a pointer to the item's node if found; otherwise returns NULL. */
/**
	@brief Search for a given key in an osrfHash.
	@param hash Pointer to the osrfHash.
	@param key The key to be sought.
	@param bucketkey Pointer through which we can report which slot the item belongs in.
	@return A pointer to the osrfHashNode where the item resides; or NULL, if it isn't there.

	If the bucketkey parameter is not NULL, we update the index of the hash bucket where the
	item belongs (whether or not we actually found it).  In some cases this feedback enables the
	calling function to avoid hashing the same key twice.
*/
static osrfHashNode* find_item( const osrfHash* hash,
		const char* key, unsigned int* bucketkey ) {

	// Find the sub-list in the hash table

	if( hash->size < 6 && !bucketkey )
	{
		// For only a few entries, when we don't need to identify
		// the hash bucket, it's probably faster to search the
		// linked list instead of hashing

		osrfHashNode* currnode = hash->first_key;
		while( currnode && strcmp( currnode->key, key ) )
			 currnode = currnode->next;

		return currnode;
	}

	unsigned int i = 0;
	OSRF_HASH_MAKE_KEY(key,i);

	// If asked, report which slot the key hashes to
	if( bucketkey ) *bucketkey = i;

	osrfList* list = OSRF_LIST_GET_INDEX( hash->hash, i );
	if( !list ) { return NULL; }

	// Search the sub-list

	int k;
	osrfHashNode* node = NULL;
	for( k = 0; k < list->size; k++ ) {
		node = OSRF_LIST_GET_INDEX(list, k);
		if( node && node->key && !strcmp(node->key, key) )
			return node;
	}

	return NULL;
}

/**
	@brief Create and populate a new osrfHashNode.
	@param key The key string.
	@param item A pointer to the item associated with the key.
	@return A pointer to the newly created node.
*/
static osrfHashNode* osrfNewHashNode(char* key, void* item) {
	if(!(key && item)) return NULL;
	osrfHashNode* n;
	OSRF_MALLOC(n, sizeof(osrfHashNode));
	n->key = strdup(key);
	n->item = item;
	n->prev = NULL;
	n->prev = NULL;
	return n;
}

/**
	@brief Store an item for a given key in an osrfHash.
	@param hash Pointer to the osrfHash in which the item is to be stored.
	@param item Pointer to the item to be stored.
	@param key A printf-style format string to be expanded into the key for the item.  Subsequent
		parameters, if any, will be formatted and inserted into the expanded key.
	@return Pointer to an item previously stored for the same key, if any (see discussion).

	If no item is already stored for the same key, osrfHashSet creates a new entry for it,
	and returns NULL.

	If an item is already stored for the same key, osrfHashSet updates the entry in place.  The
	fate of the previously stored item varies.  If there is a callback defined for freeing the
	item, osrfHashSet calls it, and returns NULL.  Otherwise it returns a pointer to the
	previously stored item, so that the calling function can dispose of it.

	osrfHashSet returns NULL if any of its first three parameters is NULL.
*/
void* osrfHashSet( osrfHash* hash, void* item, const char* key, ... ) {
	if(!(hash && item && key )) return NULL;

	unsigned int bucketkey;

	VA_LIST_TO_STRING(key);
	osrfHashNode* node = find_item( hash, VA_BUF, &bucketkey );
	if( node ) {

		// We already have an item for this key.  Update it in place.
		void* olditem = NULL;

		if( hash->freeItem ) {
			hash->freeItem( node->key, node->item );
		}
		else
			olditem = node->item;

		node->item = item;
		return olditem;
	}

	// There is no entry for this key.  Create a new one.
	osrfList* bucket;
	if( !(bucket = OSRF_LIST_GET_INDEX(hash->hash, bucketkey)) ) {
		bucket = osrfNewList();
		osrfListSet( hash->hash, bucket, bucketkey );
	}

	node = osrfNewHashNode(VA_BUF, item);
	osrfListPushFirst( bucket, node );

	hash->size++;

	// Add the new hash node to the end of the linked list

	if( NULL == hash->first_key )
		hash->first_key = hash->last_key = node;
	else {
		node->prev = hash->last_key;
		hash->last_key->next = node;
		hash->last_key = node;
	}

	return NULL;
}

/* Delete the entry for a specified key.  If the entry exists,
   and there is no callback function to destroy the associated
   item, return a pointer to the formerly associated item.
   Otherwise return NULL.
*/
/**
	@brief Remove the item for a specified key from an osrfHash.
	@param hash Pointer to the osrfHash from which the item is to be removed.
	@param key A printf-style format string to be expanded into the key for the item.  Subsequent
		parameters, if any, will be formatted and inserted into the expanded key.
	@return Pointer to the removed item, if any (see discussion).

	If no entry is present for the specified key, osrfHashRemove returns NULL.

	If there is such an entry, osrfHashRemove removes it.  The fate of the associated item
	varies.  If there is a callback function for freeing items, osrfHashRemove calls it, and
	returns NULL.  Otherwise it returns a pointer to the removed item, so that the calling
	code can dispose of it.

	osrfHashRemove returns NULL if either of its first two parameters is NULL.

	Note: the osrfHashNode for the removed item is logically deleted so that subsequent searches
	and traversals will ignore it.  However it is physically left in place so that an
	osrfHashIterator pointing to it can advance to the next node.  The downside of this
	design is that, if a lot of items are removed, the accumulation of logically deleted nodes
	will slow down searches.  In addition, the memory used by a logically deleted osrfHashNode
	remains allocated until the entire osrfHashNode is freed.
*/
void* osrfHashRemove( osrfHash* hash, const char* key, ... ) {
	if(!(hash && key )) return NULL;

	VA_LIST_TO_STRING(key);

	osrfHashNode* node = find_item( hash, VA_BUF, NULL );
	if( !node ) return NULL;

	hash->size--;

	void* item = NULL;  // to be returned
	if( hash->freeItem )
		hash->freeItem( node->key, node->item );
	else
		item = node->item;

	// Mark the node as logically deleted

	free(node->key);
	node->key = NULL;
	node->item = NULL;

	// Make the node unreachable from the rest of the linked list.
	// We leave the next and prev pointers in place so that an
	// iterator parked here can find its way to an adjacent node.

	if( node->prev )
		node->prev->next = node->next;
	else
		hash->first_key = node->next;

	if( node->next )
		node->next->prev = node->prev;
	else
		hash->last_key = node->prev;

	return item;
}


/**
	@brief Fetch the item stored in an osrfHash for a given key.
	@param hash Pointer to the osrfHash from which to fetch the item.
	@param key The key for the item sought.
	@return A pointer to the item, if it exists; otherwise NULL.
*/
void* osrfHashGet( osrfHash* hash, const char* key ) {
	if(!(hash && key )) return NULL;

	osrfHashNode* node = find_item( hash, key, NULL );
	if( !node ) return NULL;
	return node->item;
}

/**
	@brief Fetch the item stored in an osrfHash for a given key.
	@param hash Pointer to the osrfHash from which to fetch the item.
	@param key A printf-style format string to be expanded into the key for the item.  Subsequent
		parameters, if any, will be formatted and inserted into the expanded key.
	@return A pointer to the item, if it exists; otherwise NULL.
 */
void* osrfHashGetFmt( osrfHash* hash, const char* key, ... ) {
	if(!(hash && key )) return NULL;
	VA_LIST_TO_STRING(key);

	osrfHashNode* node = find_item( hash, (char*) VA_BUF, NULL );
	if( !node ) return NULL;
	return node->item;
}

/**
	@brief Create an osrfStringArray containing all the keys in an osrfHash.
	@param hash Pointer to the osrfHash whose keys are to be extracted.
	@return A pointer to the newly created osrfStringArray.

	The strings in the osrfStringArray are arranged in the sequence in which they were added to
	the osrfHash.

	The calling code is responsible for freeing the osrfStringArray by calling
	osrfStringArrayFree().

	osrfHashKeys returns NULL if its parameter is NULL.
*/
osrfStringArray* osrfHashKeys( osrfHash* hash ) {
	if(!hash) return NULL;

	osrfHashNode* node;
	osrfStringArray* strings = osrfNewStringArray( hash->size );

	// Add every key on the linked list

	node = hash->first_key;
	while( node ) {
		if( node->key )  // should always be true
			osrfStringArrayAdd( strings, node->key );
		node = node->next;
	}

	return strings;
}


/**
	@brief Count the items an osrfHash.
	@param hash Pointer to the osrfHash whose items are to be counted.
	@return The number of items in the osrfHash.

	If the parameter is NULL, osrfHashGetCount returns -1.
*/
unsigned long osrfHashGetCount( osrfHash* hash ) {
	if(!hash) return -1;
	return hash->size;
}

/**
	@brief Free an osrfHash and all of its contents.
	@param hash Pointer to the osrfHash to be freed.

	If a callback function has been defined for freeing items, osrfHashFree calls it for
		each stored item.
*/
void osrfHashFree( osrfHash* hash ) {
	if(!hash) return;

	int i, j;
	osrfList* list;
	osrfHashNode* node;

	for( i = 0; i != hash->hash->size; i++ ) {
		if( ( list = OSRF_LIST_GET_INDEX( hash->hash, i )) ) {
			for( j = 0; j != list->size; j++ ) {
				if( (node = OSRF_LIST_GET_INDEX( list, j )) ) {
					OSRF_HASH_NODE_FREE(hash, node);
				}
			}
			osrfListFree(list);
		}
	}

	osrfListFree(hash->hash);
	free(hash);
}

/**
	@brief Create and initialize an osrfHashIterator for a given osrfHash.
	@param hash Pointer to the osrfHash with which the iterator will be associated.
	@return Pointer to the newly created osrfHashIterator.

	An osrfHashIterator may be used to traverse the items stored in an osrfHash.

	The calling code is responsible for freeing the osrfHashIterator by calling
	osrfHashIteratorFree().
*/
osrfHashIterator* osrfNewHashIterator( osrfHash* hash ) {
	if(!hash) return NULL;
	osrfHashIterator* itr;
	OSRF_MALLOC(itr, sizeof(osrfHashIterator));
	itr->hash = hash;
	itr->curr_node = NULL;
	return itr;
}

/**
	@brief Advance an osrfHashIterator to the next item in an osrfHash.
	@param itr Pointer to the osrfHashIterator to be advanced.
	@return A Pointer to the next stored item, if there is one; otherwise NULL.

	The items are returned in the sequence in which their keys were added to the osrfHash.
*/
void* osrfHashIteratorNext( osrfHashIterator* itr ) {
	if(!(itr && itr->hash)) return NULL;

	// Advance to the next node in the linked list

	if( NULL == itr->curr_node )
		itr->curr_node = itr->hash->first_key;
	else
		itr->curr_node = itr->curr_node->next;

	if( itr->curr_node )
		return itr->curr_node->item;
	else
		return NULL;
}

/**
	@brief Fetch the key where an osrfHashIterator is currently positioned.
	@param itr Pointer to the osrfHashIterator.
	@return A const pointer to the key, if the iterator is currently positioned on an item;
		otherwise NULL.

	The key returned is the one associated with the previous call to osrfHashIteratorNext(),
	unless there has been a call to osrfHashIteratorReset() in the meanwhile.
*/
const char* osrfHashIteratorKey( const osrfHashIterator* itr ) {
	if( itr && itr->curr_node )
		return itr->curr_node->key;
	else
		return NULL;
}

/**
	@brief Free an osrfHashIterator.
	@param itr Pointer to the osrfHashIterator to be freed.
*/
void osrfHashIteratorFree( osrfHashIterator* itr ) {
	if(itr)
		free(itr);
}

/**
	@brief Restore an osrfHashIterator to its original pristine state.
	@param itr Pointer to the osrfHashIterator to be reset.

	After resetting the iterator, the next call to osrfHashIteratorNext() will return a pointer
	to the first item in the list.
*/
void osrfHashIteratorReset( osrfHashIterator* itr ) {
	if(!itr) return;
	itr->curr_node = NULL;
}


/**
	@brief Determine whether there is another entry in an osrfHash beyond the current
		position of an osrfHashIterator.
	@param itr Pointer to the osrfHashIterator in question.
	@return A boolean: 1 if there is another entry beyond the current position, or 0 if the
		iterator is already at the last entry (or at no entry).
*/
int osrfHashIteratorHasNext( osrfHashIterator* itr ) {
	if( !itr || !itr->hash )
		return 0;
	else if( itr->curr_node )
		return itr->curr_node->next ? 1 : 0;
	else
		return itr->hash->first_key ? 1 : 0;
}
