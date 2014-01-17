/* 
 * File: ssmem.c
 * Description: a simple object-based memory allocator with epoch-based garbage collection
 * Author: Vasileios Trigonakis 
 *
 */
#include "ssmem.h"
#include <malloc.h>

ssmem_ts_t* ssmem_ts_list = NULL;
volatile uint32_t ssmem_ts_list_len = 0;
__thread volatile ssmem_ts_t* ssmem_ts_local = NULL;

inline int 
ssmem_get_id()
{
  if (ssmem_ts_local != NULL)
    {
      return ssmem_ts_local->id;
    }
  return -1;
}

static ssmem_mem_chunk_t* ssmem_mem_chunk_new(void* mem, ssmem_mem_chunk_t* next);

/* 
 * 
 */
void
ssmem_init(ssmem_allocator_t* a, size_t size, int id)
{
  /* printf("[ALLOC] initializing %zu bytes = %zu KB\n", size, size / 1024); */
  a->mem = (void*) memalign(CACHE_LINE_SIZE, size);
  assert(a->mem != NULL);

  a->mem_curr = 0;
  a->mem_size = size;
  a->tot_size = size;

  a->mem_chunks = ssmem_mem_chunk_new(a->mem, NULL);

  a->ts = (ssmem_ts_t*) ssmem_ts_local;
  if (a->ts == NULL)
    {
      a->ts = (ssmem_ts_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmem_ts_t));
      assert (a->ts != NULL);
      ssmem_ts_local = a->ts;
    }

  a->ts->id = id;
  a->ts->version = 0;

  do
    {
      a->ts->next = ssmem_ts_list;
    }
  while (CAS_U64((volatile uint64_t*) &ssmem_ts_list, (uint64_t) a->ts->next, (uint64_t) a->ts) != (uint64_t) a->ts->next);
  
  FAI_U32(&ssmem_ts_list_len);

  a->free_set_list = NULL;
  a->free_set_num = 0;

  a->collected_set_list= NULL;
  a->collected_set_num = 0;

  a->available_set_list = NULL;
}

/* 
 * 
 */
static ssmem_mem_chunk_t*
ssmem_mem_chunk_new(void* mem, ssmem_mem_chunk_t* next)
{
  ssmem_mem_chunk_t* mc;
  mc = (ssmem_mem_chunk_t*) malloc(sizeof(ssmem_mem_chunk_t));
  assert(mc != NULL);
  mc->mem = mem;
  mc->next = next;

  return mc;
}

/* 
 * 
 */
ssmem_free_set_t*
ssmem_free_set_new(size_t size, ssmem_free_set_t* next)
{
  /* allocate both the ssmem_free_set_t and the free_set with one call */
  ssmem_free_set_t* fs = (ssmem_free_set_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmem_free_set_t) + (size * sizeof(uintptr_t)));
  assert(fs != NULL);

  fs->size = size;
  fs->curr = 0;
  
  fs->set = (uintptr_t*) (((uintptr_t) fs) + sizeof(ssmem_free_set_t));
  fs->ts_set = NULL;	      /* will get a ts when it becomes full */
  fs->set_next = next;

  return fs;
}


/* 
 * 
 */
ssmem_free_set_t*
ssmem_free_set_get_avail(ssmem_allocator_t* a, size_t size, ssmem_free_set_t* next)
{
  ssmem_free_set_t* fs;
  if (a->available_set_list != NULL)
    {
      fs = a->available_set_list;
      a->available_set_list = fs->set_next;

      fs->curr = 0;
      fs->set_next = next;

      /* printf("[ALLOC] got free_set from available_set : %p\n", fs); */
    }
  else
    {
      fs = ssmem_free_set_new(size, next);
    }

  return fs;
}


/* 
 * 
 */
static void
ssmem_free_set_free(ssmem_free_set_t* set)
{
  free(set->ts_set);
  free(set);
}

/* 
 * 
 */
static inline void
ssmem_free_set_make_avail(ssmem_allocator_t* a, ssmem_free_set_t* set)
{
  /* printf("[ALLOC] added to avail_set : %p\n", set); */
  set->curr = 0;
  set->set_next = a->available_set_list;
  a->available_set_list = set;
}


/* 
 * 
 */
void
ssmem_gc_init(ssmem_allocator_t* a)
{
  a->free_set_list = ssmem_free_set_new(SSMEM_GC_FREE_SET_SIZE, NULL);
  a->free_set_num++;
}

/* 
 * 
 */
void
ssmem_term(ssmem_allocator_t* a)
{
  printf("[ALLOC] term() : ~ total mem used: %zu bytes = %zu KB = %zu MB\n",
  	 a->tot_size, a->tot_size / 1024, a->tot_size / (1024 * 1024));
  ssmem_mem_chunk_t* mcur = a->mem_chunks;
  do
    {
      ssmem_mem_chunk_t* mnxt = mcur->next;
      free(mcur->mem);
      free(mcur);
      mcur = mnxt;
    }
  while (mcur != NULL);

  free(a->ts);

  /* printf("[ALLOC] free(free_set)\n"); fflush(stdout); */
  /* freeing free sets */
  ssmem_free_set_t* fs = a->free_set_list;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

  /* printf("[ALLOC] free(collected_set)\n"); fflush(stdout); */
  /* freeing collected sets */
  fs = a->collected_set_list;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

  /* printf("[ALLOC] free(available_set)\n"); fflush(stdout); */
  /* freeing available sets */
  fs = a->available_set_list;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

 }

/* 
 * 
 */
static inline void 
ssmem_ts_next()
{
  ssmem_ts_local->version++;
}

/* 
 * 
 */
size_t*
ssmem_ts_set_collect(size_t* ts_set)
{
  if (ts_set == NULL)
    {
      ts_set = (size_t*) malloc(ssmem_ts_list_len * sizeof(size_t));
      assert(ts_set != NULL);
    }

  ssmem_ts_t* cur = ssmem_ts_list;
  while (cur != NULL)
    {
      ts_set[cur->id] = cur->version;
      cur = cur->next;
    }

  return ts_set;
}

/* 
 * 
 */
void 
ssmem_ts_set_print(size_t* set)
{
  printf("[ALLOC] set: [");
  int i;
  for (i = 0; i < ssmem_ts_list_len; i++)
    {
      printf("%zu | ", set[i]);
    }
  printf("]\n");
}

/* 
 * 
 */
void* 
ssmem_alloc(ssmem_allocator_t* a, size_t size)
{
  void* m = NULL;

  /* 1st try to use from the collected memory */
  ssmem_free_set_t* cs = a->collected_set_list;
  if (cs != NULL)
    {
      m = (void*) cs->set[--cs->curr];

      if (cs->curr <= 0)
	{
	  /* printf("[ALLOC] Collected mem @ %p is empty\n", cs); */
	  a->collected_set_list = cs->set_next;
	  a->collected_set_num--;

	  ssmem_free_set_make_avail(a, cs);
	}
    }
  else
    {
      if ((a->mem_curr + size) >= a->mem_size)
	{
	  /* printf("[ALLOC] out of mem, need to allocate\n"); */
	  a->mem = (void*) memalign(CACHE_LINE_SIZE, a->mem_size);
	  assert(a->mem != NULL);
	  a->mem_curr = 0;
      
	  a->tot_size += a->mem_size;

	  a->mem_chunks = ssmem_mem_chunk_new(a->mem, a->mem_chunks);
	}

      m = a->mem + a->mem_curr;
      a->mem_curr += size;
    }

  ssmem_ts_next();
  return m;
}


/* return > 0 iff snew is > sold for each entry */
static int			
ssmem_ts_compare(size_t* s_new, size_t* s_old)
{
  int is_newer = 1;
  int i;
  for (i = 0; i < ssmem_ts_list_len; i++)
    {
      if (s_new[i] <= s_old[i])
	{
	  is_newer = 0;
	  break;
	}
    }
  return is_newer;
}

/* return > 0 iff s_1 is > s_2 > s_3 for each entry */
static int __attribute__((unused))
ssmem_ts_compare_3(size_t* s_1, size_t* s_2, size_t* s_3)
{
  int is_newer = 1;
  int i;
  for (i = 0; i < ssmem_ts_list_len; i++)
    {
      if (s_1[i] <= s_2[i] || s_2[i] <= s_3[i])
	{
	  is_newer = 0;
	  break;
	}
    }
  return is_newer;
}

static void ssmem_ts_set_print_no_newline(size_t* set);

/* 
 *
 */
static int
ssmem_mem_reclaim(ssmem_allocator_t* a)
{
  ssmem_free_set_t* fs_cur = a->free_set_list;
  ssmem_free_set_t* fs_nxt = fs_cur->set_next;

  int gced_num = 0;

  if (fs_nxt == NULL)		/* need at least 2 sets to compare */
    {
      return 0;
    }

  if (ssmem_ts_compare(fs_cur->ts_set, fs_nxt->ts_set))
    {
      gced_num = a->free_set_num - 1;
      /* take the the suffix of the list (all collected free_sets) away from the
	 free_set list of a and set the correct num of free_sets*/
      fs_cur->set_next = NULL;
      a->free_set_num = 1;

      /* find the tail for the collected_set list in order to append the new 
	 free_sets that were just collected */
      ssmem_free_set_t* collected_set_cur = a->collected_set_list; 
      if (collected_set_cur != NULL)
	{
	  while (collected_set_cur->set_next != NULL)
	    {
	      collected_set_cur = collected_set_cur->set_next;
	    }

	  collected_set_cur->set_next = fs_nxt;
	}
      else
	{
	  a->collected_set_list = fs_nxt;
	}
      a->collected_set_num += gced_num;
    }

  /* if (gced_num) */
  /*   { */
  /*     printf("//collected %d sets\n", gced_num); */
  /*   } */
  return gced_num;
}

/* 
 *
 */
inline void 
ssmem_free(ssmem_allocator_t* a, void* obj)
{
  ssmem_free_set_t* fs = a->free_set_list;
  if (fs->curr == fs->size)
    {
      fs->ts_set = ssmem_ts_set_collect(fs->ts_set);
      ssmem_mem_reclaim(a);

      /* printf("[ALLOC] free_set is full, doing GC / size of garbage pointers: %10zu = %zu KB\n", garbagep, garbagep / 1024); */
      ssmem_free_set_t* fs_new = ssmem_free_set_get_avail(a, SSMEM_GC_FREE_SET_SIZE, a->free_set_list);
      a->free_set_list = fs_new;
      a->free_set_num++;
      fs = fs_new;

    }
  
  fs->set[fs->curr++] = (uintptr_t) obj;
  ssmem_ts_next();
}

/* 
 *
 */
static void 
ssmem_ts_set_print_no_newline(size_t* set)
{
  printf("[");
  if (set != NULL)
    {
      int i;
      for (i = 0; i < ssmem_ts_list_len; i++)
	{
	  printf("%zu|", set[i]);
	}
    }
  else
    {
      printf(" no timestamp yet ");
    }
  printf("]");

}

/* 
 *
 */
void
ssmem_free_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] free_set list (%zu sets): \n", a->free_set_num);

  int n = 0;
  ssmem_free_set_t* cur = a->free_set_list;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

/* 
 *
 */
void
ssmem_collected_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] collected_set list (%zu sets): \n", a->collected_set_num);

  int n = 0;
  ssmem_free_set_t* cur = a->collected_set_list;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

/* 
 *
 */
void
ssmem_available_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] avail_set list: \n");

  int n = 0;
  ssmem_free_set_t* cur = a->available_set_list;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

/* 
 *
 */
void
ssmem_all_list_print(ssmem_allocator_t* a, int id)
{
  printf("[ALLOC] [%-2d] free_set list: %-4zu / collected_set list: %-4zu\n",
	 id, a->free_set_num, a->collected_set_num);
}

/* 
 *
 */
void
ssmem_ts_list_print()
{
  printf("[ALLOC] ts list (%u elems): ", ssmem_ts_list_len);
  ssmem_ts_t* cur = ssmem_ts_list;
  while (cur != NULL)
    {
      printf("(id: %-2d / version: %zu) -> ", cur->id, cur->version);
      cur = cur->next;
    }

  printf("NULL\n"); 
}
