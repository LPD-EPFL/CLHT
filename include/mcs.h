//mcs lock
#ifndef _MCS_H_
#define _MCS_H_

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

typedef struct mcs_qnode {
    volatile uint8_t waiting;
    volatile struct mcs_qnode *volatile next;
#ifdef ADD_PADDING
#if CACHE_LINE_SIZE == 16
#else
    uint8_t padding[CACHE_LINE_SIZE - 16];
#endif
#endif
} mcs_qnode;

typedef volatile mcs_qnode *mcs_qnode_ptr;
typedef mcs_qnode_ptr mcs_lock; //initialized to NULL

//initializes the lock
//mcs_lock *init_mcs(uint32_t num_buckets, int do_global_init, char *key);

//lock
void mcs_acquire(mcs_lock *the_lock, mcs_qnode_ptr I);

//unlock
void mcs_release(mcs_lock *the_lock, mcs_qnode_ptr I);

int is_free_mcs(mcs_lock *L );
/*
    Methods for easy lock array manipulation
*/

mcs_lock** init_mcs_locks(uint32_t num_locks);

mcs_qnode** init_mcs_thread(uint32_t thread_num, uint32_t num_locks);

int mcs_trylock(mcs_lock *L, mcs_qnode_ptr I);
void end_thread_mcs(mcs_qnode** the_qnodes, uint32_t size);

void end_mcs(mcs_lock** the_locks, uint32_t size);

#endif
