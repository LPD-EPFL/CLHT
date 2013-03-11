#ifdef USE_MCS_LOCKS
#include "mcs.h"
#elif defined(USE_HCLH_LOCKS)
#include "hclh.h"
#elif defined(USE_TTAS_LOCKS)
#include "ttas.h"
#elif defined(USE_SPINLOCK_LOCKS)
#include "spinlock.h"
#elif defined(USE_ARRAY_LOCKS)
#include "alock.h"
#elif defined(USE_RW_LOCKS)
#include "rw_ttas.h"
#elif defined(USE_TICKET_LOCKS)
#include "ticket.h"
#elif defined(USE_MUTEX_LOCKS)
#include <pthread.h>
#elif defined(USE_HTICKET_LOCKS)
#include "htlock.h"
#else
#error "No type of locks given"
#endif

//lock globals
#ifdef USE_MCS_LOCKS
typedef mcs_lock** global_data;
#elif defined(USE_HCLH_LOCKS)
typedef hclh_global_params** global_data;
#elif defined(USE_TTAS_LOCKS)
typedef ttas_lock_t* global_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef spinlock_lock_t* global_data;
#elif defined(USE_ARRAY_LOCKS)
typedef lock_shared_t** global_data;
#elif defined(USE_RW_LOCKS)
typedef rw_ttas* global_data;
#elif defined(USE_TICKET_LOCKS)
typedef ticketlock_t* global_data;
#elif defined(USE_MUTEX_LOCKS)
typedef pthread_mutex_t* global_data;
#elif defined(USE_HTICKET_LOCKS)
typedef htlock_t* global_data;
#endif


//typedefs for thread local data
#ifdef USE_MCS_LOCKS
typedef mcs_qnode** local_data;
#elif defined(USE_HCLH_LOCKS)
typedef hclh_local_params** local_data;
#elif defined(USE_TTAS_LOCKS)
typedef unsigned int* local_data;
#elif defined(USE_SPINLOCK_LOCKS)
typedef unsigned int* local_data;
#elif defined(USE_ARRAY_LOCKS)
typedef array_lock_t** local_data;
#elif defined(USE_RW_LOCKS)
typedef unsigned int * local_data;
#elif defined(USE_TICKET_LOCKS)
typedef void* local_data;//no local data for ticket locks
#elif defined(USE_MUTEX_LOCKS)
typedef void* local_data;//no local data for mutexes
#elif defined(USE_HTICKET_LOCKS)
typedef void* local_data;//no local data for hticket locks
#endif

/*
 *  Declarations
 */

//lock acquire operation; in case of read write lock uses the writer lock
static inline void acquire_lock(unsigned int index, local_data local_d, global_data the_locks);

//acquisition of read lock; in case of non-rw lock, just implements exlusive access
static inline void acquire_read(unsigned int index, local_data local_d, global_data the_locks);


//acquisition of write lock; in case of non-rw lock, just implements exlusive access
static inline void acquire_write(unsigned int index, local_data local_d, global_data the_locks);

//lock release operation
//cluster_id is the cluster number of the core requesting the operation;
//e.g. the socket in the case of the Opteron
static inline void release_lock(int index, int cluster_id, local_data local_d, global_data the_locks);

static inline void release_trylock(int index, int cluster_id, local_data local_d, global_data the_locks);

//release reader lock
static inline void release_read(int index, int cluster_id, local_data local_d, global_data the_locks);

//release writer lock
static inline void release_write(int index, int cluster_id, local_data local_d, global_data the_locks);

//initialization of local data for an array of locks; core_to_pin is the core on which the thread is execting, 
static inline local_data init_local(int core_to_pin, int num_locks, global_data the_locks);

//initialization of global data for an array of locks
static inline global_data init_global(int num_locks, int num_threads);

//removal of global data for a lock array
static inline void free_global(global_data the_locks, int num_locks);

//removal of local data for a lock array 
static inline void free_local(local_data local_d, int num_locks);

//non-atomic trylock: if (is_free(lock)) acquire; needed for memcached
static inline int na_trylock(unsigned int index, local_data local_d, global_data the_locks);
/*
 *  Functions
 */

static inline void acquire_lock(unsigned int index, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_pred= (qnode*) hclh_acquire(local_d[index]->my_queue,the_locks[index]->shared_queue,local_d[index]->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(the_locks, local_d, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(the_locks, local_d, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    write_acquire(&the_locks[index],&(local_d[index]));
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(&the_locks[index]);
#endif
}

static inline void release_lock(int index, int cluster_id, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_release(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_qnode=hclh_release(local_d[index]->my_qnode,local_d[index]->my_pred,cluster_id);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(the_locks, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(the_locks, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    write_release(&the_locks[index]); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(&the_locks[index]);
#endif
}

static inline void acquire_write(unsigned int index, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_pred= (qnode*) hclh_acquire(local_d[index]->my_queue,the_locks[index]->shared_queue,local_d[index]->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(the_locks, local_d, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(the_locks, local_d, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    write_acquire(&the_locks[index],&(local_d[index]));
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(&the_locks[index]);
#endif
}

static inline void release_write(int index, int cluster_id, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_release(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_qnode=hclh_release(local_d[index]->my_qnode,local_d[index]->my_pred,cluster_id);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(the_locks, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(the_locks, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    write_release(&the_locks[index]); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(&the_locks[index]);
#endif
}

static inline void acquire_read(unsigned int index, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_acquire(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_pred= (qnode*) hclh_acquire(local_d[index]->my_queue,the_locks[index]->shared_queue,local_d[index]->my_qnode);
#elif defined(USE_TTAS_LOCKS)
    ttas_lock(the_locks, local_d, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_lock(the_locks, local_d, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_lock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    read_acquire(&the_locks[index],&(local_d[index]));
#elif defined(USE_TICKET_LOCKS)
    ticket_acquire(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_lock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_lock(&the_locks[index]);
#endif
}

static inline void release_read(int index, int cluster_id, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_release(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_qnode=hclh_release(local_d[index]->my_qnode,local_d[index]->my_pred,cluster_id);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(the_locks, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(the_locks, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    read_release(&the_locks[index]); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_release(&the_locks[index]);
#endif
}

static inline local_data init_local(int core_to_pin, int num_locks, global_data the_locks){
#ifdef USE_MCS_LOCKS
    return init_mcs_thread(core_to_pin, num_locks);
#elif defined(USE_HCLH_LOCKS)
    return init_thread_hclh(core_to_pin, num_locks, the_locks);
#elif defined(USE_TTAS_LOCKS)
    return init_thread_ttas(core_to_pin, num_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_thread_spinlock(core_to_pin, num_locks);
#elif defined(USE_ARRAY_LOCKS)
    return init_thread_alock(core_to_pin, num_locks, the_locks);
#elif defined(USE_RW_LOCKS)
    return init_thread_rw_ttas(core_to_pin, num_locks);
#elif defined(USE_TICKET_LOCKS)
    init_thread_ticketlocks(core_to_pin);
    return NULL;
#elif defined(USE_MUTEX_LOCKS)
    //assign the thread to the correct core
    set_cpu(core_to_pin);
    return NULL;
#elif defined(USE_HTICKET_LOCKS)
    init_thread_htlocks(core_to_pin);
    return NULL;
#endif
}


static inline void free_local(local_data local_d, int num_locks){
#ifdef USE_MCS_LOCKS
    end_thread_mcs(local_d,num_locks);
#elif defined(USE_HCLH_LOCKS)
    end_thread_hclh(local_d,num_locks);
#elif defined(USE_TTAS_LOCKS)
    end_thread_ttas(local_d);
#elif defined(USE_SPINLOCK_LOCKS)
    end_thread_spinlock(local_d);
#elif defined(USE_ARRAY_LOCKS)
    end_thread_alock(local_d,num_locks);
#elif defined(USE_RW_LOCKS)
    end_thread_rw_ttas(local_d);
#elif defined(USE_TICKET_LOCKS)
    //nothing to be done
#elif defined(USE_MUTEX_LOCKS)
    //nothing to be done
#elif defined(USE_HTICKET_LOCKS)
    //nothing to be done
#endif
}


static inline global_data init_global(int num_locks, int num_threads){
#ifdef USE_MCS_LOCKS
    return init_mcs_locks(num_locks);
#elif defined(USE_HCLH_LOCKS)
    return init_hclh_locks(num_locks);
#elif defined(USE_TTAS_LOCKS)
    return init_ttas_locks(num_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    return init_spinlock_locks(num_locks);
#elif defined(USE_ARRAY_LOCKS)
    return init_array_locks(num_locks, num_threads);
#elif defined(USE_RW_LOCKS)
    return init_rw_ttas_locks(num_locks);
#elif defined(USE_TICKET_LOCKS)
    return init_ticketlocks(num_locks);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_t * the_locks;
    the_locks = (pthread_mutex_t*) malloc(num_locks * sizeof(pthread_mutex_t));
    int i;
    for (i=0;i<num_locks;i++) {
        pthread_mutex_init(&the_locks[i], NULL);
    }
    return the_locks;
#elif defined(USE_HTICKET_LOCKS)
    return init_htlocks(num_locks);
#endif
}

static inline void free_global(global_data the_locks, int num_locks) {
#ifdef USE_MCS_LOCKS
    end_mcs(the_locks, num_locks);
#elif defined(USE_HCLH_LOCKS)
    end_hclh(the_locks, num_locks);
#elif defined(USE_TTAS_LOCKS)
    end_ttas(the_locks);
#elif defined(USE_SPINLOCK_LOCKS)
    end_spinlock(the_locks);
#elif defined(USE_ARRAY_LOCKS)
    end_alock(the_locks, num_locks);
#elif defined(USE_RW_LOCKS)
    end_rw_ttas(the_locks);
#elif defined(USE_TICKET_LOCKS)
    free_ticketlocks(the_locks);
#elif defined(USE_MUTEX_LOCKS)
    int i;
    for (i=0;i<num_locks;i++) {
        pthread_mutex_destroy(&the_locks[i]);
    }
#elif defined(USE_HTICKET_LOCKS)
    free_htlocks(the_locks);
#endif
}


//checks whether the lock is free; if it is, acquire it;
//we use this in memcached to simulate trylocks
//return 0 on success, 1 otherwise
static inline int na_trylock(unsigned int index, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    return mcs_trylock(the_locks[index],local_d[index]);
    //if (is_free_mcs(the_locks[index])) {
    //    mcs_acquire(the_locks[index],local_d[index]);
    //    return 0;
    //}
   //return 1;
#elif defined(USE_HCLH_LOCKS)

    if (is_free_hclh(local_d[index]->my_queue,the_locks[index]->shared_queue,local_d[index]->my_qnode)) {
        local_d[index]->my_pred= (qnode*) hclh_acquire(local_d[index]->my_queue,the_locks[index]->shared_queue,local_d[index]->my_qnode);
        return 0;
    }
   return 1;
#elif defined(USE_TTAS_LOCKS)
    return ttas_trylock(the_locks, local_d, index);
    //if (is_free_ttas(the_locks, index)) {
    //    ttas_lock(the_locks, local_d, index);
    //    return 0;
    //}
   //return 1;
#elif defined(USE_SPINLOCK_LOCKS)
    return spinlock_trylock(the_locks, local_d, index);
    //if (is_free_spinlock(the_locks, index)) {
    //    spinlock_lock(the_locks, local_d, index);
    //    return 0;
   // }
   //return 1;
#elif defined(USE_ARRAY_LOCKS)
    return alock_trylock(local_d[index]);
//    if (is_free_alock(the_locks[index])) {
//        alock_lock(local_d[index]);
//        return 0;
//    }
//   return 1;
#elif defined(USE_RW_LOCKS)
    return rw_trylock(&the_locks[index],&(local_d[index]));
//    if (is_free_rw(&the_locks[index])) {
//        write_acquire(&the_locks[index],&(local_d[index]));
//        return 0;
//    }
//   return 1;
#elif defined(USE_TICKET_LOCKS)
    return ticket_trylock(&the_locks[index]);
    //if (is_free_ticket(&the_locks[index])) {
    //    ticket_acquire(&the_locks[index]);
    //    return 0;
   // }
   //return 1;
#elif defined(USE_MUTEX_LOCKS)
    return pthread_mutex_trylock(&the_locks[index]);
#elif defined(USE_HTICKET_LOCKS)
        return htlock_trylock(&the_locks[index]);
//    if (is_free_hticket(&the_locks[index])) {
//        htlock_lock(&the_locks[index]);
//        return 0;
//    }
//   return 1;
#endif
}

static inline void release_trylock(int index, int cluster_id, local_data local_d, global_data the_locks) {
#ifdef USE_MCS_LOCKS
    mcs_release(the_locks[index],local_d[index]);
#elif defined(USE_HCLH_LOCKS)
    local_d[index]->my_qnode=hclh_release(local_d[index]->my_qnode,local_d[index]->my_pred,cluster_id);
#elif defined(USE_TTAS_LOCKS)
    ttas_unlock(the_locks, index);
#elif defined(USE_SPINLOCK_LOCKS)
    spinlock_unlock(the_locks, index);
#elif defined(USE_ARRAY_LOCKS)
    alock_unlock(local_d[index]);
#elif defined(USE_RW_LOCKS)
    write_release(&the_locks[index]); 
#elif defined(USE_TICKET_LOCKS)
    ticket_release(&the_locks[index]);
#elif defined(USE_MUTEX_LOCKS)
    pthread_mutex_unlock(&(the_locks[index]));
#elif defined(USE_HTICKET_LOCKS)
    htlock_release_try(&the_locks[index]);
#endif
}

