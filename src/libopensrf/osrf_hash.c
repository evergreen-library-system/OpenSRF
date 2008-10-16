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

-----------

An osrfHash is a hybrid between a hash table and a doubly linked
list.  The hash table supports random lookups by key.  The list
supports iterative traversals.  The sequence of entries in the
list reflects the sequence in which the entries were added.

osrfHashIterators are somewhat unusual in that, if an iterator
is positioned on a given entry, deletion of that entry does
not invalidate the iterator.  The entry to which it points is
logically but not physically deleted.  You can still advance
the iterator to the next entry in the list.

*/

#include <opensrf/osrf_hash.h>

struct _osrfHashNodeStruct {
	char* key;
	void* item;
	struct _osrfHashNodeStruct* prev;
	struct _osrfHashNodeStruct* next;
};
typedef struct _osrfHashNodeStruct osrfHashNode;

struct _osrfHashStruct {
	osrfList* hash; /* this hash */
	void (*freeItem) (char* key, void* item);	/* callback for freeing stored items */
	unsigned int size;
	osrfHashNode* first_key;
	osrfHashNode* last_key;
};

struct _osrfHashIteratorStruct {
	osrfHash* hash;
	osrfHashNode* curr_node;
};

/* 0x100 is a good size for small hashes */
//#define OSRF_HASH_LIST_SIZE 0x100  /* size of the main hash list */
#define OSRF_HASH_LIST_SIZE 0x10  /* size of the main hash list */


/* used internally */
#define OSRF_HASH_NODE_FREE(h, n) \
	if(h && n) { \
		if(h->freeItem && n->key) h->freeItem(n->key, n->item);\
		free(n->key); free(n); \
}

osrfHash* osrfNewHash() {
	osrfHash* hash;
	OSRF_MALLOC(hash, sizeof(osrfHash));
	hash->hash		= osrfNewList();
	hash->first_key = NULL;
	hash->last_key  = NULL;
	return hash;
}

static osrfHashNode* osrfNewHashNode(char* key, void* item);

/* algorithm proposed by Donald E. Knuth 
 * in The Art Of Computer Programming Volume 3 (more or less..)*/
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

/* Installs a callback function for freeing stored items */
void osrfHashSetCallback( osrfHash* hash, void (*callback) (char* key, void* item) )
{
	if( hash ) hash->freeItem = callback;
}

/* Returns a pointer to the item's node if found; otherwise returns NULL. */
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

/* If an entry exists for a given key, update it; otherwise create it.
   If an entry exists, and there is no callback function to destroy it,
   return a pointer to it so that the calling code has the option of
   destroying it.  Otherwise return NULL.
*/
void* osrfHashSet( osrfHash* hash, void* item, const char* key, ... ) {
	if(!(hash && item && key )) return NULL;

	void* olditem = NULL;
	unsigned int bucketkey;
	
	VA_LIST_TO_STRING(key);
	osrfHashNode* node = find_item( hash, VA_BUF, &bucketkey );
	if( node ) {

		// We already have an item for this key.  Update it in place.
		if( hash->freeItem ) {
			hash->freeItem( node->key, node->item );
		}
		else
			olditem = node->item;

		node->item = item;
		return olditem;
	}
	
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
	
	return olditem;
}

/* Delete the entry for a specified key.  If the entry exists,
   and there is no callback function to destroy the associated
   item, return a pointer to the formerly associated item.
   Otherwise return NULL.
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


void* osrfHashGet( osrfHash* hash, const char* key, ... ) {
	if(!(hash && key )) return NULL;
	VA_LIST_TO_STRING(key);

	osrfHashNode* node = find_item( hash, (char*) VA_BUF, NULL );
	if( !node ) return NULL;
	return node->item;
}

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


unsigned long osrfHashGetCount( osrfHash* hash ) {
	if(!hash) return -1;
	return hash->size;
}

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

osrfHashIterator* osrfNewHashIterator( osrfHash* hash ) {
	if(!hash) return NULL;
	osrfHashIterator* itr;
	OSRF_MALLOC(itr, sizeof(osrfHashIterator));
	itr->hash = hash;
	itr->curr_node = NULL;
	return itr;
}

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

const char* osrfHashIteratorKey( const osrfHashIterator* itr ) {
	if( itr && itr->curr_node )
		return itr->curr_node->key;
	else
		return NULL;
}

void osrfHashIteratorFree( osrfHashIterator* itr ) {
	if(itr)
		free(itr);
}

void osrfHashIteratorReset( osrfHashIterator* itr ) {
	if(!itr) return;
	itr->curr_node = NULL;
}


int osrfHashIteratorHasNext( osrfHashIterator* itr ) {
	if( !itr )
		return 0;
	else if( itr->curr_node )
		return itr->curr_node->next ? 1 : 0;
	else
		return itr->hash->first_key ? 1 : 0;
}
