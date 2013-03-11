#include <math.h>
#ifdef __sparc__
#include "../include/dht.h"
#else
#include "dht.h"
#endif

#include "stdlib.h"
#include "assert.h"

static inline int
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
    
  bucket = malloc(sizeof(bucket_t ));
  /* bucket = memalign(CACHE_LINE_SIZE, sizeof(bucket_t )); */
  if(bucket == NULL)
    {
      return NULL;
    }
    
  bucket->empty = 0;

  uint32_t j;
  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
    {
      bucket->key[j] = 0;
      bucket->entry[j] = NULL;
    }
    
  bucket->next = NULL;
    
  return bucket;
}

int *num_buckets;

hashtable_t* 
ht_create(uint32_t capacity) 
{
    
  hashtable_t *hashtable = NULL;
    
  if(capacity == 0)
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
    
  hashtable->table = calloc(capacity, (sizeof(bucket_t)));
  if(hashtable->table  == NULL ) 
    {
      free(hashtable);
      return NULL;
    }
    
  uint32_t i;
  for(i = 0; i < capacity; i++)
    {
      /* hashtable->table[i] = create_bucket(); */
      hashtable->table[i].empty = 0;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
	  hashtable->table[i].entry[j] = 0;
	}
    }

  hashtable->capacity = capacity;
    
  /* if( ( num_buckets = calloc( capacity, sizeof( int ) ) ) == NULL ) { */
  /*   return NULL; */
  /* } */
    
  return hashtable;
}

/* Hash a key for a particular hash table. */
uint32_t
ht_hash( hashtable_t *hashtable, uint64_t key ) 
{
	uint64_t hashval;
	hashval = __ac_Jenkins_hash_64(key);
    
	return hashval % hashtable->capacity;
}

/* Insert a key-value entry into a hash table. */
uint32_t
ht_put(hashtable_t* hashtable, uint64_t key, void* value, uint32_t bin) 
{
  bucket_t *bucket = hashtable->table + bin;
    
  uint32_t j;
  do 
    {
      for (j = 0; j < bucket->empty; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      return false;
	    }
	}
        
      if (bucket->empty < ENTRIES_PER_BUCKET)
	{
	  bucket->key[j] = key;
	  bucket->entry[j] = value;
	  bucket->empty++;
	  return true;
	}
      else if (bucket->next == NULL)
	{
	  bucket->next = create_bucket();
	  assert(bucket->next != NULL);
	}

      bucket = bucket->next;
    } while (true);
}

  /* Retrieve a key-value entry from a hash table. */
void*
ht_get(hashtable_t *hashtable, uint64_t key, uint32_t bin)
{
    
  bucket_t *bucket = hashtable->table + bin;
    
  uint32_t j;
  do 
    {
      for(j = 0; j < bucket->empty; j++) 
	{
	  if(bucket->key[j] == key) 
	    {
	      return bucket->entry[j];
	    }
	}

      if (bucket->empty == CACHE_LINE_SIZE)
	{
	  return NULL;
	}
      bucket = bucket->next;
    } while (bucket != NULL);
  return NULL;
}

  /* Remove a key-value entry from a hash table. */
void*
ht_remove( hashtable_t *hashtable, uint64_t key, int bin )
{
  bucket_t* bucket = hashtable->table + bin;
  bucket_t* bucket_last_prev = bucket;
    
  uint32_t j;
  do 
    {
      for(j = 0; j < bucket->empty; j++) 
	{
	  if(bucket->key[j] == key) 
	    {
	      bucket->key[j] = 0;

	      bucket_t* bucket_last = bucket;
	      while (bucket_last->next != NULL)
		{
		  bucket_last_prev = bucket_last;
		  bucket_last = bucket_last->next;
		}

	      uint32_t move = bucket_last->empty - 1;
	      bucket->key[j] = bucket_last->key[move];
	      void* value_rmved = bucket->entry[j];
	      bucket->entry[j] = bucket_last->entry[move];

	      bucket_last->key[move] = 0;
	      bucket_last->entry[move] = NULL;
	      bucket_last->empty--;

	      if (bucket_last->empty == 0 && bucket_last != bucket_last_prev)
		{
		  free(bucket_last);
		  bucket_last_prev->next = NULL;
		}

	      return value_rmved;
	    }
	}

      if (bucket->empty < ENTRIES_PER_BUCKET)
	{
	  return NULL;
	}
        
      bucket_last_prev = bucket;
      bucket = bucket->next;
    } while (bucket != NULL);
  return NULL;
}

void
ht_destroy( hashtable_t *hashtable)
{
    /* int capacity = hashtable->capacity; */
    /* bucket_t *bucket_c = NULL, *bucket_p = NULL; */
    
    /* int i,j; */
    /* for( i = 0; i < capacity; i++ ) { */
        
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
ht_size(hashtable_t *hashtable, uint32_t capacity)
{
  bucket_t *bucket = NULL;
  size_t size = 0;

  uint32_t bin;
  for (bin = 0; bin < capacity; bin++)
    {
      bucket = hashtable->table + bin;
       
      uint8_t have_more = 1;
      uint32_t j;
      do
	{
	  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
	    {
	      if(bucket->key[j] != 0)
		{
		  size++;
		}
	      else
		{
		  have_more = 0;
		  break;
		}
	    }

	  bucket = bucket->next;
	}
      while (have_more && bucket != NULL);
    }
  return size;
}

void
ht_print(hashtable_t *hashtable, uint32_t capacity)
{
  bucket_t *bucket = NULL;

  uint32_t bin;
  for (bin = 0; bin < capacity; bin++)
    {
      printf("[%-4u]: ", bin);
      bucket = hashtable->table + bin;
       
      uint8_t have_more = 1;
      uint32_t j;
      do
	{
	  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
	    {
	      if(bucket->key[j] != 0)
		{
		  printf("(%-5llu/ %p)-> ", (long long unsigned int) bucket->key[j], bucket->entry[j]);
		}
	      else
		{
		  printf("NULL\n");
		  have_more = 0;
		  break;
		}
	    }

	  printf("** -> ");
	  bucket = bucket->next;
	}
      while (have_more && bucket != NULL);
    }
}




