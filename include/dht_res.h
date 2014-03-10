#ifndef _DHT_RES_H_
#define _DHT_RES_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "atomic_ops.h"
#include "utils.h"

#include "ssmem.h"

extern __thread ssmem_allocator_t* hyht_alloc;

#define true 1
#define false 0

/* #define DEBUG */

#define HYHT_READ_ONLY_FAIL   1
#define HYHT_HELP_RESIZE      0
#define HYHT_PERC_EXPANSIONS  3
#define HYHT_MAX_EXPANSIONS   24
#define HYHT_PERC_FULL_DOUBLE 95	   /* % */
#define HYHT_RATIO_DOUBLE     2		  
#define HYHT_PERC_FULL_HALVE  5		   /* % */
#define HYHT_RATIO_HALVE      8		  
#define HYHT_MIN_HT_SIZE      8
#define HYHT_DO_CHECK_STATUS  0
#define HYHT_DO_GC            0
#define HYHT_STATUS_INVOK     500000
#define HYHT_STATUS_INVOK_IN  500000
#if defined(RTM)	       /* only for processors that have RTM */
#define HYHT_USE_RTM          1
#else
#define HYHT_USE_RTM          0
#endif

#if HYHT_DO_CHECK_STATUS == 1
#  define HYHT_CHECK_STATUS(h)				\
  if (unlikely((--check_ht_status_steps) == 0))		\
    {							\
      ht_status(h, 0, 0);				\
      check_ht_status_steps = HYHT_STATUS_INVOK;	\
    }

#else 
#  define HYHT_CHECK_STATUS(h)
#endif

#if HYHT_DO_GC == 1
#  define HYHT_GC_HT_VERSION_USED(ht) ht_gc_thread_version((hashtable_t*) ht)
#else
#  define HYHT_GC_HT_VERSION_USED(ht)
#endif


/* HYHT LINKED version specific parameters */
#define HYHT_LINKED_PERC_FULL_DOUBLE       75
#define HYHT_LINKED_MAX_AVG_EXPANSION      1
#define HYHT_LINKED_MAX_EXPANSIONS         7
#define HYHT_LINKED_MAX_EXPANSIONS_HARD    16
#define HYHT_LINKED_EMERGENCY_RESIZE       4 /* how many times to increase the size on emergency */
/* *************************************** */

#if defined(DEBUG)
#  define DPP(x)	x++				
#else
#  define DPP(x)
#endif

#define CACHE_LINE_SIZE    64
#define ENTRIES_PER_BUCKET 3

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif

#define likely(x)       __builtin_expect((x), 1)
#define unlikely(x)     __builtin_expect((x), 0)

#if defined(__sparc__)
#  define PREFETCHW(x) 
#  define PREFETCH(x) 
#  define PREFETCHNTA(x) 
#  define PREFETCHT0(x) 
#  define PREFETCHT1(x) 
#  define PREFETCHT2(x) 

#  define PAUSE    asm volatile("rd    %%ccr, %%g0\n\t" \
				::: "memory")
#  define _mm_pause() PAUSE
#  define _mm_mfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore");
#  define _mm_lfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore");
#  define _mm_sfence() __asm__ __volatile__("membar #StoreLoad | #StoreStore");


#elif defined(__tile__)
#  define _mm_lfence() arch_atomic_read_barrier()
#  define _mm_sfence() arch_atomic_write_barrier()
#  define _mm_mfence() arch_atomic_full_barrier()
#  define _mm_pause() cycle_relax()
#endif

#define CAS_U64_BOOL(a, b, c) (CAS_U64(a, b, c) == b)
inline int is_power_of_two(unsigned int x);

typedef uintptr_t hyht_addr_t;
typedef volatile uintptr_t hyht_val_t;

#if defined(__tile__)
typedef volatile uint32_t hyht_lock_t;
#else
typedef volatile uint8_t hyht_lock_t;
#endif

typedef struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  hyht_lock_t lock;
  volatile uint32_t hops;
  hyht_addr_t key[ENTRIES_PER_BUCKET];
  hyht_val_t val[ENTRIES_PER_BUCKET];
  volatile struct bucket_s* next;
} bucket_t;

#if __GNUC__ > 4 && __GNUC_MINOR__ > 4
_Static_assert (sizeof(bucket_t) % 64 == 0, "sizeof(bucket_t) == 64");
#endif

typedef struct ALIGNED(CACHE_LINE_SIZE) hyht_wrapper
{
  union
  {
    struct
    {
      struct hashtable_s* ht;
      uint8_t next_cache_line[CACHE_LINE_SIZE - (sizeof(void*))];
      struct hashtable_s* ht_oldest;
      struct ht_ts* version_list;
      size_t version_min;
      volatile hyht_lock_t resize_lock;
      volatile hyht_lock_t gc_lock;
      volatile hyht_lock_t status_lock;
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} hyht_wrapper_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) hashtable_s
{
  union
  {
    struct
    {
      size_t num_buckets;
      bucket_t* table;
      size_t hash;
      size_t version;
      uint8_t next_cache_line[CACHE_LINE_SIZE - (3 * sizeof(size_t)) - (sizeof(void*))];
      struct hashtable_s* table_tmp;
      struct hashtable_s* table_prev;
      struct hashtable_s* table_new;
      volatile uint32_t num_expands;
      union
      {
	volatile uint32_t num_expands_threshold;
	uint32_t num_buckets_prev;
      };
      volatile int32_t is_helper;
      volatile int32_t helper_done;
      size_t version_min;
    };
    uint8_t padding[2*CACHE_LINE_SIZE];
  };
} hashtable_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) ht_ts
{
  union
  {
    struct
    {
      size_t version;
      hashtable_t* versionp;
      int id;
      volatile struct ht_ts* next;
    };
    uint8_t padding[CACHE_LINE_SIZE];
  };
} ht_ts_t;


inline uint64_t __ac_Jenkins_hash_64(uint64_t key);

/* Hash a key for a particular hashtable. */
uint32_t ht_hash(hashtable_t* hashtable, hyht_addr_t key );

static inline void
_mm_pause_rep(uint64_t w)
{
  while (w--)
    {
      _mm_pause();
    }
}

#if defined(XEON) | defined(COREi7) 
#  define TAS_RLS_MFENCE() _mm_sfence();
#elif defined(__tile__)
#  define TAS_RLS_MFENCE() _mm_mfence();
#else
#  define TAS_RLS_MFENCE()
#endif

#define LOCK_FREE   0
#define LOCK_UPDATE 1
#define LOCK_RESIZE 2

#if HYHT_USE_RTM == 1		/* USE RTM */
#  define LOCK_ACQ(lock, ht)			\
  lock_acq_rtm_chk_resize(lock, ht)
#  define LOCK_RLS(lock)			\
  if (likely(*(lock) == LOCK_FREE))		\
    {						\
      _xend();					\
      DPP(put_num_failed_on_new);		\
    }						\
  else						\
    {						\
      TAS_RLS_MFENCE();				\
     *lock = LOCK_FREE;				\
      DPP(put_num_failed_expand);		\
    }
#else  /* NO RTM */
#  define LOCK_ACQ(lock, ht)			\
  lock_acq_chk_resize(lock, ht)

#  define LOCK_RLS(lock)			\
  TAS_RLS_MFENCE();				\
 *lock = 0;	  

#endif	/* RTM */

#define LOCK_ACQ_RES(lock)			\
  lock_acq_resize(lock)

#define TRYLOCK_ACQ(lock)			\
  TAS_U8(lock)

#define TRYLOCK_RLS(lock)			\
  lock = LOCK_FREE

void ht_resize_help(hashtable_t* h);

#if defined(DEBUG)
extern __thread uint32_t put_num_restarts;
#endif

static inline int
lock_acq_chk_resize(hyht_lock_t* lock, hashtable_t* h)
{
  char once = 1;
  hyht_lock_t l;
  while ((l = CAS_U8(lock, LOCK_FREE, LOCK_UPDATE)) == LOCK_UPDATE)
    {
      if (once)
      	{
      	  DPP(put_num_restarts);
      	  once = 0;
      	}
      _mm_pause();
    }

  if (l == LOCK_RESIZE)
    {
      /* helping with the resize */
#if HYHT_HELP_RESIZE == 1
      ht_resize_help(h);
#endif

#if !defined(HYHT_LINKED)
      while (h->table_new == NULL)
	{
	  _mm_mfence();
	}
#endif

      return 0;
    }

  return 1;
}

static inline int
lock_acq_resize(hyht_lock_t* lock)
{
  hyht_lock_t l;
  while ((l = CAS_U8(lock, LOCK_FREE, LOCK_RESIZE)) == LOCK_UPDATE)
    {
      _mm_pause();
    }

  if (l == LOCK_RESIZE)
    {
      return 0;
    }

  return 1;
}


/* ******************************************************************************** */
#if HYHT_USE_RTM == 1  /* use RTM */
/* ******************************************************************************** */

#include <immintrin.h>		/*  */

static inline int
lock_acq_rtm_chk_resize(hyht_lock_t* lock, hashtable_t* h)
{

  int rtm_retries = 1;
  do 
    {
      /* while (unlikely(*lock == LOCK_UPDATE)) */
      /* 	{ */
      /* 	  _mm_pause(); */
      /* 	} */

      if (likely(_xbegin() == _XBEGIN_STARTED))
	{
	  hyht_lock_t lv = *lock;
	  if (likely(lv == LOCK_FREE))
	    {
	      return 1;
	    }
	  else if (lv == LOCK_RESIZE)
	    {
	      _xend();
#  if HYHT_HELP_RESIZE == 1
	      ht_resize_help(h);
#  endif

	      while (h->table_new == NULL)
		{
		  _mm_mfence();
		}

	      return 0;
	    }

	  DPP(put_num_restarts);
	  _xabort(0xff);
	}
    } while (rtm_retries-- > 0);

  return lock_acq_chk_resize(lock, h);
}
#endif	/* RTM */


/* ******************************************************************************** */
/* intefance */
/* ******************************************************************************** */

/* Create a new hashtable. */
hashtable_t* ht_create(uint32_t num_buckets);
hyht_wrapper_t* hyht_wrapper_create(uint32_t num_buckets);

/* Insert a key-value pair into a hashtable. */
int ht_put(hyht_wrapper_t* hashtable, hyht_addr_t key, hyht_val_t val);

/* Retrieve a key-value pair from a hashtable. */
hyht_val_t ht_get(hashtable_t* hashtable, hyht_addr_t key);

/* Remove a key-value pair from a hashtable. */
hyht_val_t ht_remove(hyht_wrapper_t* hashtable, hyht_addr_t key);

size_t ht_size(hashtable_t* hashtable);
size_t ht_size_mem(hashtable_t* hashtable);
size_t ht_size_mem_garbage(hashtable_t* hashtable);

void ht_gc_thread_init(hyht_wrapper_t* hashtable, int id);
inline void ht_gc_thread_version(hashtable_t* h);
inline int hyht_gc_get_id();
int ht_gc_collect(hyht_wrapper_t* h);
int ht_gc_release(hashtable_t* h);
int ht_gc_collect_all(hyht_wrapper_t* h);
int ht_gc_free(hashtable_t* hashtable);
void ht_gc_destroy(hyht_wrapper_t* hashtable);

void ht_print(hashtable_t* hashtable);
#if defined(HYHT_LINKED)
/* emergency_increase, grabs the lock and forces an increase by *emergency_increase times */
size_t ht_status(hyht_wrapper_t* hashtable, int resize_increase, int emergency_increase, int just_print);
#else
size_t ht_status(hyht_wrapper_t* hashtable, int resize_increase, int just_print);
#endif
bucket_t* create_bucket();
int ht_resize_pes(hyht_wrapper_t* hashtable, int is_increase, int by);


#endif /* _DHT_RES_H_ */

