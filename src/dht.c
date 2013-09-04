#include <math.h>
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

__thread bucket_t* bucket_expand = NULL;

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
    
  bucket = malloc(sizeof(bucket_t ));
  /* bucket = memalign(CACHE_LINE_SIZE, sizeof(bucket_t )); */
  if(bucket == NULL)
    {
      return NULL;
    }
    

  /* bucket->empty = bucket->key; */
  bucket->ts = 0;
  bucket->lock = 0;

  uint32_t j;
  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
    {
      bucket->key[j] = 0;
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
      /* hashtable->table[i].empty =  hashtable->table[i].key; */
      hashtable->table[i].ts = 0;
      hashtable->table[i].lock = 0;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
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
	/* uint64_t hashval; */
	/* hashval = __ac_Jenkins_hash_64(key); */
    
	/* return hashval % hashtable->capacity; */
  return key % hashtable->capacity;
}



/* Insert a key-value entry into a hash table. */
/* uint32_t */
/* ht_put(hashtable_t* hashtable, ssht_addr_t key, uint32_t bin)  */
/* { */
/*   bucket_t *bucket = hashtable->table + bin; */
/*   bucket_t* bucket_first = bucket; */
/*   ssht_addr_t* empties[8] = {0}; */

/*   uint32_t j; */
/*  restart: */
/*   do  */
/*     { */
/*       uint16_t empty_ind = 0; */
/*       for (j = 0; j < ENTRIES_PER_BUCKET; j++)  */
/* 	{ */
/* 	  if (bucket->key[j] == key)  */
/* 	    { */
/* 	      return false; */
/* 	    } */
/* 	  else if (bucket->key[j] == 0) */
/* 	    { */
/* 	      empties[empty_ind++ & 7] = &bucket->key[j]; */
/* 	    } */
/* 	} */
        
/*       if (bucket->next == NULL) */
/* 	{ */
/* 	  LOCK_ACQ(&bucket_first->lock); */
/* 	  if (empty_ind < 2 && bucket->next == NULL) */
/* 	    { */
/* 	      bucket->next = create_bucket(); */
/* 	      bucket->next->key[0] = key; */
/* 	      bucket_first->empty = &bucket->next->key[1]; */
/* 	      LOCK_RLS(&bucket_first->lock); */
/* 	      return true; */
/* 	    } */
/* 	  else */
/* 	    { */
/* 	      ssht_addr_t* empty_other = NULL; */
/* 	      if (empty_ind > 7) empty_ind = 7; */
/* 	      uint32_t id; */
/* 	      for (id = 0; id < empty_ind; id++) */
/* 		{ */
/* 		  ssht_addr_t* k = empties[id]; */
/* 		  if (*k == NULL && k != bucket_first->empty) */
/* 		    { */
/* 		      empty_other = k; */
/* 		      break; */
/* 		    } */
/* 		} */

/* 	      if (empty_other != NULL) */
/* 		{ */
/* 		  *bucket_first->empty = key; */
/* 		  bucket_first->empty = empty_other; */
/* 		  LOCK_RLS(&bucket_first->lock); */
/* 		  return true; */
/* 		} */

/* 	      LOCK_RLS(&bucket_first->lock); */
/* 	      goto restart; */
/* 	    } */
/* 	} */

/*       bucket = bucket->next; */
/*     } while (true); */
/* } */


/* Insert a key-value entry into a hash table. */
uint32_t
ht_put(hashtable_t* hashtable, ssht_addr_t key, uint32_t bin) 
{
  bucket_t *bucket = hashtable->table + bin;
  bucket_t* bucket_first = bucket;
  ssht_addr_t* empties[8] = {0};

  uint32_t j;
  uint64_t ts;
 restart:
  bucket = bucket_first;
  ts = bucket->ts;
  do 
    {
      ssht_addr_t* empty = NULL;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      return false;
	    }
	  else if (empty == NULL && bucket->key[j] == 0)
	    {
	      empty = &bucket->key[j];
	    }
	}
        
      if (bucket->next == NULL)
	{
	  LOCK_ACQ(&bucket_first->lock);
	  if (bucket_first->ts != ts || bucket->next != NULL)
	    {
	      LOCK_RLS(&bucket_first->lock);
	      goto restart;
	    }

	  if (empty == NULL)
	    {
	      bucket_first->ts++;
	      bucket->next = create_bucket();
	      bucket->next->key[0] = key;
	      LOCK_RLS(&bucket_first->lock);
	      return true;
	    }
	  else if (*empty != NULL)
	    {
	      LOCK_RLS(&bucket_first->lock);
	      goto restart;
	    }
	  else 
	    {
	      bucket_first->ts++;
	      *empty = key;
	      LOCK_RLS(&bucket_first->lock);
	      return true;
	    }
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
      for(j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if(bucket->key[j] == key) 
	    {
	      return key;
	    }
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
  bucket_t* bucket_first = bucket;

  uint32_t j;
  do 
    {
      for(j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if(bucket->key[j] == key) 
	    {
	      return CAS_U64_BOOL(&bucket->key[j], key, NULL);
	    }	      /* else  */
	      /* 	{ */
	      /* 	  LOCK_RLS(&bucket_first->lock); */
	      /* 	  return false; */
	      /* 	} */
	    }
	}

      bucket = bucket->next;
    } while (bucket != NULL);
  return false;
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
       
      uint32_t j;
      do
	{
	  for(j = 0; j < ENTRIES_PER_BUCKET; j++)
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
ht_print(hashtable_t *hashtable, uint32_t capacity)
{
  bucket_t *bucket;

  printf("Number of buckets: %u\n", capacity);

  uint32_t bin;
  for (bin = 0; bin < capacity; bin++)
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




