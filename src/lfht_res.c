#include <math.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "lfht_res.h"

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
  if (bucket == NULL)
    {
      return NULL;
    }

  uint32_t j;
  for (j = 0; j < KEY_BUCKT; j++)
    {
      bucket->snapshot = 0;
      bucket->key[j] = 0;
    }

  return bucket;
}

hashtable_t* ht_create(uint32_t num_buckets);

hyht_wrapper_t* 
hyht_wrapper_create(uint32_t num_buckets)
{
  hyht_wrapper_t* w = (hyht_wrapper_t*) memalign(CACHE_LINE_SIZE, sizeof(hyht_wrapper_t));
  if (w == NULL)
    {
      printf("** malloc @ hatshtalbe\n");
      return NULL;
    }

  w->ht = ht_create(num_buckets);

  w->resize_lock = 0;
  w->gc_lock = 0;
  w->status_lock = 0;
  w->version_list = NULL;
  w->version_min = 0;
  w->ht_oldest = w->ht;

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

  memset((void*) hashtable->table, 0, num_buckets * (sizeof(bucket_t)));
    
  uint32_t i;
  for (i = 0; i < num_buckets; i++)
    {
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].snapshot = 0;
	  hashtable->table[i].key[j] = 0;
	}
    }

  hashtable->num_buckets = num_buckets;
  hashtable->hash = num_buckets - 1;

  hashtable->table_new = NULL;
  hashtable->table_prev = NULL;

  return hashtable;
}


/* Hash a key for a particular hash table. */
uint32_t
ht_hash(hashtable_t* hashtable, hyht_addr_t key) 
{
  /* uint64_t hashval; */
  /* return __ac_Jenkins_hash_64(key) & (hashtable->hash); */
  /* return hashval % hashtable->num_buckets; */
  /* return key % hashtable->num_buckets; */
  /* return key & (hashtable->num_buckets - 1); */
  return key & (hashtable->hash);
}


static inline hyht_val_t
lfht_bucket_search(bucket_t* bucket, hyht_addr_t key)
{
  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      hyht_val_t val = bucket->val[i];
      if (bucket->map[i] == MAP_VALID && bucket->key[i] == key)
      	{
	  if (likely(bucket->val[i] == val))
	    {
	      return val;
	    }
	  else
	    {
	      return 0;
	    }
	}
    }
  return 0;
}


/* Retrieve a key-value entry from a hash table. */
hyht_val_t
ht_get(hashtable_t* hashtable, hyht_addr_t key)
{
  LFHT_GC_HT_VERSION_USED(hashtable);
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;

  return lfht_bucket_search(bucket, key);
}



__thread size_t num_retry_cas1 = 0, num_retry_cas2 = 0, num_retry_cas3 = 0, num_retry_cas4 = 0, num_retry_cas5 = 0;

void
ht_print_retry_stats()
{
  printf("#cas1: %-8zu / #cas2: %-8zu / #cas3: %-8zu / #cas4: %-8zu\n",
	 num_retry_cas1, num_retry_cas2, num_retry_cas3, num_retry_cas4);
}

#define DO_LF_STATS 0

#if DO_LF_STATS == 1
#  define INC(x) x++
#else
#  define INC(x) ;
#endif


/* Insert a key-value entry into a hash table. */
int
ht_put(hyht_wrapper_t* h, hyht_addr_t key, hyht_val_t val) 
{
  int empty_retries = 0;
 retry_all:
  LFHT_CHECK_RESIZE(h);
  hashtable_t* hashtable = h->ht;
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;

  int empty_index = -2;
  lfht_snapshot_all_t s, s1;

 retry:
  s = bucket->snapshot;

  if (lfht_bucket_search(bucket, key) != 0)
    {
      if (unlikely(empty_index >= 0))
	{
	  bucket->map[empty_index] = MAP_INVLD;
	}
      return false;
    }

  if (likely(empty_index < 0))
    {
      empty_index = snap_get_empty_index(s);
      if (empty_index < 0)
	{
	  if (empty_retries++ >= 2)
	    {
	      empty_retries = 0;
	      if (LFHT_LOCK_RESIZE(h))
		{
		  ht_resize_pes(h, 1, 2);
		}
	    }
	  goto retry_all;
	}
      s1 = snap_set_map(s, empty_index, MAP_INSRT);
      if (CAS_U64(&bucket->snapshot, s, s1) != s)
	{
	  empty_index = -2;
	  INC(num_retry_cas1);
	  goto retry;
	}
  
      bucket->val[empty_index] = val;
      bucket->key[empty_index] = key;
    }
  else
    {
      s1 = snap_set_map(s, empty_index, MAP_INSRT);
    }

  lfht_snapshot_all_t s2 = snap_set_map_and_inc_version(s1, empty_index, MAP_VALID);
  if (CAS_U64(&bucket->snapshot, s1, s2) != s1)
    {
      INC(num_retry_cas2);
      goto retry;
    }

  return true;
}


/* Remove a key-value entry from a hash table. */
hyht_val_t
ht_remove(hyht_wrapper_t* h, hyht_addr_t key)
{
  LFHT_CHECK_RESIZE(h);
  hashtable_t* hashtable = h->ht;
  size_t bin = ht_hash(hashtable, key);
  bucket_t* bucket = hashtable->table + bin;

  lfht_snapshot_t s;

  int i;
 retry:
  s.snapshot = bucket->snapshot;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (bucket->key[i] == key && s.map[i] == MAP_VALID)
	{
	  hyht_val_t removed = bucket->val[i];
	  lfht_snapshot_all_t s1 = snap_set_map(s.snapshot, i, MAP_INVLD);
	  if (CAS_U64(&bucket->snapshot, s.snapshot, s1) == s.snapshot)
	    {
	      return removed;
	    }
	  else
	    {
	      INC(num_retry_cas3);
	      goto retry;
	    }
	}
    }
  return 0;
}


static uint32_t
ht_put_seq(hashtable_t* hashtable, hyht_addr_t key, hyht_val_t val, uint32_t bin) 
{
  volatile bucket_t* bucket = hashtable->table + bin;
  uint32_t j;
  for (j = 0; j < KEY_BUCKT; j++) 
    {
      if (bucket->key[j] == 0)
	{
	  bucket->val[j] = val;
	  bucket->key[j] = key;
	  bucket->map[j] = MAP_VALID;
	  return true;
	}
    }

  printf("[LFHT] even the new ht does not have space (bucket %d) \n", bin);
  return false;
}

static int
bucket_cpy(volatile bucket_t* bucket, hashtable_t* ht_new)
{
  uint32_t j;
  for (j = 0; j < KEY_BUCKT; j++) 
    {
      if (bucket->map[j] == MAP_VALID)
	{
	  hyht_addr_t key = bucket->key[j];
	  uint32_t bin = ht_hash(ht_new, key);
	  ht_put_seq(ht_new, key, bucket->val[j], bin);
	}
    }

  return 1;
}


/* resizing */
int 
ht_resize_pes(hyht_wrapper_t* h, int is_increase, int by)
{
  ticks s = getticks();

  hashtable_t* ht_old = h->ht;

  size_t num_buckets_new;
  if (is_increase == true)
    {
      num_buckets_new = by * ht_old->num_buckets;
    }
  else
    {
      num_buckets_new = ht_old->num_buckets / 2;
    }

  hashtable_t* ht_new = ht_create(num_buckets_new);
  
  size_t cur_version = ht_old->version;
  ht_old->version++;

  LFHT_GC_HT_VERSION_USED(ht_old);

  size_t version_min;
  do
    {
      version_min = ht_gc_min_version_used(h);
    }
  while(cur_version >= version_min);

  ht_new->version = cur_version + 2;

  int32_t b;
  for (b = 0; b < ht_old->num_buckets; b++)
    {
      bucket_t* bu_cur = ht_old->table + b;
      bucket_cpy(bu_cur, ht_new);
    }

#if defined(DEBUG)
  /* if (ht_size(ht_old) != ht_size(ht_new)) */
  /*   { */
  /*     printf("**ht_size(ht_old) = %zu != ht_size(ht_new) = %zu\n", ht_size(ht_old), ht_size(ht_new)); */
  /*   } */
#endif

  ht_new->table_prev = ht_old;

  SWAP_U64((uint64_t*) h, (uint64_t) ht_new);
  ht_old->table_new = ht_new;

  LFHT_RLS_RESIZE(h);

  ticks e = getticks() - s;
  printf("[RESIZE-%02d] to #bu %7zu    | took: %13llu ti = %8.6f s\n", 
	 0, ht_new->num_buckets, (unsigned long long) e, e / 2.1e9);

  ht_gc_collect(h);

  return 1;
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
      int i;
      for (i = 0; i < KEY_BUCKT; i++)
	{
	  if (bucket->key[i] != 0  && bucket->map[i] == MAP_VALID)
	    {
	      size++;
	    }
	}
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
		  printf("(%-5llu/%p)-> ", (long long unsigned int) bucket->key[j], (void*) bucket->val[j]);
		}
	    }
	  printf(" ** -> ");
	}
      while (bucket != NULL);
      printf("\n");
    }
  fflush(stdout);
}
