/* December 10, 2013 */
#ifndef _SSMEM_H_
#define _SSMEM_H_

#include "utils.h"
#include "atomic_ops.h"

/* **************************************************************************************** */
/* parameters */
/* **************************************************************************************** */

#define SSMEM_GC_FREE_SET_SIZE 16380
#define SSMEM_DEFAULT_MEM_SIZE (32 * 1024 * 1024L)

/* **************************************************************************************** */
/* data structures used by ssmem */
/* **************************************************************************************** */

/* a ssmem allocator */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_allocator
{
  union
  {
    struct
    {
      void* mem;
      size_t mem_curr;
      size_t mem_size;
      size_t tot_size;
      struct ssmem_mem_chunk* mem_chunks;

      struct ssmem_ts* ts;

      struct ssmem_free_set* free_set_list;
      size_t free_set_num;
      struct ssmem_free_set* collected_set_list;
      size_t collected_set_num;
      struct ssmem_free_set* available_set_list;
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} ssmem_allocator_t;

/* a timestamp used by a thread */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_ts
{
  union
  {
    struct
    {
      size_t version;
      uint8_t id;
      struct ssmem_ts* next;
    };
  };
  uint8_t padding[CACHE_LINE_SIZE];
} ssmem_ts_t;

/* 
 * a timestamped free_set. It holds:  
 *  1. the collection of timestamps at the point the freeing for this set
 *   starts
 *  2. the array of freed pointers to be used by ssmem_free()
 *  3. a set_next pointer in order to be able to create linked lists of
 *   free_sets
 */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_free_set
{
  size_t* ts_set;		/* set of timestamps for GC */
  size_t size;
  long int curr;		
  struct ssmem_free_set* set_next;
  uintptr_t* set;
} ssmem_free_set_t;

/*
 * list that keeps track of actual memory that has been allocated
 * (using malloc / memalign)
 */
typedef struct ssmem_mem_chunk
{
  void* mem;
  struct ssmem_mem_chunk* next;
} ssmem_mem_chunk_t;

/* **************************************************************************************** */
/* ssmem interface */
/* **************************************************************************************** */

/* initialize an allocator */
void ssmem_init(ssmem_allocator_t* a, size_t size, int id);
/* subscribe to the list of threads in order to used timestamps for GC */
void ssmem_gc_init(ssmem_allocator_t* a);
/* terminate the system and free all memory */
void ssmem_term();

/* allocate some memory using allocator a */
inline void* ssmem_alloc(ssmem_allocator_t* a, size_t size);
/* free some memory using allocator a */
inline void ssmem_free(ssmem_allocator_t* a, void* obj);

/* debug functions */
void ssmem_ts_list_print();
size_t* ssmem_ts_set_collect();
void ssmem_ts_set_print(size_t* set);

void ssmem_free_list_print(ssmem_allocator_t* a);
void ssmem_collected_list_print(ssmem_allocator_t* a);
void ssmem_available_list_print(ssmem_allocator_t* a);


#endif /* _SSMEM_H_ */

