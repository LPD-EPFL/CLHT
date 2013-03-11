//test-and-test-and-set read-write lock
#ifndef _RWTTAS_H_
#define _RWTTAS_H_

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

#define MAX_DELAY 1000

#ifdef __tile__
#define MAX_RW UINT32_MAX
#define W_MASK 0x100000000
typedef uint32_t rw_data_t;
typedef uint64_t all_data_t;
#else
#define MAX_RW UINT8_MAX
#define W_MASK 0x100
typedef uint8_t rw_data_t;
typedef uint16_t all_data_t;
#endif

typedef struct rw_ttas_data {
    volatile rw_data_t read_lock;
    volatile rw_data_t write_lock;
} rw_ttas_data;


typedef struct rw_ttas {
    union {
        rw_ttas_data rw;
        volatile all_data_t lock_data;
#ifdef ADD_PADDING
        uint8_t padding[CACHE_LINE_SIZE];
#endif
    };
} rw_ttas;

void read_acquire(rw_ttas* lock, uint32_t * limit);

void read_release(rw_ttas * lock);

void write_acquire(rw_ttas* lock, uint32_t * limit);

int rw_trylock(rw_ttas* lock, uint32_t* limit);
void write_release(rw_ttas * lock);

int is_free_rw(rw_ttas* lock);

rw_ttas* init_rw_ttas_locks(uint32_t num_locks);

uint32_t* init_thread_rw_ttas(uint32_t thread_num, uint32_t size);

void end_thread_rw_ttas(uint32_t* limits);

void end_rw_ttas(rw_ttas* the_locks);

#endif
