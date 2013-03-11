#include "ticket.h"


static inline uint32_t
sub_abs(const uint32_t a, const uint32_t b)
{
  if (a > b)
    {
      return a - b;
    }
  else
    {
      return b - a;
    }
}

int ticket_trylock(ticketlock_t* lock) {
    uint32_t me = lock->tail;
    uint32_t me_new = me + 1;
    uint64_t cmp = ((uint64_t) me << 32) + me_new; 
    uint64_t cmp_new = ((uint64_t) me_new << 32) + me_new; 
    uint64_t* la = (uint64_t*) lock;
    if (CAS_U64(la,cmp,cmp_new)==cmp) return 0;
    return 1;
}
#define TICKET_BASE_WAIT 512
#define TICKET_MAX_WAIT  4095
#define TICKET_WAIT_NEXT 128

void
ticket_acquire(ticketlock_t* lock) 
{
  uint32_t my_ticket = IAF_U32(&(lock->tail));

#if defined(OPTERON_OPTIMIZE)
  uint32_t wait = TICKET_BASE_WAIT;
  uint32_t distance_prev = 1;
  while (1)
    {
      PREFETCHW(lock);
      uint32_t cur = lock->head;
      if (cur == my_ticket)
	{
	  break;
	}
      uint32_t distance = sub_abs(cur, my_ticket);
      if (distance > 1)
      	{
	  if (distance != distance_prev)
	    {
	      distance_prev = distance;
	      wait = TICKET_BASE_WAIT;
	    }

	  nop_rep(distance * wait);
	  wait = (wait + TICKET_BASE_WAIT) & TICKET_MAX_WAIT;
      	}
      else
	{
	  nop_rep(TICKET_WAIT_NEXT);
	}
    }
#else  /* !OPTERON_OPTIMIZE */
  /* backoff proportional to the distance would make sense even without the PREFETCHW */
  /* however, I did some tests on the Niagara and it performed worse */
  while (lock->head != my_ticket)
    {
      PAUSE;
    }
#endif	/* OPTERON_OPTIMIZE */
}

void ticket_release(ticketlock_t* lock) {
#if defined(OPTERON_OPTIMIZE)
  PREFETCHW(lock);
#endif	/* OPTERON */
  lock->head++;
}

ticketlock_t* create_ticketlock() {
    ticketlock_t* the_lock;
    the_lock = (ticketlock_t*)malloc(sizeof(ticketlock_t));
    the_lock->head=1;
    the_lock->tail=0;
    return the_lock;
}

int is_free_ticket(ticketlock_t* t){
    if ((t->head - t->tail) == 1) return 1;
    return 0;
}

void init_thread_ticketlocks(uint32_t thread_num) {
    set_cpu(thread_num);
}

ticketlock_t* init_ticketlocks(uint32_t num_locks) {
    ticketlock_t* the_locks;
    the_locks = (ticketlock_t*) malloc(num_locks * sizeof(ticketlock_t));
    uint32_t i;
    for (i = 0; i < num_locks; i++) {
        the_locks[i].head=1;
        the_locks[i].tail=0;
    }
    return the_locks;
}

void free_ticketlocks(ticketlock_t* the_locks) {
    free(the_locks);
}

