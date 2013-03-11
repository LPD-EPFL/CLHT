//ticket lock

#ifndef _TICKET_H_
#define _TICKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#ifndef __sparc__
#include <numa.h>
#endif
#include <pthread.h>
#include "utils.h"
#include "atomic_ops.h"


#define MIN_DELAY 100
#define MAX_DELAY 1000

typedef struct ticketlock_t 
{
  volatile uint32_t head;
  volatile uint32_t tail;
#ifdef ADD_PADDING
  uint8_t padding[CACHE_LINE_SIZE - 8];
#endif
} ticketlock_t;



int ticket_trylock(ticketlock_t* lock);
void ticket_acquire(ticketlock_t* lock);
void ticket_release(ticketlock_t* lock);
ticketlock_t* create_ticketlock();
ticketlock_t* init_ticketlocks(uint32_t num_locks);
void init_thread_ticketlocks(uint32_t thread_num);
void free_ticketlocks(ticketlock_t* the_locks);
int is_free_ticket(ticketlock_t* t);
#endif


