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

__thread size_t check_ht_status_steps = HYHT_STATUS_INVOK_IN;

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

  bucket->lock = 0;

  uint32_t j;
  for (j = 0; j < ENTRIES_PER_BUCKET; j++)
    {
      bucket->key[j] = 0;
    }
  bucket->next = NULL;

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
  if (w->ht == NULL)
    {
      free(w);
      return NULL;
    }
  w->resize_lock = LOCK_FREE;
  w->gc_lock = LOCK_FREE;
  w->status_lock = LOCK_FREE;
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
      hashtable->table[i].hops = 0;
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  hashtable->table[i].key[j] = 0;
	}

      /* link buckets to their next bucket (and last bucket with the first)*/
      hashtable->table[i].next = &hashtable->table[(i+1) % num_buckets]; 
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
ht_get(hashtable_t* hashtable, hyht_addr_t key)
{
  size_t bin = ht_hash(hashtable, key);
  HYHT_GC_HT_VERSION_USED(hashtable);
  volatile bucket_t* bucket = hashtable->table + bin;

  uint32_t j, hops = bucket->hops;
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
    } while (hops-- > 0);
  return 0;
}

static inline int
bucket_exists(volatile bucket_t* bucket, hyht_addr_t key)
{
  int32_t j, hops = bucket->hops;
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
    } while (hops-- > 0);
  return false;
}

static inline void 
lock_release_n(volatile bucket_t* b, int n)
{
  int i;
  for (i = 0; i < n; i++)
    {
      LOCK_RLS(&b->lock);
      b = b->next;
    }
}

/* Insert a key-value entry into a hash table. */
int
ht_put(hyht_wrapper_t* h, hyht_addr_t key, hyht_val_t val) 
{
  volatile hashtable_t* hashtable;

  int once = 1;

 again:
  hashtable = h->ht;
  size_t bin = ht_hash((hashtable_t*)hashtable, key);
  volatile bucket_t* bucket = hashtable->table + bin;
  volatile bucket_t* bucket_first = bucket;

#if HYHT_READ_ONLY_FAIL == 1
  if (bucket_exists(bucket, key))
    {
      return false;
    }
#endif

  HYHT_GC_HT_VERSION_USED(hashtable);
  HYHT_CHECK_STATUS(h);
  volatile hyht_addr_t* empty = NULL;
  volatile hyht_val_t* empty_v = NULL;

  int l = 0;
  int j;

  assert(bucket_first->hops < (hashtable->num_buckets - 1));

  size_t tot_hops = bucket_first->hops;

  int hops;
  for (hops = 0; hops <= tot_hops; hops++)
    {
      if (!LOCK_ACQ(&bucket->lock, (hashtable_t*) hashtable))
	{
	  lock_release_n(bucket_first, l);
	  while (hashtable->table_new == NULL)
	    {
	      _mm_pause();
	    }
	  goto again;
	}      
      l++;

      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      lock_release_n(bucket_first, l);
	      return false;
	    }
	  else if (empty == NULL && bucket->key[j] == 0)
	    {
	      empty = &bucket->key[j];
	      empty_v = &bucket->val[j];
	    }
	}
        
      bucket = bucket->next;
    }


  if (empty == NULL)
    {
      /* just find a free spot */
      uint32_t j;
      do
	{
	  tot_hops++;
	  if (tot_hops >= ((hashtable->num_buckets/2) - 1))
	    {
	      if (once)
		{
		  printf(" ** emergency // total hops: %zu \n", tot_hops);
		  once = 0;
		}
	      lock_release_n(bucket_first, l);

	      ht_status(h, 0, 0);
	      while (hashtable->table_new == NULL)
		{
		  _mm_pause();
		}
	      goto again;
	    }

	  if (!LOCK_ACQ(&bucket->lock, (hashtable_t*) hashtable))
	    {
	      lock_release_n(bucket_first, l);
	      while (hashtable->table_new == NULL)
		{
		  _mm_pause();
		}
	      goto again;
	    }      
	  l++;  
	  for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	    {
	      if (bucket->key[j] == 0)
		{
		  empty = &bucket->key[j];
		  empty_v = &bucket->val[j];
		  break;
		}
	    }
	  bucket = bucket->next;
	  assert(l < hashtable->num_buckets-1);
	}
      while (empty == NULL);

      bucket_first->hops = tot_hops;

    }

  *empty_v = val;
  *empty = key;

  lock_release_n(bucket_first, l);

  if (unlikely(bucket_first->hops > 12))
    {
      ht_status(h, 0, 0);
    }

  return true;
}


/* Remove a key-value entry from a hash table. */
hyht_val_t
ht_remove(hyht_wrapper_t* h, hyht_addr_t key)
{
  volatile hashtable_t* hashtable;
 again:
  hashtable = h->ht;
  size_t bin = ht_hash((hashtable_t*) hashtable, key);
  volatile bucket_t* bucket = hashtable->table + bin;
  volatile bucket_t* bucket_first = bucket;

#if HYHT_READ_ONLY_FAIL == 1
  if (!bucket_exists(bucket, key))
    {
      return false;
    }
#endif

  HYHT_GC_HT_VERSION_USED(hashtable);
  HYHT_CHECK_STATUS(h);

  int l = 0;
  int j;
  int hops;
  for (hops = 0; hops <= bucket_first->hops; hops++)
    {
      if (!LOCK_ACQ(&bucket->lock, (hashtable_t*) hashtable))
	{
	  lock_release_n(bucket_first, l);
	  goto again;
	}      
      l++;

      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == key) 
	    {
	      hyht_val_t val = bucket->val[j];
	      bucket->key[j] = 0;
	      lock_release_n(bucket_first, l);
	      return val;
	    }
	}
      bucket = bucket->next;
    } 

  lock_release_n(bucket_first, l);
  return false;
}

static uint32_t
ht_put_seq(hashtable_t* hashtable, hyht_addr_t key, hyht_val_t val, uint32_t bin) 
{
  volatile bucket_t* bucket = hashtable->table + bin;
  volatile bucket_t* bucket_first = bucket;
  uint32_t j;

  int tr = 0;
  do 
    {
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
	{
	  if (bucket->key[j] == 0)
	    {
	      bucket->val[j] = val;
	      bucket->key[j] = key;
	      int diff = tr - bucket_first->hops;
	      if (diff > 0)
		{
		  bucket_first->hops = tr;
		  hashtable->num_expands += diff;
		}
	      return true;
	    }
	}
        
      tr++;
      bucket = bucket->next;
    } 
  while (true);
}


static int
bucket_cpy(volatile bucket_t* bucket, hashtable_t* ht_new)
{
  if (!LOCK_ACQ_RES(&bucket->lock))
    {
      return 0;
    }
  uint32_t j;
  for (j = 0; j < ENTRIES_PER_BUCKET; j++) 
    {
      hyht_addr_t key = bucket->key[j];
      if (key != 0) 
	{
	  uint32_t bin = ht_hash(ht_new, key);
	  ht_put_seq(ht_new, key, bucket->val[j], bin);
	}
    }
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
	  /* printf("[GC-%02d] helped  #buckets: %10zu = %5.1f%%\n",  */
	  /* 	 hyht_gc_get_id(), h->num_buckets - b, 100.0 * (h->num_buckets - b) / h->num_buckets); */
	  break;
	}
    }

  h->helper_done = 1;
}

int 
ht_resize_pes(hyht_wrapper_t* h, int is_increase, int by)
{
  ticks s = getticks();

  check_ht_status_steps = HYHT_STATUS_INVOK;

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

  int one_p = ht_old->num_buckets / 100;

  /* printf("RES%%"); fflush(stdout); */
  int32_t b;
  for (b = 0; b < ht_old->num_buckets; b++)
    {
      bucket_t* bu_cur = ht_old->table + b;
      bucket_cpy(bu_cur, ht_new);
      /* if (b % one_p == 0) */
      /* 	{ */
      /* 	  printf("|"); fflush(stdout); */
      /* 	} */
    }
  /* printf("\n"); */
#endif

#if defined(DEBUG)
  /* if (ht_size(ht_old) != ht_size(ht_new)) */
  /*   { */
  /*     printf("**ht_size(ht_old) = %zu != ht_size(ht_new) = %zu\n", ht_size(ht_old), ht_size(ht_new)); */
  /*   } */
#endif

  ht_new->table_prev = ht_old;

  double avg_expands = ht_new->num_expands / (double) ht_new->num_buckets;
  int ht_resize_again = 0;
  if (avg_expands >= 1)
    {
      printf("--problem: have already %.1f avg. expands\n", avg_expands);
      ht_resize_again = 1;
      /* ht_new->num_expands_threshold = ht_new->num_expands + 1; */
    }

  
  SWAP_U64((uint64_t*) h, (uint64_t) ht_new);
  ht_old->table_new = ht_new;
  TRYLOCK_RLS(h->resize_lock);

  ticks e = getticks() - s;
  double mba = (ht_new->num_buckets * 64) / (1024.0 * 1024);
  printf("[RESIZE-%02d] to #bu %7zu = MB: %7.2f    | took: %13llu ti = %8.6f s\n", 
	 hyht_gc_get_id(), ht_new->num_buckets, mba, (unsigned long long) e, e / 2.1e9);

  ht_gc_collect(h);

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
  volatile bucket_t* bucket = NULL;
  size_t size = 0;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;
       
      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  if (bucket->key[j] > 0)
	    {
	      size++;
	    }
	}
    }
  return size;
}

size_t
ht_status(hyht_wrapper_t* h, int resize_increase, int just_print)
{
  if (TRYLOCK_ACQ(&h->status_lock) && !resize_increase)
    {
      return 0;
    }

  hashtable_t* hashtable = h->ht;
  uint32_t num_buckets = hashtable->num_buckets;
  volatile bucket_t* bucket = NULL;
  size_t size = 0;
  int expands = 0;
  int expands_max = 0;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;

      expands += bucket->hops;
      if (bucket->hops > expands_max)
	{
	  expands_max = bucket->hops;
	}

      uint32_t j;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++)
	{
	  if (bucket->key[j] > 0)
	    {
	      size++;
	    }
	}
    }

  double full_ratio = 100.0 * size / ((hashtable->num_buckets) * ENTRIES_PER_BUCKET);

  if (just_print)
    {
      printf("[STATUS-%02d] #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d / avg expands: %.1f / max expands: %2d\n",
	     99, hashtable->num_buckets, size, full_ratio, expands, (double) expands / hashtable->num_buckets, expands_max);
    }
  else
    {
      if (full_ratio > 0 && full_ratio < HYHT_PERC_FULL_HALVE)
	{
	  printf("[STATUS-%02d] #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d / max expands: %2d\n",
		 hyht_gc_get_id(), hashtable->num_buckets, size, full_ratio, expands, expands_max);
	  ht_resize_pes(h, 0, 33);
	}
      else if ((full_ratio > 0 && full_ratio > HYHT_LINKED_PERC_FULL_DOUBLE) || expands_max > HYHT_LINKED_MAX_EXPANSIONS ||
	       ((double) expands / hashtable->num_buckets) > HYHT_LINKED_MAX_AVG_EXPANSION || resize_increase)
	{
	  int inc_by = (full_ratio / 33) + resize_increase;
	  int inc_by_pow2 = pow2roundup(inc_by);

	  printf("[STATUS-%02d] #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: %4d /avg expands: %.1f / max expands: %2d\n",
		 hyht_gc_get_id(), hashtable->num_buckets, size, full_ratio, 
		 expands, (double) expands / hashtable->num_buckets, expands_max);
	  ht_resize_pes(h, 1, inc_by_pow2);
	}
    }

  if (!just_print)
    {
      ht_gc_collect(h);
    }

  TRYLOCK_RLS(h->status_lock);
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
  size_tot += h->num_buckets * sizeof(bucket_t);
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
  volatile bucket_t* bucket;

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

	  bucket = bucket->next;
	  printf(" ** -> ");
	}
      while (bucket != NULL);
      printf("\n");
    }
  fflush(stdout);
}
