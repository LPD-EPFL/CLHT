//spinlock lock
#include "spinlock.h"

#define UNLOCKED 0
#define LOCKED 1

__thread unsigned long* spinlock_seeds;

spinlock_lock_t* 
init_spinlock_locks_processes(spinlock_index_t size, char *key) 
{
  //this function is no longer needed
  return NULL;
}
int spinlock_trylock(spinlock_lock_t* locks, uint32_t* limits,  spinlock_index_t index) {
    if (CAS_U8(&locks[index].lock,UNLOCKED,LOCKED)==UNLOCKED) return 0;
    return 1;
}
void
spinlock_lock(spinlock_lock_t* locks, uint32_t* limits,  spinlock_index_t index) 
{
#if defined(OPTERON_OPTIMIZE)
  uint32_t* limit = limits + index;
  volatile spinlock_lock_data_t* l = &locks[index].lock;
  while (CAS_U8(l, UNLOCKED, LOCKED) == LOCKED)
    {
      PREFETCHW(l);
      while(*l == LOCKED)
	{
	  limit = &limits[index];
	  uint32_t delay = my_random(&(spinlock_seeds[0]),&(spinlock_seeds[1]),&(spinlock_seeds[2]))%(*limit);
	  *limit = MAX_DELAY > 2*(*limit) ? 2*(*limit) : MAX_DELAY;
	  cdelay(delay);
	  PREFETCHW(l);
	}
    }
#else  /* !OPTERON_OPTIMIZE */
  volatile spinlock_lock_data_t* l = &locks[index].lock;
  while (CAS_U8(l, UNLOCKED, LOCKED) == LOCKED) 
    {
      PAUSE;
    } 
#endif	/* OPTERON_OPTIMIZE */
}

void
spinlock_unlock(spinlock_lock_t *locks, spinlock_index_t index) 
{
  locks[index].lock = UNLOCKED;
}

int is_free_spinlock(spinlock_lock_t * locks, spinlock_index_t index){
    if (locks[index].lock==UNLOCKED) return 1;
    return 0;
}

/*
    Some methods for easy lock array manipulation
*/


//spinlock
spinlock_lock_t*
init_spinlock_locks(uint32_t num_locks) 
{
  spinlock_lock_t* the_locks;
  the_locks = (spinlock_lock_t*)malloc(num_locks * sizeof(spinlock_lock_t));
  uint32_t i;
  for (i = 0; i < num_locks; i++) 
    {
      the_locks[i].lock = UNLOCKED;
    }
  return the_locks;
}

uint32_t*
init_thread_spinlock(uint32_t thread_num, uint32_t size)
{
  //assign the thread to the correct core
  set_cpu(thread_num);
  spinlock_seeds = seed_rand();

  uint32_t* limits;
  limits = (uint32_t*)malloc(size * sizeof(uint32_t));
  uint32_t i;
  for (i = 0; i < size; i++) 
    {
      limits[i] = 1; 
    }
  return limits;
}

void end_thread_spinlock(uint32_t* limits) 
{
  free(limits);
}

void end_spinlock(spinlock_lock_t* the_locks) 
{
  free(the_locks);
}

