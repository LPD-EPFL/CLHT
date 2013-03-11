#ifndef _DHT_H_
#define _DHT_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "lock_if.h"

#define true 1
#define false 0

#define CACHE_LINE_SIZE 64
#define ENTRIES_PER_BUCKET 6

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif


typedef uintptr_t ssht_addr_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  uint64_t empty;
  ssht_addr_t key[ENTRIES_PER_BUCKET];
  struct bucket_s *next;
  void* entry[ENTRIES_PER_BUCKET]; 
} bucket_t;

typedef struct ALIGNED(64) hashtable_s
{
  uint32_t capacity;
  volatile global_data the_locks;
  __attribute__((aligned(CACHE_LINE_SIZE))) bucket_t **table;
} hashtable_t;


/* Create a new hashtable. */
hashtable_t* ht_create(uint32_t capacity );

/* Hash a key for a particular hashtable. */
uint32_t ht_hash( hashtable_t *hashtable, uint64_t key );

/* Insert a key-value pair into a hashtable. */
uint32_t ht_put( hashtable_t *hashtable, uint64_t key, void *value, uint32_t bin);

/* Retrieve a key-value pair from a hashtable. */
void* ht_get( hashtable_t *hashtable, uint64_t key, uint32_t bin);

/* Remove a key-value pair from a hashtable. */
void*  ht_remove( hashtable_t *hashtable, uint64_t key, int bin);

/* Dealloc the hashtable */
void ht_destroy( hashtable_t *hashtable);

uint32_t ht_size( hashtable_t *hashtable, uint32_t capacity);

void ht_print(hashtable_t *hashtable, uint32_t capacity);

bucket_t* create_bucket();


#endif /* _DHT_H_ */
