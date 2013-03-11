//test-and-test-and-set lock with backoff

#ifndef _TTAS_H_
#define _TTAS_H_

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

typedef volatile uint32_t ttas_index_t;
#ifdef __tile__
typedef uint32_t ttas_lock_data_t;
#else
typedef uint8_t ttas_lock_data_t;
#endif

typedef struct ttas_lock_t {
    union {
        ttas_lock_data_t lock;
#ifdef ADD_PADDING
        uint8_t padding[CACHE_LINE_SIZE];
#else
        uint8_t padding;
#endif
    };
}ttas_lock_t;


static inline uint32_t backoff(uint32_t limit) {
    uint32_t delay = rand()%limit;
    limit = MAX_DELAY > 2*limit ? 2*limit : MAX_DELAY;
    cdelay(delay);
    return limit;

}

//initializes the lock array; should be called only once per experiment; the number of locks is given as parameter
ttas_lock_t* init_ttas_locks_processes(ttas_index_t size, char *key);

//lock the 
void ttas_lock(ttas_lock_t* locks, uint32_t* limits, ttas_index_t index);
int ttas_trylock(ttas_lock_t* locks, uint32_t* limits, ttas_index_t index);

//unlock the lock with the given index
void ttas_unlock(ttas_lock_t* locks, ttas_index_t index);


int is_free_ttas(ttas_lock_t * locks, ttas_index_t index);
/*
    Some methods for easy lock array manipluation
*/

ttas_lock_t* init_ttas_locks(uint32_t num_locks);


uint32_t* init_thread_ttas(uint32_t thread_num, uint32_t size);


void end_thread_ttas(uint32_t* limits);


void end_ttas(ttas_lock_t* the_locks);

#endif


