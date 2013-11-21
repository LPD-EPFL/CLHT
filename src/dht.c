#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#ifdef __sparc__
#include "../include/dht.h"
#else
#include "dht.h"
#endif

#ifdef DEBUG
__thread uint32_t put_num_restarts = 0;
__thread uint32_t put_num_failed_expand = 0;
__thread uint32_t put_num_failed_on_new = 0;
#endif

#include "stdlib.h"
#include "assert.h"

inline int
is_power_of_two (unsigned int x) 
{
return ((x != 0) && !(x & (x - 1)));
}

static inline
int is_odd (int x)
{
    return x & 1;
}

/** Jenkins' hash function for 64-bit integers. */
inline uint64_t
__ac_Jenkins_hash_64(uint64_t key)
{
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return key;
}

/* Create a new bucket. */
bucket_t*
create_bucket() 
{
  bucket_t* bucket = NULL;
  bucket = memalign(CACHE_LINE_SIZE, sizeof(bucket_t));
  /* bucket = malloc(sizeof(bucket_t)); */
  if (bucket == NULL)
    {
      return NULL;
    }

  bucket->lock = 0;

  uint32_t j;
  for (j = 0; j < ENTRIES_PER_BUCKET; j++)
    {
      bucket->key[j] = 0;
    }
  bucket->next = NULL;
    
  return bucket;
}

hashtable_t* 
ht_create(uint32_t capacity) 
{
  hashtable_t* hashtable = NULL;
    
  if (capacity == 0)
    {
      return NULL;
    }
    
  /* Allocate the table itself. */
  hashtable = (hashtable_t*) memalign(CACHE_LINE_SIZE, sizeof(hashtable_t));
  if (hashtable == NULL) 
    {
      printf("** malloc @ hatshtalbe\n");
      return NULL;
    }
    
  /* hashtable->table = calloc(capacity, (sizeof(bucket_t))); */
  hashtable->table = (bucket_t*) memalign(CACHE_LINE_SIZE, capacity * (sizeof(bucket_t)));
  if (hashtable->table == NULL) 
    {
      printf("** alloc: hashtable->table\n"); fflush(stdout);
      free(hashtable);
      return NULL;
    }

  memset(hashtable->table, 0, capacity * (sizeof(bucket_t)));
    
  uint32_t i;
  for (i = 0; i < capacity; i++)
    {
      hashtable->table[i].lock = 0;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
	}
    }

  hashtable->capacity = capacity;
    
  return hashtable;
}

/* Hash a key for a particular hash table. */
uint32_t
ht_hash(hashtable_t* hashtable, ssht_addr_t key) 
{
	/* uint64_t hashval; */
	/* hashval = __ac_Jenkins_hash_64(key); */
	/* return hashval % hashtable->capacity; */
  /* return key % hashtable->capacity; */
  return key & (hashtable->capacity - 1);
}


  /* Retrieve a key-value entry from a hash table. */
void*
ht_get(hashtable_t* hashtable, ssht_addr_t key, uint32_t bin)
{
  bucket_t* bucket = hashtable->table + bin;
    
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  void* val = bucket->val[j];
	  if (bucket->key[j] == key) 
	    {
	      if (bucket->val[j] == val)
		{
		  return val;
		}
	      else
		{
		  return NULL;
		}
	    }
	}

      bucket = bucket->next;
    } while (bucket != NULL);
  return NULL;
}

inline ssht_addr_t
bucket_exists(bucket_t* bucket, ssht_addr_t key)
{
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      return true;
	    }
	}

      bucket = bucket->next;
    } while (bucket != NULL);
  return false;
}


/* Insert a key-value entry into a hash table. */
uint32_t
ht_put(hashtable_t* hashtable, ssht_addr_t key, uint32_t bin) 
{
  bucket_t* bucket = hashtable->table + bin;
#if defined(READ_ONLY_FAIL)
  if (bucket_exists(bucket, key))
    {
      return false;
    }
#endif
  lock_t* lock = &bucket->lock;

  ssht_addr_t* empty = NULL;
  void** empty_v = NULL;

  uint32_t j;

  LOCK_ACQ(lock);
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      LOCK_RLS(lock);
	      return false;
	    }
	  else if (empty == NULL && bucket->key[j] == 0)
	    {
	      empty = &bucket->key[j];
	      empty_v = &bucket->val[j];
	    }
	}
        
      if (bucket->next == NULL)
	{
	  if (empty == NULL)
	    {
	      DPP(put_num_failed_expand);
	      bucket->next = create_bucket();
	      bucket->next->key[0] = key;
	      bucket->next->val[0] = (void*) bucket;
	    }
	  else 
	    {
	      *empty_v = (void*) bucket;
	      *empty = key;
	    }

	  LOCK_RLS(lock);
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


/* Remove a key-value entry from a hash table. */
ssht_addr_t
ht_remove(hashtable_t* hashtable, ssht_addr_t key, int bin)
{
  bucket_t* bucket = hashtable->table + bin;
#if defined(READ_ONLY_FAIL)
  if (!bucket_exists(bucket, key))
    {
      return false;
    }
#endif  /* READ_ONLY_FAIL */

  lock_t* lock = &bucket->lock;
  uint32_t j;

  LOCK_ACQ(lock);
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      bucket->key[j] = 0;
	      LOCK_RLS(lock);
	      return key;
	    }
	}
      bucket = bucket->next;
    } while (bucket != NULL);
  LOCK_RLS(lock);
  return false;
}

void
ht_destroy(hashtable_t* hashtable)
{
  free(hashtable->table);
  free(hashtable);
}



uint32_t
ht_size(hashtable_t* hashtable, uint32_t capacity)
{
  bucket_t* bucket = NULL;
  size_t size = 0;

  uint32_t bin;
  for (bin = 0; bin < capacity; bin++)
    {
      bucket = hashtable->table + bin;
       
      uint32_t j;
      do
	{
	  for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	    {
	      if (bucket->key[j] > 0)
		{
		  size++;
		}
	    }

	  bucket = bucket->next;
	}
      while (bucket != NULL);
    }
  return size;
}

void
ht_print(hashtable_t* hashtable, uint32_t capacity)
{
  bucket_t* bucket;

  printf("Number of buckets: %u\n", capacity);

  uint32_t bin;
  for (bin = 0; bin < capacity; bin++)
    {
      bucket = hashtable->table + bin;
      
      printf("[[%05d]] ", bin);

      uint32_t j;
      do
	{
	  for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	    {
	      if (bucket->key[j])
	      	{
		  printf("(%-5llu)-> ", (long long unsigned int) bucket->key[j]);
		}
	    }

	  bucket = bucket->next;
	  printf(" ** -> ");
	}
      while (bucket != NULL);
      printf("\n");
    }
  fflush(stdout);
}
