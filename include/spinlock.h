//spinlock lock

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

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
#include "atomic_ops.h"
#include "utils.h"


#define MIN_DELAY 100
#define MAX_DELAY 1000

typedef volatile uint32_t spinlock_index_t;
#ifdef __tile__
typedef uint32_t spinlock_lock_data_t;
#else
typedef uint8_t spinlock_lock_data_t;
#endif

typedef struct spinlock_lock_t 
{
  union 
  {
    spinlock_lock_data_t lock;
#ifdef ADD_PADDING
    uint8_t padding[CACHE_LINE_SIZE];
#else
    uint8_t padding;
#endif
  };
} spinlock_lock_t;


//initializes the lock array; should be called only once per experiment; the number of locks is given as parameter
spinlock_lock_t* init_spinlock_locks_processes(spinlock_index_t size, char *key);

//lock the 
void spinlock_lock(spinlock_lock_t* locks, uint32_t* limits, spinlock_index_t index);

int spinlock_trylock(spinlock_lock_t* locks, uint32_t* limits,  spinlock_index_t index);
//unlock the lock with the given index
void spinlock_unlock(spinlock_lock_t* locks, spinlock_index_t index);


int is_free_spinlock(spinlock_lock_t * locks, spinlock_index_t index);
/*
    Some methods for easy lock array manipluation
*/

spinlock_lock_t* init_spinlock_locks(uint32_t num_locks);


uint32_t* init_thread_spinlock(uint32_t thread_num, uint32_t size);


void end_thread_spinlock(uint32_t* limits);


void end_spinlock(spinlock_lock_t* the_locks);

#endif


