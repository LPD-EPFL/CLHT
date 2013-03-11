#include "htlock.h"

__thread uint32_t my_node, my_id;

htlock_t* 
create_htlock()
{
  htlock_t* htl;
#ifdef __sparc__
  htl = memalign(CACHE_LINE_SIZE, sizeof(htlock_t));
  if (htl==NULL) 
    {
      fprintf(stderr,"Error @ memalign : create htlock\n");
    }
#else
  if (posix_memalign((void**) &htl, CACHE_LINE_SIZE, sizeof(htlock_t)) < 0)
    {
      fprintf(stderr, "Error @ posix_memalign : create_htlock\n");
    }
#endif    
  assert(htl != NULL);

  htl->global.cur = 0;
  htl->global.nxt = 0;
  uint32_t n;
  for (n = 0; n < NUMBER_OF_SOCKETS; n++)
    {
      htl->local[n].cur = NB_TICKETS_LOCAL;
      htl->local[n].nxt = 0;
    }

  return htl;
}


void
init_htlock(htlock_t* htl)
{
  assert(htl != NULL);
  htl->global.cur = 0;
  htl->global.nxt = 0;
  uint32_t n;
  for (n = 0; n < NUMBER_OF_SOCKETS; n++)
    {
      htl->local[n].cur = NB_TICKETS_LOCAL;
      htl->local[n].nxt = 0;
    }
}

void
init_thread_htlocks(uint32_t phys_core)
{
  set_cpu(phys_core);

#ifdef XEON
    __sync_synchronize();
    uint32_t real_core_num;
    uint32_t i;
    for (i = 0; i < (NUMBER_OF_SOCKETS * CORES_PER_SOCKET); i++) {
	if (the_cores[i]==phys_core) {
		real_core_num = i;
		break;
	}
    }
    __sync_synchronize();
    MEM_BARRIER;
    my_id = real_core_num;
   my_node = get_cluster(phys_core);
#else
  my_id = phys_core;
  my_node = get_cluster(phys_core);
#endif
}

int is_free_hticket(htlock_t* htl){
    if (htl->global.cur == htl->global.nxt) return 1;
    return 0;
}

htlock_t*
init_htlocks(uint32_t num_locks)
{
  htlock_t* htls;
#ifdef __sparc__
    htls = memalign(CACHE_LINE_SIZE, num_locks * sizeof(htlock_t));
    if (htls==NULL) {
      fprintf(stderr, "Error @ memalign : init_htlocks\n");
    }
#else
  if (posix_memalign((void**) &htls, 64, num_locks * sizeof(htlock_t)) < 0)
    {
      fprintf(stderr, "Error @ posix_memalign : init_htlocks\n");
    }
#endif   
  assert(htls != NULL);

  uint32_t i;
  for (i = 0; i < num_locks; i++)
    {
      htls[i].global.cur = 0;
      htls[i].global.nxt = 0;
      uint32_t n;
      for (n = 0; n < NUMBER_OF_SOCKETS; n++) /* for each node */
	{
	  htls[i].local[n].cur = NB_TICKETS_LOCAL;
	  htls[i].local[n].nxt = 0;
	}
    }

  return htls;
}


void 
free_htlocks(htlock_t* locks)
{
  free(locks);
}

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


#define TICKET_BASE_WAIT 512
#define TICKET_MAX_WAIT  4095
#define TICKET_WAIT_NEXT 64


static inline void
htlock_wait_ticket(htlock_local_t* lock, const uint32_t ticket)
{

#if defined(OPTERON)
  uint32_t wait = TICKET_BASE_WAIT;
  uint32_t distance_prev = 1;
#endif	/* OPTERON */

  while (1)
    {
      PREFETCHW(lock);
      int32_t lock_cur = lock->cur;
      if (lock_cur == ticket)
	{
	  break;
	}
      uint32_t distance = sub_abs(lock->cur, ticket);
      if (distance > 1)
	{
#if defined(OPTERON)
	  if (distance != distance_prev)
	    {
	      distance_prev = distance;
	      wait = TICKET_BASE_WAIT;
	    }

	  nop_rep(distance * wait);
	  wait = (wait + TICKET_BASE_WAIT) & TICKET_MAX_WAIT;
#else
	  /* pause_rep(distance); */
	  nop_rep(distance);
#endif  /* OPTERON */
	}
      else
	{
#if defined(OPTERON)
	  nop_rep(TICKET_WAIT_NEXT);
#else
	  PAUSE;
#endif  /* OPTERON */
	}
    }  

}

void
htlock_lock(htlock_t* l)
{
  htlock_local_t* localp = &l->local[my_node];
  int32_t local_ticket;

 again_local:
  local_ticket = DAF_U32(&localp->nxt);
  /* only the guy which gets local_ticket == -1 is allowed to share tickets */
  if (local_ticket < -1)	
    {
#if defined(OPTERON)
      nop_rep(64 * (my_id + 1));
#endif  /* OPTERON */
      goto again_local;
    }

  if (local_ticket >= 0)	/* local grabing successful */
    {
      htlock_wait_ticket((htlock_local_t*) localp, local_ticket);
    }
  else				/* no local ticket available */
    {
      do
	{
	  PREFETCHW(localp);
	} while (localp->cur != NB_TICKETS_LOCAL);
      localp->nxt = NB_TICKETS_LOCAL; /* give tickets to the local neighbors */

      htlock_global_t* globalp = &l->global;
      uint32_t global_ticket = FAI_U32(&globalp->nxt);

      htlock_wait_ticket((htlock_local_t*) globalp, global_ticket);
    }
}

uint32_t 
htlock_trylock(htlock_t* l)
{
  htlock_global_t* globalp = &l->global;
  PREFETCHW(globalp);  
  uint32_t global_nxt = globalp->nxt;

  htlock_global_t tmp = 
    {
      .nxt = global_nxt, 
      .cur = global_nxt
    };
  htlock_global_t tmp_new = 
    {
      .nxt = global_nxt + 1, 
      .cur = global_nxt
    };

  uint64_t tmp64 = *(uint64_t*) &tmp;
  uint64_t tmp_new64 = *(uint64_t*) &tmp_new;

  if (CAS_U64((uint64_t*) globalp, tmp64, tmp_new64) == tmp64)
    {
      return 1;
    }

  return 0;
}

inline void
htlock_release(htlock_t* l)
{
  htlock_local_t* localp = &l->local[my_node];
  PREFETCHW(localp);
  int32_t local_cur = localp->cur;
  int32_t local_nxt = CAS_U32(&localp->nxt, local_cur, 0);
  if (local_cur == 0 || local_cur == local_nxt) /* global */
    {
      PREFETCHW((&l->global));
      PREFETCHW(localp);
      localp->cur = NB_TICKETS_LOCAL;
      l->global.cur++;
    }
  else				/* local */
    {
      PREFETCHW(localp);
      localp->cur--;
    }
}

inline void
htlock_release_try(htlock_t* l)	/* trylock rls */
{
  PREFETCHW((&l->global));
  l->global.cur++;
}

