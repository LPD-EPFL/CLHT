#ifndef _LFHT_DUP_H_
#define _LFHT_DUP_H_

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

#define KEY_NULL 0
#define KEY_BLCK -1

#define KEY_BUCKT 4
#define ENTRIES_PER_BUCKET KEY_BUCKT

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

typedef struct ALIGNED(CACHE_LINE_SIZE) bucket_s
{
  hyht_addr_t key[KEY_BUCKT];
  hyht_val_t val[KEY_BUCKT];
} bucket_t;

typedef struct ALIGNED(CACHE_LINE_SIZE) lfht_wrapper
{
  union
  {
    struct
    {
      struct hashtable_s* ht;
      uint8_t next_cache_line[CACHE_LINE_SIZE - (sizeof(void*))];
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
    };
    uint8_t padding[2*CACHE_LINE_SIZE];
  };
} hashtable_t;

inline uint64_t __ac_Jenkins_hash_64(uint64_t key);

/* Hash a key for a particular hashtable. */
uint32_t ht_hash(hashtable_t* hashtable, hyht_addr_t key );


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
vals_get_empty_index(hyht_val_t* vals)
{
  int i;
  for (i = 0; i < KEY_BUCKT; i++)
    {
      if (vals[i] == 0)
	{
	  return i;
	}
    }
  return -1;
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
inline int lfht_gc_get_id();
int ht_gc_collect(hyht_wrapper_t* h);
int ht_gc_collect_all(hyht_wrapper_t* h);
int ht_gc_free(hashtable_t* hashtable);
void ht_gc_destroy(hyht_wrapper_t* hashtable);

void ht_print(hashtable_t* hashtable);
size_t ht_status(hyht_wrapper_t* hashtable, int resize_increase, int just_print);

bucket_t* create_bucket();
int ht_resize_pes(hyht_wrapper_t* hashtable, int is_increase, int by);
void  ht_print_retry_stats();

#endif /* _LFHT_DUP_H_ */

