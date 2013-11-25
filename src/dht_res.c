#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "dht_res.h"

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

bucket_t*
create_bucket_stats(hashtable_t* h, int* resize) 
{
  bucket_t* b = create_bucket();
  if (IAF_U32(&h->num_expands) == h->num_expands_threshold)
    {
      printf("      -- hit threshold (%u ~ %u)\n", h->num_expands, h->num_expands_threshold);
      *resize = 1;
    }
  return b;
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
      hashtable->table[i].lock = LOCK_FREE;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
	}
    }

  hashtable->num_buckets = num_buckets;
  hashtable->resize_lock = 0;
  hashtable->table_tmp = NULL;
  hashtable->table_new = NULL;
  hashtable->num_expands = 0;
  hashtable->num_expands_threshold = (HYHT_PERC_EXPANSIONS * num_buckets);
  if (hashtable->num_expands_threshold == 0)
    {
      hashtable->num_expands_threshold = 1;
    }
  printf(" :: buckets: %u / threshold: %u\n", num_buckets, hashtable->num_expands_threshold);
    
  return hashtable;
}


/* Hash a key for a particular hash table. */
uint32_t
ht_hash(hashtable_t* hashtable, ssht_addr_t key) 
{
	/* uint64_t hashval; */
	/* hashval = __ac_Jenkins_hash_64(key); */
	/* return hashval % hashtable->num_buckets; */
  /* return key % hashtable->num_buckets; */
  return key & (hashtable->num_buckets - 1);
}


  /* Retrieve a key-value entry from a hash table. */
void*
ht_get(hashtable_t** h, ssht_addr_t key)
{

  hashtable_t* hashtable = *h;
  size_t bin = ht_hash(hashtable, key);
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
ht_put(hashtable_t** h, ssht_addr_t key) 
{

  hashtable_t* hashtable = *h;
  size_t bin = ht_hash(hashtable, key);

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

  bucket = LOCK_ACQ(lock, bucket, hashtable, key);
  lock = &bucket->lock;

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
        
      int resize = 0;
      if (bucket->next == NULL)
	{
	  if (empty == NULL)
	    {
	      DPP(put_num_failed_expand);
	      bucket->next = create_bucket_stats(hashtable, &resize);
	      bucket->next->key[0] = key;
	      bucket->next->val[0] = (void*) bucket;
	    }
	  else 
	    {
	      *empty_v = (void*) bucket;
	      *empty = key;
	    }

	  LOCK_RLS(lock);
	  if (resize)
	    {
	      ht_resize_pes(h);
	    }
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


/* Remove a key-value entry from a hash table. */
ssht_addr_t
ht_remove(hashtable_t** h, ssht_addr_t key)
{

  hashtable_t* hashtable = *h;
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;
#if defined(READ_ONLY_FAIL)
  if (!bucket_exists(bucket, key))
    {
      return false;
    }
#endif  /* READ_ONLY_FAIL */

  lock_t* lock = &bucket->lock;
  uint32_t j;

  bucket = LOCK_ACQ(lock, bucket, hashtable, key);
  lock = &bucket->lock;

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

static uint32_t
ht_put_seq(hashtable_t* hashtable, ssht_addr_t key, uint32_t bin) 
{
  bucket_t* bucket = hashtable->table + bin;
  ssht_addr_t* empty = NULL;
  void** empty_v = NULL;
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
	      *empty_v = (void*) bucket;
	      *empty = key;
	      return true;
	    }
	}
        
      if (bucket->next == NULL)
	{
	  DPP(put_num_failed_expand);
	  bucket->next = create_bucket();
	  bucket->next->key[0] = key;
	  bucket->next->val[0] = (void*) bucket;
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


static void
bucket_cpy(bucket_t* bucket, hashtable_t* ht_new)
{
  LOCK_ACQ_RES(&bucket->lock);
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  ssht_addr_t key = bucket->key[j];
	  if (key != 0) 
	    {
	      uint32_t bin = ht_hash(ht_new, key);
	      ht_put_seq(ht_new, key, bin);
	    }
	}
      bucket = bucket->next;
    } while (bucket != NULL);

}

int 
ht_resize_pes(hashtable_t** h)
{
  hashtable_t* ht_old = *h;
  if (ht_old->num_buckets > 16384)
    {
      return 0;
    }

  if (TAS_U8(&ht_old->resize_lock))
    {
      /* printf("  // abort: already being resized\n"); */
      return 0;
    }
  printf("// resizing: from %8lu to %8lu buckets\n", ht_old->num_buckets, 2 * ht_old->num_buckets);

  hashtable_t* ht_new = ht_create(2 * ht_old->num_buckets);
  int32_t b;
  for (b = 0; b < ht_old->num_buckets; b++)
    {
      bucket_t* bu_cur = ht_old->table + b;
      bucket_cpy(bu_cur, ht_new);
    }

  /* if (ht_size(ht_old) != ht_size(ht_new)) */
  /*   { */
  /*     printf("**ht_size(ht_old) = %lu != ht_size(ht_new) = %lu\n", ht_size(ht_old), ht_size(ht_new)); */
  /*   } */

  SWAP_PTR((volatile void*) h, (void*) ht_new);
  ht_old->table_new = ht_new;

  return 1;
}


void
ht_destroy(hashtable_t** hashtable)
{
  free((*hashtable)->table);
  free(*hashtable);
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
