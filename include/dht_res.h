#ifndef _DHT_RES_H_
#define _DHT_RES_H_

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "atomic_ops.h"
#include "utils.h"

#define true 1
#define false 0

#define READ_ONLY_FAIL
/* #define DEBUG */
#define HYHT_HELP_RESIZE      0
#define HYHT_PERC_EXPANSIONS  0.05

#if defined(DEBUG)
#  define DPP(x)	x++				
#else
#  define DPP(x)
#endif

#define CACHE_LINE_SIZE 64
#define ENTRIES_PER_BUCKET 3

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif

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
inline int is_power_of_two (unsigned int x);

typedef uintptr_t ssht_addr_t;

typedef uint64_t lock_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  lock_t lock;
  ssht_addr_t key[ENTRIES_PER_BUCKET];
  void* val[ENTRIES_PER_BUCKET];
  struct bucket_s* next;
} bucket_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) hashtable_s
{
  union
  {
    struct
    {
      size_t num_buckets;
      bucket_t* table;
      volatile uint8_t resize_lock;
      struct hashtable_s* table_tmp;
      struct hashtable_s* table_new;
      uint8_t next_cl[64];
      volatile uint32_t num_expands;
      volatile uint32_t num_expands_threshold;
    };
    uint8_t padding[2*CACHE_LINE_SIZE];
  };
} hashtable_t;


/* Hash a key for a particular hashtable. */
uint32_t ht_hash(hashtable_t* hashtable, ssht_addr_t key );

static inline void
_mm_pause_rep(uint64_t w)
{
  while (w--)
    {
      _mm_pause();
    }
}

/* #define TAS_WITH_FAI */
/* #define TAS_WITH_TAS */
#define TAS_WITH_CAS
/* #define TAS_WITH_SWAP */

#if defined(XEON) | defined(COREi7)
#  define TAS_RLS_MFENCE() _mm_mfence();
#else
#  define TAS_RLS_MFENCE()
#endif

#if defined(TAS_WITH_FAI)
#  define LOCK_ACQ(lock)			\
  while (FAI_U64(lock))				\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#elif defined(TAS_WITH_TAS)			
#  define LOCK_ACQ(lock)			\
  while (TAS_U8((uint8_t*) (lock)))		\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#elif defined(TAS_WITH_SWAP)			
#  define LOCK_ACQ(lock)			\
  while (SWAP_U64(lock, 1))			\
    {						\
      _mm_pause();				\
      DPP(put_num_restarts);			\
    }						
#elif defined(TAS_WITH_CAS)

#define LOCK_FREE   0
#define LOCK_UPDATE 1
#define LOCK_RESIZE 2

#  define LOCK_ACQ(lock, bu, ht, key)		\
  lock_acq_chk_resize(lock, bu, ht, key)
#  define LOCK_ACQ_RES(lock)			\
  lock_acq_resize(lock)

static inline bucket_t*
lock_acq_chk_resize(lock_t* lock, bucket_t* b, hashtable_t* h, ssht_addr_t k)
{
  lock_t l;
  while ((l = CAS_U8(lock, LOCK_FREE, LOCK_UPDATE)) == LOCK_UPDATE)
    {
      _mm_pause_rep(16);
    }

  if (l == LOCK_RESIZE)
    {
      while (h->table_new == NULL)
	{
	  _mm_mfence();
	}

      size_t bin = ht_hash(h->table_new, k);
      bucket_t* bnew = h->table_new->table + bin;
      return lock_acq_chk_resize(&bnew->lock, bnew, h->table_new, k);
    }

  return b;
}

static inline void
lock_acq_resize(lock_t* lock)
{
  while (CAS_U8(lock, LOCK_FREE, LOCK_RESIZE) != LOCK_FREE)
    {
      _mm_pause_rep(16);
    }
}


#endif

#define LOCK_RLS(lock)				\
  TAS_RLS_MFENCE();				\
 *lock = 0;	  

/* Create a new hashtable. */
hashtable_t* ht_create(uint32_t num_buckets );

/* Insert a key-value pair into a hashtable. */
uint32_t ht_put(hashtable_t** hashtable, ssht_addr_t key);

/* Retrieve a key-value pair from a hashtable. */
void* ht_get(hashtable_t** hashtable, ssht_addr_t key);

/* Remove a key-value pair from a hashtable. */
ssht_addr_t ht_remove(hashtable_t** hashtable, ssht_addr_t key);

/* Dealloc the hashtable */
void ht_destroy(hashtable_t** hashtable);

size_t ht_size(hashtable_t* hashtable);

void ht_print(hashtable_t* hashtable);

bucket_t* create_bucket();
int ht_resize_pes(hashtable_t** h);


#endif /* _DHT_RES_H_ */
