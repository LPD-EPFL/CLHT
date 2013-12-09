#ifndef _SSMEM_H_
#define _SSMEM_H_

#include "utils.h"
#include "atomic_ops.h"

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

      struct ssmem_ts* ts;

      struct ssmem_free_set* free_set;
      size_t free_set_num;
      struct ssmem_free_set* collected_set;
      size_t collected_set_num;
    };
    uint8_t padding[CACHE_LINE_SIZE];
  };
  /* remember to padd */
} ssmem_allocator_t;

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

typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_free_set
{
  size_t* ts_set;		/* set of timestamps for GC */
  size_t size;
  long int curr;		
  struct ssmem_free_set* set_next;
  uintptr_t* free_set;
} ssmem_free_set_t;

void ssmem_init(ssmem_allocator_t* a, size_t size, int id);
void ssmem_gc_init(ssmem_allocator_t* a);

void ssmem_term();

inline void* ssmem_alloc(ssmem_allocator_t* a, size_t size);
inline void ssmem_free(ssmem_allocator_t* a, void* obj);

void ssmem_ts_list_print();
size_t* ssmem_ts_set_collect();
void ssmem_ts_set_print(size_t* set);

void ssmem_free_list_print(ssmem_allocator_t* a);
void ssmem_collected_list_print(ssmem_allocator_t* a);


#endif /* _SSMEM_H_ */

