#ifndef _DHT_H_
#define _DHT_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "lock_if.h"

#define false 0
#define true 1

#define CACHE_LINE_SIZE 64
#define ENTRIES_PER_BUCKET 6

typedef union {
    uint64_t key;
    void * value;
} entry;

typedef struct bucket_s {
    int bucket_id;
    entry **entries;
    struct bucket_s *next;
} bucket_t;

struct hashtable_s {
	int capacity;
    volatile global_data the_locks;
	__attribute__((aligned(CACHE_LINE_SIZE))) bucket_t **table;
};

typedef struct hashtable_s hashtable_t;

/* Create a new hashtable. */
hashtable_t *ht_create( int capacity );

/* Hash a key for a particular hashtable. */
int ht_hash( hashtable_t *hashtable, uint64_t key );

/* Insert a key-value pair into a hashtable. */
int ht_put( hashtable_t *hashtable, uint64_t key, void *value, int bin, int payload_size );

/* Retrieve a key-value pair from a hashtable. */
void * ht_get( hashtable_t *hashtable, uint64_t key, int bin);

/* Remove a key-value pair from a hashtable. */
int ht_remove( hashtable_t *hashtable, uint64_t key, int bin);

/* Dealloc the hashtable */
void ht_destroy( hashtable_t *hashtable);

int ht_size( hashtable_t *hashtable, int capacity);

#endif /* _DHT_H_ */
