//test-and-test-and-set lock
#include "ttas.h"

#define UNLOCKED 0
#define LOCKED 1

__thread unsigned long * ttas_seeds;

ttas_lock_t * init_ttas_locks_processes(ttas_index_t size, char *key) {
    //this function is no longer needed
    return NULL;
}


int ttas_trylock(ttas_lock_t * locks, uint32_t * limits,  ttas_index_t index) {
    if (CAS_U8(&locks[index].lock,UNLOCKED,LOCKED)==UNLOCKED) return 0;
    return 1;
}

void ttas_lock(ttas_lock_t * locks, uint32_t * limits,  ttas_index_t index) {
#if defined(OPTERON_OPTIMIZE)
//  uint32_t* limit = limits + index;
//  volatile ttas_lock_data_t* l = &locks[index].lock;
//  while (CAS_U8(l, UNLOCKED, LOCKED) == LOCKED)
//    {
//      uint32_t t = 512;
//      PREFETCHW(l);
//      while(*l == LOCKED)
//	{
//	  uint32_t i;
//	  uint32_t wt = (my_random(&(ttas_seeds[0]),&(ttas_seeds[1]),&(ttas_seeds[2])) % t) + 1;
//	  for (i = 0; i < wt; i++)
//	    {
//	      PAUSE;
//	    }
//	  PREFETCHW(l);
//	  t *= 4;
//	  if (t > 102400)
//	    {
//	      t = 102400;
//	    }
//	}
//    }
  uint32_t* limit = limits + index;
  volatile ttas_lock_data_t* l = &locks[index].lock;
  uint32_t delay;
  while (1){
    PREFETCHW(l);
    while (locks[index].lock==1) {
        PREFETCHW(l);
    }
    if (CAS_U8(&locks[index].lock,UNLOCKED,LOCKED)==UNLOCKED) {
      return;
    } else {
      //backoff
      limit = &limits[index];
      delay = my_random(&(ttas_seeds[0]),&(ttas_seeds[1]),&(ttas_seeds[2]))%(*limit);
      *limit = MAX_DELAY > 2*(*limit) ? 2*(*limit) : MAX_DELAY;
      cdelay(delay);
    }
  }

#else  /* !OPTERON_OPTIMIZE */
  uint32_t* limit = limits + index;
  uint32_t delay;
  while (1){
    while (locks[index].lock==1) {}
    if (CAS_U8(&locks[index].lock,UNLOCKED,LOCKED)==UNLOCKED) {
      return;
    } else {
      //backoff
      limit = &limits[index];
      delay = my_random(&(ttas_seeds[0]),&(ttas_seeds[1]),&(ttas_seeds[2]))%(*limit);
      *limit = MAX_DELAY > 2*(*limit) ? 2*(*limit) : MAX_DELAY;
      cdelay(delay);
    }
  }
#endif	/* OPTERON_OPTIMIZE */
}


int is_free_ttas(ttas_lock_t * locks, ttas_index_t index){
    if (locks[index].lock==UNLOCKED) return 1;
    return 0;
}

void ttas_unlock(ttas_lock_t *locks, ttas_index_t index) 
{
  locks[index].lock=0;
}


/*
    Some methods for easy lock array manipulation
*/


//ttas
ttas_lock_t* init_ttas_locks(uint32_t num_locks) {

   ttas_lock_t* the_locks;
   the_locks = (ttas_lock_t*)malloc(num_locks * sizeof(ttas_lock_t));
   uint32_t i;
   for (i = 0; i < num_locks; i++) {
       the_locks[i].lock=0;
   }
   return the_locks;
}

uint32_t* init_thread_ttas(uint32_t thread_num, uint32_t size){
    //assign the thread to the correct core
    set_cpu(thread_num);
    ttas_seeds = seed_rand();

    uint32_t* limits;
    limits = (uint32_t*)malloc(size * sizeof(uint32_t));
    uint32_t i;
    for (i = 0; i < size; i++) {
       limits[i]=1; 
    }
    return limits;
}

void end_thread_ttas(uint32_t* limits) {
    free(limits);
}

void end_ttas(ttas_lock_t* the_locks) {
    free(the_locks);
}

