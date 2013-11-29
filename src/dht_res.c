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

size_t ht_status(hyht_wrapper_t* h, int resize_increase, int just_print);
#define HYHT_STATUS_INV 50000
__thread size_t check_ht_status_steps = 0;

#define HYHT_DO_CHECK_STATUS 1

#if HYHT_DO_CHECK_STATUS == 1
#  define HYHT_CHECK_STATUS(h)			\
  if ((--check_ht_status_steps) == 0)		\
    {						\
      ht_status(h, 0, 0);			\
      check_ht_status_steps = HYHT_STATUS_INV;	\
    }

#else 
#  define HYHT_CHECK_STATUS()
#endif


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
  if (w->ht == NULL)
    {
      free(w);
      return NULL;
    }
  w->resize_lock = LOCK_FREE;
  w->gc_lock = LOCK_FREE;
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
  hashtable->hash = num_buckets - 1;
  hashtable->version = 0;
  hashtable->table_tmp = NULL;
  hashtable->table_new = NULL;
  hashtable->table_prev = NULL;
  hashtable->num_expands = 0;
  hashtable->num_expands_threshold = (HYHT_PERC_EXPANSIONS * num_buckets);
  if (hashtable->num_expands_threshold == 0)
    {
      hashtable->num_expands_threshold = 1;
    }
  hashtable->is_helper = 1;
  hashtable->helper_done = 0;
 
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


/* Retrieve a key-value entry from a hash table. */
hyht_val_t
ht_get(hyht_wrapper_t* h, hyht_addr_t key)
{
  hashtable_t* hashtable = h->ht;
  size_t bin = ht_hash(hashtable, key);
  HYHT_GC_HT_VERSION_USED(hashtable);
  bucket_t* bucket = hashtable->table + bin;

  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  hyht_val_t val = bucket->val[j];
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
    } while (bucket != NULL);
  return 0;
}

inline int
bucket_exists(bucket_t* bucket, hyht_addr_t key)
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
ht_put(hyht_wrapper_t* h, hyht_addr_t key, hyht_val_t val) 
{
  HYHT_CHECK_STATUS(h);
  hashtable_t* hashtable;

  hyht_addr_t* empty = NULL;
  hyht_val_t* empty_v = NULL;
  hyht_lock_t* lock;
  bucket_t* bucket;

  do
    {
      hashtable = h->ht;
      size_t bin = ht_hash(hashtable, key);

      bucket = hashtable->table + bin;
#if defined(READ_ONLY_FAIL)
      if (bucket_exists(bucket, key))
	{
	  return false;
	}
#endif
      lock = &bucket->lock;
    }
  while (!LOCK_ACQ(lock, hashtable));

  HYHT_GC_HT_VERSION_USED(hashtable);

  uint32_t j;
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
	      empty_v = (hyht_val_t*) &bucket->val[j];
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
	      bucket->next->val[0] = val;
	    }
	  else 
	    {
	      *empty_v = val;
	      *empty = key;
	    }

	  LOCK_RLS(lock);
	  if (resize)
	    {
	      /* ht_resize_pes(h, 1); */
	      ht_status(h, 1, 0);
	    }
	  return true;
	}
      bucket = bucket->next;
    }
  while (true);
}


/* Remove a key-value entry from a hash table. */
hyht_addr_t
ht_remove(hyht_wrapper_t* h, hyht_addr_t key)
{
  HYHT_CHECK_STATUS(h);

  hashtable_t* hashtable;
  hyht_lock_t* lock;
  bucket_t* bucket;
  do
    {
      hashtable = h->ht;
      size_t bin = ht_hash(hashtable, key);

      bucket = hashtable->table + bin;
#if defined(READ_ONLY_FAIL)
      if (!bucket_exists(bucket, key))
	{
	  return false;
	}
#endif
      lock = &bucket->lock;
    }
  while (!LOCK_ACQ(lock, hashtable));

  HYHT_GC_HT_VERSION_USED(hashtable);

  uint32_t j;
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
ht_put_seq(hashtable_t* hashtable, hyht_addr_t key, hyht_val_t val, uint32_t bin) 
{
  bucket_t* bucket = hashtable->table + bin;
  uint32_t j;

  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == 0)
	    {
	      bucket->key[j] = key;
	      bucket->val[j] = val;
	      return true;
	    }
	}
        
      if (bucket->next == NULL)
	{
	  DPP(put_num_failed_expand);
	  int null;
	  bucket->next = create_bucket_stats(hashtable, &null);
	  bucket->next->key[0] = key;
	  bucket->next->val[0] = val;
	  return true;
	}

      bucket = bucket->next;
    } while (true);
}


static int
bucket_cpy(bucket_t* bucket, hashtable_t* ht_new)
{
  if (!LOCK_ACQ_RES(&bucket->lock))
    {
      return 0;
    }
  uint32_t j;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  hyht_addr_t key = bucket->key[j];
	  if (key != 0) 
	    {
	      uint32_t bin = ht_hash(ht_new, key);
	      ht_put_seq(ht_new, key, bucket->val[j], bin);
	    }
	}
      bucket = bucket->next;
    } 
  while (bucket != NULL);

  return 1;
}


void
ht_resize_help(hashtable_t* h)
{
  if ((int32_t) FAD_U32((volatile uint32_t*) &h->is_helper) <= 0)
    {
      return;
    }

  int32_t b;
  /* hash = num_buckets - 1 */
  for (b = h->hash; b >= 0; b--)
    {
      bucket_t* bu_cur = h->table + b;
      if (!bucket_cpy(bu_cur, h->table_tmp))
	{	    /* reached a point where the resizer is handling */
	  printf("** helped with #buckets: %zu\n", h->num_buckets - b);
	  break;
	}
    }

  h->helper_done = 1;
}

int 
ht_resize_pes(hyht_wrapper_t* h, int is_increase, int by)
{
  ticks s = getticks();

  check_ht_status_steps = HYHT_STATUS_INV;

  hashtable_t* ht_old = h->ht;

  if (TRYLOCK_ACQ(&h->resize_lock))
    {
      return 0;
    }

  size_t num_buckets_new;
  if (is_increase == true)
    {
      /* num_buckets_new = HYHT_RATIO_DOUBLE * ht_old->num_buckets; */
      num_buckets_new = by * ht_old->num_buckets;
    }
  else
    {
#if HYHT_HELP_RESIZE == 1
      ht_old->is_helper = 0;
#endif
      num_buckets_new = ht_old->num_buckets / HYHT_RATIO_HALVE;
    }

  printf("// resizing: from %8zu to %8zu buckets\n", ht_old->num_buckets, num_buckets_new);

  hashtable_t* ht_new = ht_create(num_buckets_new);
  ht_new->version = ht_old->version + 1;

#if HYHT_HELP_RESIZE == 1
  ht_old->table_tmp = ht_new; 

  int32_t b;
  for (b = 0; b < ht_old->num_buckets; b++)
    {
      bucket_t* bu_cur = ht_old->table + b;
      if (!bucket_cpy(bu_cur, ht_new)) /* reached a point where the helper is handling */
	{
	  break;
	}
    }

  if (is_increase && ht_old->is_helper != 1)	/* there exist a helper */
    {
      while (ht_old->helper_done != 1)
	{
	  _mm_pause();
	}
    }

#else

  int32_t b;
  for (b = 0; b < ht_old->num_buckets; b++)
    {
      bucket_t* bu_cur = ht_old->table + b;
      bucket_cpy(bu_cur, ht_new);
    }
#endif

#if defined(DEBUG)
  /* if (ht_size(ht_old) != ht_size(ht_new)) */
  /*   { */
  /*     printf("**ht_size(ht_old) = %zu != ht_size(ht_new) = %zu\n", ht_size(ht_old), ht_size(ht_new)); */
  /*   } */
#endif

  ht_new->table_prev = ht_old;

  int ht_resize_again = 0;
  if (ht_new->num_expands >= ht_new->num_expands_threshold)
    {
      printf("--problem: have already %u expands\n", ht_new->num_expands);
      ht_resize_again = 1;
      /* ht_new->num_expands_threshold = ht_new->num_expands + 1; */
    }

  
  SWAP_PTR(h, ht_new);
  ht_old->table_new = ht_new;
  TRYLOCK_RLS(h->resize_lock);

  ticks e = getticks() - s;
  printf("   resize:: took: %20llu = %9.6f\n", (unsigned long long) e, e / 2.1e9);

  s = getticks();
  ht_gc_collect(h);
  e = getticks() - s;
  printf("   gc col:: took: %20llu = %9.6f\n", (unsigned long long) e, e / 2.1e9);

  if (ht_resize_again)
    {
      ht_status(h, 1, 0);
    }

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

size_t
ht_status(hyht_wrapper_t* h, int resize_increase, int just_print)
{
  hashtable_t* hashtable = h->ht;
  uint32_t num_buckets = hashtable->num_buckets;
  bucket_t* bucket = NULL;
  size_t size = 0;
  int expands = 0;
  int expands_max = 0;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;

      int expands_cont = -1;
      expands--;
      uint32_t j;
      do
	{
	  expands_cont++;
	  expands++;
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

      if (expands_cont > expands_max)
	{
	  expands_max = expands_cont;
	}
    }

  double full_ratio = 100.0 * size / ((hashtable->num_buckets) * ENTRIES_PER_BUCKET);

  if (just_print)
    {
      printf("* #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d / max expands: %2d\n",
	     hashtable->num_buckets, size, full_ratio, expands, expands_max);
    }
  else
    {
      if (full_ratio > 0 && full_ratio < HYHT_PERC_FULL_HALVE)
	{
	  printf("* #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d / max expands: %2d\n",
		 hashtable->num_buckets, size, full_ratio, expands, expands_max);
	  ht_resize_pes(h, 0, 33);
	}
      else if ((full_ratio > 0 && full_ratio > HYHT_PERC_FULL_DOUBLE) || expands_max > HYHT_MAX_EXPANSIONS ||
	       resize_increase)
	{
	  int inc_by = full_ratio / 50;
	  int inc_by_pow2 = pow2roundup(inc_by);

	  printf("* #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d / max expands: %2d / inc by: %3d (pow2) %d\n",
		 hashtable->num_buckets, size, full_ratio, expands, expands_max, inc_by, inc_by_pow2);
	  if (inc_by_pow2 == 1)
	    {
	      inc_by_pow2 = 2;
	    }
	  ht_resize_pes(h, 1, inc_by_pow2);
	}
    }

  if (!just_print)
    {
      ht_gc_collect(h);
    }
  return size;
}


size_t
ht_size_mem(hashtable_t* h) /* in bytes */
{
  if (h == NULL)
    {
      return 0;
    }

  size_t size_tot = sizeof(hashtable_t**);
  size_tot += (h->num_buckets + h->num_expands) * sizeof(bucket_t);
  return size_tot;
}

size_t
ht_size_mem_garbage(hashtable_t* h) /* in bytes */
{
  if (h == NULL)
    {
      return 0;
    }

  size_t size_tot = 0;
  hashtable_t* cur = h->table_prev;
  while (cur != NULL)
    {
      size_tot += ht_size_mem(cur);
      cur = cur->table_prev;
    }

  return size_tot;
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
