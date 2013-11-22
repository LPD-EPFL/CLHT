#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "dht_packed.h"

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
  bucket = memalign(CACHE_LINE_SIZE, sizeof(bucket_t ));
  if(bucket == NULL)
    {
      return NULL;
    }

  bucket->lock = 0;
  bucket->last = 0;

  uint32_t j;
  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
    {
      bucket->key[j] = 0;
    }
  bucket->next = NULL;
    
  return bucket;
}

hashtable_t* 
ht_create(uint32_t num_buckets) 
{
  hashtable_t* hashtable = NULL;
    
  if(num_buckets == 0)
    {
      return NULL;
    }
    
  /* Allocate the table itself. */
  hashtable = (hashtable_t*) memalign(CACHE_LINE_SIZE, sizeof(hashtable_t));
  if(hashtable == NULL ) 
    {
      printf("** malloc @ hatshtalbe\n");
      return NULL;
    }
    
  /* hashtable->table = calloc(num_buckets, (sizeof(bucket_t))); */
  hashtable->table = (bucket_t*) memalign(CACHE_LINE_SIZE, num_buckets * (sizeof(bucket_t)));
  if(hashtable->table == NULL ) 
    {
      printf("** alloc: hashtable->table\n"); fflush(stdout);
      free(hashtable);
      return NULL;
    }

  memset(hashtable->table, 0, num_buckets * (sizeof(bucket_t)));
    
  uint32_t i;
  for(i = 0; i < num_buckets; i++)
    {
      hashtable->table[i].lock = 0;
      hashtable->table[i].last = 0;
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
ht_hash( hashtable_t* hashtable, ssht_addr_t key ) 
{
	/* uint64_t hashval; */
	/* hashval = __ac_Jenkins_hash_64(key); */
	/* return hashval % hashtable->num_buckets; */
  /* return key % hashtable->num_buckets; */
  return key & (hashtable->num_buckets - 1);
}


  /* Retrieve a key-value entry from a hash table. */
void*
ht_get(hashtable_t *hashtable, ssht_addr_t key, uint32_t bin)
{
  bucket_t* bucket = hashtable->table + bin;
    
  int32_t j;
  do 
    {
      int32_t bu_last = bucket->last - 1;
      for(j = bu_last; j >= 0; j--) 
	{
	  void* val = bucket->val[j];
	  if(bucket->key[j] == key) 
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

      if (bu_last < ENTRIES_PER_BUCKET - 1)
	{
	  break;
	}
      bucket = bucket->next;
    } while (bucket != NULL);
  return NULL;
}

inline ssht_addr_t
bucket_exists(bucket_t* bucket, ssht_addr_t key)
{
  int32_t j;
  do 
    {
      int32_t bu_last = bucket->last - 1;
      for(j = bu_last; j >= 0; j--) 
	{
	  if(bucket->key[j] == key) 
	    {
	      return true;
	    }
	}

      if (bu_last < ENTRIES_PER_BUCKET - 1)
	{
	  break;
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
  uint32_t j;

  LOCK_ACQ(lock);
  do 
    {
      uint32_t bu_last = bucket->last;
      for (j = 0; j < bu_last; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      LOCK_RLS(lock);
	      return false;
	    }
	}
        
      if (bu_last < ENTRIES_PER_BUCKET)
	{
	  bucket->val[bu_last] = (void*) bucket;
	  bucket->key[bu_last] = key;
	  bucket->last++;
	  LOCK_RLS(lock);
	  return true;
	}
      else if (bucket->next == NULL)
	{
	  DPP(put_num_failed_expand);
	  bucket->next = create_bucket();
	  bucket->next->key[0] = key;
	  bucket->next->val[0] = (void*) bucket;
	  bucket->next->last++;
	  LOCK_RLS(lock);
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


/* Remove a key-value entry from a hash table. */
ssht_addr_t
ht_remove( hashtable_t *hashtable, ssht_addr_t key, int bin )
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
      uint32_t bu_last = bucket->last;
      for(j = 0; j < bu_last; j++) 
	{
	  if(bucket->key[j] == key) 
	    {
	      bucket_t* blast = bucket;
	      while (blast->next != NULL && blast->next->last)
		{
		  blast = blast->next;
		}

	      bucket->key[j] = 0;
	      uint32_t blast_last = blast->last;
	      assert(blast_last != 0);
	      bucket->val[j] = blast->val[blast_last - 1];
	      bucket->key[j] = blast->key[blast_last - 1];
	      blast->last--;
	      LOCK_RLS(lock);
	      return key;
	    }
	}

      if (bu_last < ENTRIES_PER_BUCKET)
	{
	  break;
	}
      bucket = bucket->next;
    } while (bucket != NULL);
  LOCK_RLS(lock);
  return false;
}

void
ht_destroy( hashtable_t *hashtable)
{
    /* int num_buckets = hashtable->num_buckets; */
    /* bucket_t *bucket_c = NULL, *bucket_p = NULL; */
    
    /* int i,j; */
    /* for( i = 0; i < num_buckets; i++ ) { */
        
    /*   bucket_c = hashtable->table[i]; */
        
    /*   do { */
    /*     for( j = 0; j < ENTRIES_PER_BUCKET; j++ ) { */
                
    /* 	if(bucket_c->entries[j] != NULL) { */
                    
    /* 	  bucket_c->entries[j] = NULL; */
    /* 	  (bucket_c->next)->entries[j] = NULL; */
    /* 	} */
    /*     } */
            
    /*     bucket_p = bucket_c; */
    /*     bucket_c = (bucket_p->next)->next; */
            
    /*     free(bucket_p->next->entries); */
    /*     free(bucket_p->entries); */
    /*     free(bucket_p->next); */
    /*     free(bucket_p); */
            
    /*   } while (bucket_c != NULL); */
    /* } */
    
    free(hashtable->table);
    free(hashtable);
  }



uint32_t
ht_size(hashtable_t *hashtable, uint32_t num_buckets)
{
  bucket_t *bucket = NULL;
  size_t size = 0;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;
       
      uint32_t j;
      do
	{
	  uint32_t bu_last = bucket->last;
	  for(j = 0; j < bu_last; j++)
	    {
	      size++;
	    }

	  if (bu_last < ENTRIES_PER_BUCKET)
	    {
	      break;
	    }
	  bucket = bucket->next;
	}
      while (bucket != NULL);
    }
  return size;
}

void
ht_print(hashtable_t *hashtable, uint32_t num_buckets)
{
  bucket_t *bucket;

  printf("Number of buckets: %u\n", num_buckets);

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;
      
      printf("[[%05d]] ", bin);

      uint32_t j;
      do
	{
	  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
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
