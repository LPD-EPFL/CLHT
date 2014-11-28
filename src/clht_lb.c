#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "clht_lb.h"

__thread ssmem_allocator_t* clht_alloc;

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

clht_wrapper_t* 
clht_wrapper_create(uint32_t num_buckets)
{
  clht_wrapper_t* w = (clht_wrapper_t*) memalign(CACHE_LINE_SIZE, sizeof(clht_wrapper_t));
  if (w == NULL)
    {
      printf("** malloc @ hatshtalbe\n");
      return NULL;
    }

  w->ht = ht_create(num_buckets);
  if (w->ht == NULL)
    {
      free(w);
      return NULL;
    }

  return w;
}

hashtable_t* 
ht_create(uint32_t num_buckets) 
{
  hashtable_t* hashtable = NULL;
    
  if (num_buckets == 0)
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
    
  /* hashtable->table = calloc(num_buckets, (sizeof(bucket_t))); */
  hashtable->table = (bucket_t*) memalign(CACHE_LINE_SIZE, num_buckets * (sizeof(bucket_t)));
  if (hashtable->table == NULL) 
    {
      printf("** alloc: hashtable->table\n"); fflush(stdout);
      free(hashtable);
      return NULL;
    }

  memset(hashtable->table, 0, num_buckets * (sizeof(bucket_t)));
    
  uint32_t i;
  for (i = 0; i < num_buckets; i++)
    {
      hashtable->table[i].lock = 0;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
	}
    }

  hashtable->num_buckets = num_buckets;
    
  return hashtable;
}

/* Hash a key for a particular hash table. */
uint32_t
ht_hash(hashtable_t* hashtable, clht_addr_t key) 
{
	/* uint64_t hashval; */
	/* hashval = __ac_Jenkins_hash_64(key); */
	/* return hashval % hashtable->num_buckets; */
  /* return key % hashtable->num_buckets; */
  return key & (hashtable->num_buckets - 1);
}


  /* Retrieve a key-value entry from a hash table. */
clht_val_t
ht_get(hashtable_t* hashtable, clht_addr_t key)
{
  size_t bin = ht_hash(hashtable, key);
  volatile bucket_t* bucket = hashtable->table + bin;
    
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  clht_val_t val = bucket->val[j];
#ifdef __tile__
	  _mm_lfence();
#endif
	  if (bucket->key[j] == key) 
	    {
	      if (bucket->val[j] == val)
		{
		  return val;
		}
	      else
		{
		  return 0;
		}
	    }
	}

      bucket = bucket->next;
    } 
  while (bucket != NULL);
  return 0;
}

inline clht_addr_t
bucket_exists(bucket_t* bucket, clht_addr_t key)
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
int
ht_put(clht_wrapper_t* h, clht_addr_t key, clht_val_t val) 
{
  hashtable_t* hashtable = h->ht;
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;

#if defined(READ_ONLY_FAIL)
  if (bucket_exists(bucket, key))
    {
      return false;
    }
#endif
  clht_lock_t* lock = &bucket->lock;

  clht_addr_t* empty = NULL;
  clht_val_t* empty_v = NULL;

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
#ifdef __tile__
	      _mm_sfence();
#endif
	      bucket->next->val[0] = val;
	    }
	  else 
	    {
	      *empty_v = val;
#ifdef __tile__
	      _mm_sfence();
#endif
	      *empty = key;
	    }

	  LOCK_RLS(lock);
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}



/* Remove a key-value entry from a hash table. */
clht_val_t
ht_remove(clht_wrapper_t* h, clht_addr_t key)
{
  hashtable_t* hashtable = h->ht;
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;

#if defined(READ_ONLY_FAIL)
  if (!bucket_exists(bucket, key))
    {
      return false;
    }
#endif  /* READ_ONLY_FAIL */

  clht_lock_t* lock = &bucket->lock;
  uint32_t j;

  LOCK_ACQ(lock);
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      clht_val_t val = bucket->val[j];
	      bucket->key[j] = 0;
	      LOCK_RLS(lock);
	      return val;
	    }
	}
      bucket = bucket->next;
    } while (bucket != NULL);
  LOCK_RLS(lock);
  return false;
}

static uint32_t
ht_put_seq(hashtable_t* hashtable, clht_addr_t key, clht_val_t val, uint32_t bin) 
{
  bucket_t* bucket = hashtable->table + bin;
  clht_addr_t* empty = NULL;
  clht_val_t* empty_v = NULL;
  uint32_t j;

  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
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
	      bucket->next->val[0] = val;
	    }
	  else 
	    {
	      *empty_v = val;
	      *empty = key;
	    }
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


static inline void
bucket_cpy(bucket_t* bucket, hashtable_t* ht_new)
{
  LOCK_ACQ(&bucket->lock);
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  clht_addr_t key = bucket->key[j];
	  if (key != 0) 
	    {
	      uint32_t bin = ht_hash(ht_new, key);
	      clht_val_t val = bucket->key[j];
	      ht_put_seq(ht_new, key, val, bin);
	    }
	}
      bucket = bucket->next;
    } while (bucket != NULL);

}

void
ht_destroy(hashtable_t* hashtable)
{
  free(hashtable->table);
  free(hashtable);
}



size_t
ht_size(hashtable_t* hashtable)
{
  uint32_t num_buckets = hashtable->num_buckets;
  bucket_t* bucket = NULL;
  size_t size = 0;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
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
ht_print(hashtable_t* hashtable)
{
  uint32_t num_buckets = hashtable->num_buckets;
  bucket_t* bucket;

  printf("Number of buckets: %u\n", num_buckets);

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
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
