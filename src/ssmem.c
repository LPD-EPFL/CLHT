#include "ssmem.h"

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

void
ssmem_init(ssmem_allocator_t* a, size_t size, int id)
{
  /* printf("[ALLOC] initializing %zu bytes = %zu KB\n", size, size / 1024); */
  a->mem = (void*) memalign(CACHE_LINE_SIZE, size);
  assert(a->mem != NULL);

  a->mem_curr = 0;
  a->mem_size = size;

  a->tot_size = size;

  a->ts = (ssmem_ts_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmem_ts_t));
  assert (a->ts != NULL);
  ssmem_ts_local = a->ts;

  a->ts->id = id;
  a->ts->version = 0;

  do
    {
      a->ts->next = ssmem_ts_list;
    }
  while (CAS_U64((volatile size_t*) &ssmem_ts_list, (size_t) a->ts->next, (size_t) a->ts) != (size_t) a->ts->next);
  
  FAI_U32(&ssmem_ts_list_len);

  a->free_set = NULL;
  a->free_set_num = 0;

  a->collected_set = NULL;
  a->collected_set_num = 0;

  a->available_set = NULL;
}

ssmem_free_set_t*
ssmem_free_set_new(size_t size, ssmem_free_set_t* next)
{
  /* allocate both the ssmem_free_set_t and the free_set with one call */
  ssmem_free_set_t* fs = (ssmem_free_set_t*) memalign(CACHE_LINE_SIZE, sizeof(ssmem_free_set_t) + (size * sizeof(uintptr_t)));
  assert(fs != NULL);

  fs->size = size;
  fs->curr = 0;
  
  fs->free_set = (uintptr_t*) (((uintptr_t) fs) + sizeof(ssmem_free_set_t));
  fs->ts_set = ssmem_ts_set_collect(NULL);

  fs->set_next = next;

  return fs;
}


ssmem_free_set_t*
ssmem_free_set_get_avail(ssmem_allocator_t* a, size_t size, ssmem_free_set_t* next)
{
  ssmem_free_set_t* fs;
  if (a->available_set != NULL)
    {
      fs = a->available_set;
      a->available_set = fs->set_next;

      fs->curr = 0;
      fs->set_next = next;

      fs->ts_set = ssmem_ts_set_collect(fs->ts_set);
      /* printf("[ALLOC] got free_set from available_set : %p\n", fs); */
    }
  else
    {
      fs = ssmem_free_set_new(size, next);
    }

  return fs;
}


static void
ssmem_free_set_free(ssmem_free_set_t* set)
{
  free(set->ts_set);
  free(set);
}

static inline void
ssmem_free_set_make_avail(ssmem_allocator_t* a, ssmem_free_set_t* set)
{
  /* printf("[ALLOC] added to avail_set : %p\n", set); */
  set->curr = 0;
  set->set_next = a->available_set;
  a->available_set = set;
}


/* #define SSMEM_GC_FREE_SET_SIZE 65532 */
#define SSMEM_GC_FREE_SET_SIZE 16380
/* #define SSMEM_GC_FREE_SET_SIZE 2044 */
/* #define SSMEM_GC_FREE_SET_SIZE 4 */



void
ssmem_gc_init(ssmem_allocator_t* a)
{
  a->free_set = ssmem_free_set_new(SSMEM_GC_FREE_SET_SIZE, NULL);
  a->free_set_num++;
}

void
ssmem_term(ssmem_allocator_t* a)
{
  printf("[ALLOC] term() : ~ total mem used: %zu bytes = %zu KB = %zu MB\n",
  	 a->tot_size, a->tot_size / 1024, a->tot_size / (1024 * 1024));

  /* printf("[ALLOC] free(mem)\n"); fflush(stdout); */
  free(a->mem);
  /* printf("[ALLOC] free(ts)\n"); fflush(stdout); */
  free(a->ts);

  /* printf("[ALLOC] free(free_set)\n"); fflush(stdout); */
  /* freeing free sets */
  ssmem_free_set_t* fs = a->free_set;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

  /* printf("[ALLOC] free(collected_set)\n"); fflush(stdout); */
  /* freeing collected sets */
  fs = a->collected_set;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

  /* printf("[ALLOC] free(available_set)\n"); fflush(stdout); */
  /* freeing available sets */
  fs = a->available_set;
  while (fs != NULL)
    {
      ssmem_free_set_t* nxt = fs->set_next;
      ssmem_free_set_free(fs);
      fs = nxt;
    }

 }

static inline void 
ssmem_ts_next()
{
  ssmem_ts_local->version++;
}

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

inline void* 
ssmem_alloc(ssmem_allocator_t* a, size_t size)
{
  ssmem_ts_next();

  void* m = NULL;
  /* 1st try to use from the collected memory */

  ssmem_free_set_t* cs = a->collected_set;
  if (cs != NULL)
    {
      m = (void*) cs->free_set[--cs->curr];

      if (cs->curr <= 0)
	{
	  /* printf("[ALLOC] Collected mem @ %p is empty\n", cs); */
	  a->collected_set = cs->set_next;
	  a->collected_set_num--;

	  ssmem_free_set_make_avail(a, cs);
	}

      return m;
    }

  if (a->mem_curr >= a->mem_size)
    {
      /* printf("[ALLOC] out of mem, need to allocate\n"); */
      a->mem = (void*) memalign(CACHE_LINE_SIZE, a->mem_size);
      assert(a->mem != NULL);
      a->mem_curr = 0;
      
      a->tot_size += a->mem_size;
    }

  void* ret = a->mem + a->mem_curr;
  a->mem_curr += size;
  return ret;
}


/* return > 0 iff snew is > sold for each entry */
__attribute__ ((unused)) static int			
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
static int			
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

static int
ssmem_gc(ssmem_allocator_t* a)
{
  ssmem_free_set_t* fs_cur = a->free_set;
  ssmem_free_set_t* fs_nxt = fs_cur->set_next;

  int gced_num = 0;

  if (fs_nxt == NULL || fs_nxt->set_next == NULL)
    {
      return 0;
    }

  ssmem_free_set_t* fs_nxt_nxt = fs_nxt->set_next;

  int not_gced_num = 2;
  if (ssmem_ts_compare_3(fs_cur->ts_set, fs_nxt->ts_set, fs_nxt_nxt->ts_set))
    {
      gced_num = a->free_set_num - not_gced_num;
      /* take the the suffix of the list (all collected free_sets) away from the
	 free_set list of a and set the correct num of free_sets*/
      fs_nxt->set_next = NULL;
      a->free_set_num = not_gced_num;

      /* find the tail for the collected_set list in order to append the new 
	 free_sets that were just collected */
      ssmem_free_set_t* collected_set_cur = a->collected_set; 
      if (collected_set_cur != NULL)
	{
	  while (collected_set_cur->set_next != NULL)
	    {
	      collected_set_cur = collected_set_cur->set_next;
	    }

	  collected_set_cur->set_next = fs_nxt_nxt;
	}
      else
	{
	  a->collected_set = fs_nxt_nxt;
	}
      a->collected_set_num += gced_num;
    }
  return gced_num;
}

inline void 
ssmem_free(ssmem_allocator_t* a, void* obj)
{
  ssmem_ts_next();
  ssmem_free_set_t* fs = a->free_set;
  if (fs->curr == fs->size)
    {
      /* size_t garbagep = a->free_set_num * SSMEM_GC_FREE_SET_SIZE * sizeof(uintptr_t); */
      /* printf("[ALLOC] free_set is full, doing GC / size of garbage pointers: %10zu = %zu KB\n", garbagep, garbagep / 1024); */
      ssmem_free_set_t* fs_new = ssmem_free_set_get_avail(a, SSMEM_GC_FREE_SET_SIZE, a->free_set);
      a->free_set = fs_new;
      a->free_set_num++;
      fs = fs_new;

      ssmem_gc(a);
    }
  
  fs->free_set[fs->curr++] = (uintptr_t) obj;
}

static void 
ssmem_ts_set_print_no_newline(size_t* set)
{
  printf("[");
  int i;
  for (i = 0; i < ssmem_ts_list_len; i++)
    {
      printf("%zu|", set[i]);
    }
  printf("]");
}


void
ssmem_free_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] free_set list (%zu sets): \n", a->free_set_num);

  int n = 0;
  ssmem_free_set_t* cur = a->free_set;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

void
ssmem_collected_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] collected_set list (%zu sets): \n", a->collected_set_num);

  int n = 0;
  ssmem_free_set_t* cur = a->collected_set;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

void
ssmem_available_list_print(ssmem_allocator_t* a)
{
  printf("[ALLOC] avail_set list: \n");

  int n = 0;
  ssmem_free_set_t* cur = a->available_set;
  while (cur != NULL)
    {
      printf("(%-3d | %p::", n++, cur);
      ssmem_ts_set_print_no_newline(cur->ts_set);
      printf(") -> \n");
      cur = cur->set_next;
    }
  printf("NULL\n");
}

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
