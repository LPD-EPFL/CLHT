#ifndef _LFHT_RES_H_
#define _LFHT_RES_H_

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

#if defined(DEBUG)
#  define DPP(x)	x++				
#else
#  define DPP(x)
#endif

#define CACHE_LINE_SIZE    64

#define MAP_INVLD 0
#define MAP_VALID 1
#define MAP_INSRT 2

#define KEY_BUCKT 3
#define ENTRIES_PER_BUCKET KEY_BUCKT

#define LFHT_DO_GC               1
#define LFHT_PERC_FULL_HALVE     2
#define LFHT_PERC_FULL_DOUBLE    15
#define LFHT_OCCUP_AFTER_RES     40
#define LFHT_INC_EMERGENCY       2
#define LFHT_NO_EMPTY_SLOT_TRIES 3
#define LFHT_GC_HT_VERSION_USED(ht) ht_gc_thread_version(ht)


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
typedef uint64_t lfht_snapshot_all_t;

typedef union
{
  volatile uint64_t snapshot;
  struct
  {
#if KEY_BUCKT == 4
    uint32_t version;
#elif KEY_BUCKT == 6
    uint16_t version;
#else
    uint32_t version;
#endif
    uint8_t map[KEY_BUCKT];
  };
} lfht_snapshot_t;

#if __GNUC__ > 4 && __GNUC_MINOR__ > 4
_Static_assert (sizeof(lfht_snapshot_t) == 8, "sizeof(lfht_snapshot_t) == 8");
#endif

typedef volatile struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  union
  {
    volatile uint64_t snapshot;
    struct
    {
#if KEY_BUCKT == 4
      uint32_t version;
#elif KEY_BUCKT == 6
      uint16_t version;
#else
      uint32_t version;
/* #  error "KEY_BUCKT should be either 4 or 6" */
#endif
      uint8_t map[KEY_BUCKT];
    };
  };
  hyht_addr_t key[KEY_BUCKT];
  hyht_val_t val[KEY_BUCKT];
} bucket_t;

#if __GNUC__ > 4 && __GNUC_MINOR__ > 4
_Static_assert (sizeof(bucket_t) % 64 == 0, "sizeof(bucket_t) == 64");
#endif

#if defined(__tile__)
typedef volatile uint32_t hyht_lock_t;
#else
typedef volatile uint8_t hyht_lock_t;
#endif
/* typedef volatile uint64_t lfht_lock_t; */
#define LFHT_LOCK_FREE 0
#define LFHT_LOCK_ACQR 1

#define LFHT_CHECK_RESIZE(w)				\
  while (unlikely(w->resize_lock == LFHT_LOCK_ACQR))	\
    {							\
      _mm_pause();					\
      LFHT_GC_HT_VERSION_USED(w->ht);			\
    }

#define LFHT_LOCK_RESIZE(w)						\
  (CAS_U8(&w->resize_lock, LFHT_LOCK_FREE, LFHT_LOCK_ACQR) == LFHT_LOCK_FREE)

#define LFHT_RLS_RESIZE(w)			\
  w->resize_lock = LFHT_LOCK_FREE

#define TRYLOCK_ACQ(lock)			\
  TAS_U8(lock)

#define TRYLOCK_RLS(lock)			\
  lock = LFHT_LOCK_FREE


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

inline uint64_t __ac_Jenkins_hash_64(uint64_t key);

/* Hash a key for a particular hashtable. */
uint32_t ht_hash(hashtable_t* hashtable, hyht_addr_t key );


static inline int
snap_get_empty_index(uint64_t snap)
{
  lfht_snapshot_t s = { .snapshot = snap };
  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (s.map[i] == MAP_INVLD)
	{
	  return i;
	}
    }
  return -1;
}

static inline int
keys_get_empty_index(hyht_addr_t* keys)
{
  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (keys[i] == 0)
	{
	  return i;
	}
    }
  return -1;
}

static inline int
buck_get_empty_index(bucket_t* b, uint64_t snap)
{
  lfht_snapshot_t s = { .snapshot = snap };

  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (b->key[i] == 0 && s.map[i] != MAP_INSRT)
	{
	  return i;
	}
    }
  return -1;
}


static inline int
vals_get_empty_index(hyht_val_t* vals, lfht_snapshot_all_t snap)
{
  lfht_snapshot_t s = { .snapshot = snap };

  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (vals[i] == 0 && s.map[i] != MAP_INSRT)
	{
	  return i;
	}
    }
  return -1;
}


static inline uint64_t
snap_set_map(uint64_t s, int index, int val)
{
  lfht_snapshot_t s1 = { .snapshot = s };
  s1.map[index] = val;
  return s1.snapshot;
}

static inline uint64_t
snap_set_map_and_inc_version(uint64_t s, int index, int val)
{
  lfht_snapshot_t s1 = { .snapshot =  s};
  s1.map[index] = val;
  s1.version++;
  return s1.snapshot;
}

static inline void
_mm_pause_rep(uint64_t w)
{
  while (w--)
    {
      _mm_pause();
    }
}



/* ******************************************************************************** */
/* inteface */
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
int ht_gc_collect_all(hyht_wrapper_t* h);
int ht_gc_free(hashtable_t* hashtable);
void ht_gc_destroy(hyht_wrapper_t* hashtable);
size_t ht_gc_min_version_used(hyht_wrapper_t* h);

void ht_print(hashtable_t* hashtable);
size_t ht_status(hyht_wrapper_t* hashtable, int resize_increase, int emergency_increase, int just_print);

bucket_t* create_bucket();
int ht_resize_pes(hyht_wrapper_t* hashtable, int is_increase, int by);
void  ht_print_retry_stats();

#endif /* _LFHT_RES_H_ */

