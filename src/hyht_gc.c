#include "dht_res.h"
#include <assert.h>
#include <malloc.h>

static __thread ht_ts_t* hyht_ts_thread = NULL;

/* 
 * initialize thread metadata for GC
 */
void
ht_gc_thread_init(hashtable_t* h, int id)
{
  ht_ts_t* ts = (ht_ts_t*) memalign(CACHE_LINE_SIZE, sizeof(ht_ts_t));
  assert(ts != NULL);

  ts->version = h->version;
  ts->id = id;

do
  {
    ts->next = h->version_list;
  }
 while (CAS_U64((volatile size_t*) &h->version_list, (size_t) ts->next, (size_t) ts) != (size_t) ts->next);

 hyht_ts_thread = ts;
}

/* 
 * set the ht version currently used by the current thread
 */
inline void
ht_gc_thread_version(hashtable_t* h)
{
  hyht_ts_thread->version = h->version;
}

/* 
 * get the GC id of the current thread
 */
inline int 
hyht_gc_get_id()
{
  return hyht_ts_thread->id;
}

static int ht_gc_collect_cond(hashtable_t* hashtable, int collect_not_referenced_only);

/* 
 * perform a GC of the versions of the ht that are not currently used by any
 * of the participating threads
 */
inline int
ht_gc_collect(hashtable_t* hashtable)
{
#if HYHT_DO_GC == 1
  HYHT_GC_HT_VERSION_USED(hashtable);
  return ht_gc_collect_cond(hashtable, 1);
#else
  return 0;
#endif
}

/* 
 * perform a GC of ALL old versions of the ht, regardless if they are
 * referenced by any of the threads
 */
int
ht_gc_collect_all(hashtable_t* hashtable)
{
  return ht_gc_collect_cond(hashtable, 0);
}

#define GET_ID(x) x ? hyht_gc_get_id() : 99

/* 
 * go over the version metadata of all threads and return the min ht
 * version that is currently used. In other words, all versions, less
 * than the returned value, can be GCed
 */
static inline size_t
ht_gc_min_version_used(hashtable_t* h)
{
  volatile ht_ts_t* cur = h->version_list;

  size_t min = h->version;
  while (cur != NULL)
    {
      if (cur->version < min)
	{
	  min = cur->version;
	}
      cur = cur->next;
    }

  return min;
}

/* 
 * GC help function:
 * collect_not_referenced_only == 0 -> ht_gc_collect_all();
 * collect_not_referenced_only != 0 -> ht_gc_collect();
 */
static int
ht_gc_collect_cond(hashtable_t* hashtable, int collect_not_referenced_only)
{
  if (TRYLOCK_ACQ(&hashtable->gc_lock))
    {
      /* printf("** someone else is performing gc\n"); */
      return 0;
    }

  /* printf("[GC-%02d] LOCK  : %zu\n", GET_ID(collect_not_referenced_only), hashtable->version); */

  size_t version_min = hashtable->version; 
  if (collect_not_referenced_only)
    {
      version_min = ht_gc_min_version_used(hashtable);
    }

  /* printf("[GC-%02d] gc collect versions < %3zu - current: %3zu - oldest: %zu\n",  */
  /* 	 GET_ID(collect_not_referenced_only), version_min, hashtable->version, hashtable->version_min); */

  int gced = 0;

  if (hashtable->version_min >= version_min)
    {
      /* printf("[GC-%02d] UNLOCK: %zu (nothing to collect)\n", GET_ID(collect_not_referenced_only), hashtable->version); */
      TRYLOCK_RLS(hashtable->gc_lock);
    }
  else
    {
      /* printf("[GC-%02d] collect from %zu to %zu\n", GET_ID(collect_not_referenced_only), hashtable->version_min, version_min); */

      int gc_locks = 1;
      int gc_locks_num = 1;
      hashtable_t* cur = hashtable->table_prev;

      while (cur != NULL && cur->table_prev != NULL)
	{
	  if (TRYLOCK_ACQ(&cur->gc_lock))
	    {
	      /* printf("[GC-%02d] someone else is performing gc: is locked: %zu\n", GET_ID(collect_not_referenced_only), cur->version); */
	      gc_locks = 0;
	      break;
	    }

	  gc_locks_num++;
	  /* printf("[GC-%02d] LOCK  : %zu\n", GET_ID(collect_not_referenced_only), cur->version); */
	  cur = cur->table_prev;
	}

      if (gc_locks)
	{
	  while (cur != NULL && cur->version < version_min)
	    {
	      gced = 1;
	      hashtable_t* nxt = cur->table_new;
	      printf("[GC-%02d] gc_free version: %6zu | current version: %6zu\n", GET_ID(collect_not_referenced_only),
	      	     cur->version, hashtable->version);
	      nxt->table_prev = NULL;
	      ht_gc_free(cur);
	      gc_locks_num--;
	      cur = nxt;
	    }

	  hashtable->version_min = cur->version;
	}

      if (gc_locks == 0 || gc_locks_num == 1)
	{
	  cur = cur->table_new;
	}

      do
	{
	  TRYLOCK_RLS(cur->gc_lock);
	  /* printf("[GC-%02d] UNLOCK: %zu\n", GET_ID(collect_not_referenced_only), cur->version); */
	  cur = cur->table_new;
	}
      while (cur != NULL && --gc_locks_num > 0);
    }

  return gced;
}

/* 
 * free the given hashtable
 */
int
ht_gc_free(hashtable_t* hashtable)
{
  uint32_t num_buckets = hashtable->num_buckets;
  bucket_t* bucket = NULL;

  uint32_t bin;
  for (bin = 0; bin < num_buckets; bin++)
    {
      bucket = hashtable->table + bin;
       
      bucket_t* bstack[8] = {0};
      int bidx = 0;

      do
	{
	  bucket = bucket->next;
	  bstack[bidx++] = bucket;
	  if (bidx == 8)
	    {
	      /* printf("[GC] stack full\n"); */
	      bidx--;
		while (--bidx >= 0) /* free from 7..0 */
		{
		  if (bstack[bidx] != NULL)
		    {
		      /* printf("[GC] free(%d) = %p\n", bidx, bstack[bidx]); */
		      free(bstack[bidx]);
		    }
		}
	      bstack[0] = bstack[7]; /* do not free the current bucket* */
	      bidx = 1;
	    }
	}
      while (bucket != NULL);

      while(--bidx >= 0)
	{
	  /* printf("[GC] done collecting\n"); */
	  if (bstack[bidx] != NULL)
	    {
	      /* printf("[GC] free(%d) = %p\n", bidx, bstack[bidx]); */
	      free(bstack[bidx]);
	    }
	}
    }

  free(hashtable->table);
  free(hashtable);

  return 1;
}

/* 
 * free all hashtable version (inluding the latest)
 */
void
ht_gc_destroy(hashtable_t** hashtable)
{
  ht_gc_collect_all(*hashtable);
  ht_gc_free(*hashtable);
  free(hashtable);
}

