#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/time.h>
#include <unistd.h>
#include "utils.h"
#include "atomic_ops.h"
#ifdef __sparc__
#  include <sys/types.h>
#  include <sys/processor.h>
#  include <sys/procset.h>
#include "include/dht.h"
#include "include/mcore_malloc.h"
#else
#include <numa.h>
#include "dht.h"
#include "mcore_malloc.h"
#endif

#define DEBUG

/* ################################################################### *
 * GLOBALS
 * ################################################################### */

hashtable_t *hashtable;
int capacity;
int num_threads;
int num_elements;
int duration;
float filling_rate;
float update_rate;
float get_rate;
int payload_size;

int seed = 0;
__thread unsigned long * seeds;
int rand_max, rand_min;

static volatile int stop;
__thread uint32_t phys_id;

volatile local_data * local_th_data;

volatile ticks *putting_acqs;
volatile ticks *putting_rels;
volatile ticks *putting_opts;
volatile ticks *getting_acqs;
volatile ticks *getting_rels;
volatile ticks *getting_opts;
volatile ticks *removing_acqs;
volatile ticks *removing_rels;
volatile ticks *removing_opts;
volatile ticks *putting_count;
volatile ticks *putting_count_succ;
volatile ticks *getting_count;
volatile ticks *getting_count_succ;
volatile ticks *removing_count;
volatile ticks *removing_count_succ;
volatile ticks *total;

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier {
    pthread_cond_t complete;
    pthread_mutex_t mutex;
    int count;
    int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n) {
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    /* One more thread through */
    b->crossing++;
    /* If not all here, wait */
    if (b->crossing < b->count) {
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        pthread_cond_broadcast(&b->complete);
        /* Reset for next time */
        b->crossing = 0;
    }
    pthread_mutex_unlock(&b->mutex);
}
barrier_t barrier, barrier_global;

void *procedure(void *threadid) 
{
  uint64_t id_tmp = (uint64_t)threadid;
  uint8_t ID = (uint8_t) id_tmp;
  phys_id = the_cores[ID];
    
  set_cpu(phys_id);
    
  void* mcore_mem_tmp = (void*) malloc(MCORE_SIZE);
  assert(mcore_mem_tmp != NULL);
  MCORE_shmalloc_set(mcore_mem_tmp);
    
  local_th_data[ID] = init_local(phys_id, capacity, hashtable->the_locks);
    
#ifndef COMPUTE_THROUGHPUT
  ticks my_putting_acqs = 0;
  ticks my_putting_rels = 0;
  ticks my_putting_opts = 0;
  ticks my_getting_acqs = 0;
  ticks my_getting_rels = 0;
  ticks my_getting_opts = 0;
  ticks my_removing_acqs = 0;
  ticks my_removing_rels = 0;
  ticks my_removing_opts = 0;
#endif
  uint64_t my_putting_count = 0;
  uint64_t my_putting_count_succ = 0;
  uint64_t my_getting_count = 0;
  uint64_t my_getting_count_succ = 0;
  uint64_t my_removing_count = 0;
  uint64_t my_removing_count_succ = 0;
    
#ifndef COMPUTE_THROUGHPUT
  ticks start_acq, end_acq;
  ticks start_rel, end_rel;
  ticks correction = getticks_correction_calc();
#endif
    
  seeds = seed_rand();
    
  uint64_t key;
  int bin;
  void * value, * local;
  int c = 0;
  int scale_update = (int)(update_rate * 128);
  uint8_t putting = 1;
    
  int i;
  uint32_t num_elems_thread = (uint32_t) (num_elements * filling_rate / num_threads);
  int32_t missing = (uint32_t) (num_elements * filling_rate) - (num_elems_thread * num_threads);
  if (ID < missing)
    {
      num_elems_thread++;
    }
    
  local = MCORE_shmalloc(payload_size);
    
  for(i = 0; i < num_elems_thread; i++) 
    {
      key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
      bin = ht_hash( hashtable, key );
      value = MCORE_shmalloc( payload_size );
      acquire_lock(bin, local_th_data[ID], hashtable->the_locks);
      if(!ht_put( hashtable, key, value, bin)) 
	{
	  i--;
	}
      
      MEM_BARRIER;
      release_lock(bin, get_cluster(phys_id), local_th_data[ID], hashtable->the_locks);
    }


  value = NULL;

  barrier_cross(&barrier);

#if defined(DEBUG)
  if (!ID)
    {
      printf("size of ht is: %u\n", ht_size(hashtable, capacity));
    }
#endif

  barrier_cross(&barrier_global);


  /* uint64_t num_get = 0, num_get_succ = 0; */
  /* uint64_t num_rem = 0, num_rem_succ = 0; */
  
    
  int succ = 1;
  while (stop == 0) 
    {
      key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
        
#ifndef SEQUENTIAL
      bin = ht_hash( hashtable, key );
#endif
      if(succ) 
	{
	  c = (int)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & 0x7f);
	}

      uint8_t update = (c < scale_update);

      if(update && putting && value == NULL) 
	{
	  value = MCORE_shmalloc(payload_size);
	}

      succ = 0;
        
        
#ifndef COMPUTE_THROUGHPUT
      start_acq = getticks();
#endif
#ifndef SEQUENTIAL
      acquire_lock(bin, local_th_data[ID], hashtable->the_locks);
#endif
#ifndef COMPUTE_THROUGHPUT
      end_acq = getticks();
#endif
      if(update) 
	{
	  if(putting) 
	    {
	      if(ht_put( hashtable, key, value, bin ))
		{
		  succ = 1;
		  value = NULL;
		}
	    } 
	  else 
	    {
	      void* removed = ht_remove(hashtable, key, bin);
	      if(removed != NULL) 
		{
		  succ = 1;
		  MCORE_shfree(removed);
		}
	    }
            
	} 
      else
	{ 
	  value = ht_get( hashtable, key, bin );
	  if(value != NULL) 
	    {
	      succ = 1;
	      memcpy(local, value, payload_size);
	    }
	}
        
      //MEM_BARRIER;
#ifndef COMPUTE_THROUGHPUT
      start_rel = getticks();
#endif
#ifndef SEQUENTIAL
      MEM_BARRIER;
      release_lock(bin, get_cluster(phys_id), local_th_data[ID], hashtable->the_locks);
      //MEM_BARRIER;
#endif
#ifndef COMPUTE_THROUGHPUT
      end_rel = getticks();
#endif
        
      if(update) 
	{
	  if(putting) 
	    {
	      if (succ)
		{
		  my_putting_count_succ++;
		  putting = false;
		}
#ifndef COMPUTE_THROUGHPUT
	      my_putting_acqs += (end_acq - start_acq - correction);
	      my_putting_rels += (end_rel - start_rel - correction);
	      my_putting_opts += (start_rel - end_acq - correction);
#endif
	      my_putting_count++;
	    } 
	  else 			/* removing */
	    {
	      if (succ)
		{
		  my_removing_count_succ++;
		  putting = true;
		}
#ifndef COMPUTE_THROUGHPUT
	      my_removing_acqs += (end_acq - start_acq - correction);
	      my_removing_rels += (end_rel - start_rel - correction);
	      my_removing_opts += (start_rel - end_acq - correction);
#endif
	      my_removing_count++;
	    }
	} 
      else
	{ //if(c < scale_update_get) {
	  if (succ)
	    {
	      my_getting_count_succ++;
	    }
#ifndef COMPUTE_THROUGHPUT
	  my_getting_acqs += (end_acq - start_acq - correction);
	  my_getting_rels += (end_rel - start_rel - correction);
	  my_getting_opts += (start_rel - end_acq - correction);
#endif
	  my_getting_count++;
	}
    }
    
    
  /* printf("gets: %-10llu / succ: %llu\n", num_get, num_get_succ); */
  /* printf("rems: %-10llu / succ: %llu\n", num_rem, num_rem_succ); */
  barrier_cross(&barrier);
#if defined(DEBUG)
  if (!ID)
    {
      printf("size of ht is: %u\n", ht_size(hashtable, capacity));
    }
#endif

#ifndef COMPUTE_THROUGHPUT
  putting_acqs[ID] += my_putting_acqs;
  putting_rels[ID] += my_putting_rels;
  putting_opts[ID] += my_putting_opts;
  getting_acqs[ID] += my_getting_acqs;
  getting_rels[ID] += my_getting_rels;
  getting_opts[ID] += my_getting_opts;
  removing_acqs[ID] += my_removing_acqs;
  removing_rels[ID] += my_removing_rels;
  removing_opts[ID] += my_removing_opts;
#endif
  putting_count[ID] += my_putting_count;
  putting_count_succ[ID] += my_putting_count_succ;
  getting_count[ID] += my_getting_count;
  getting_count_succ[ID] += my_getting_count_succ;
  removing_count[ID]+= my_removing_count;
  removing_count_succ[ID]+= my_removing_count_succ;
    
  free_local(local_th_data[ID], hashtable->the_locks);

  pthread_exit(NULL);
}

int main( int argc, char **argv ) {
    
  if ( argc == 9 ) {
    capacity = atoi( argv[1] );
    num_threads = atoi( argv[2] );
    num_elements = atoi( argv[3] );
    filling_rate = atof( argv[4] );
    payload_size = atoi( argv[5] );
    duration = atoi( argv[6] );
    update_rate = atof( argv[7] );
    get_rate = atof( argv[8] );
  } else {
    printf("ERROR; usage ./main table_capacity num_threads num_elements filling_rate payload_size duration update_rate get_rate\n");
    exit(-1);
  }
    
  rand_max = num_elements;
  rand_min = 1;
    
  struct timeval start, end;
  struct timespec timeout;
  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;
    
  stop = 0;
    
  /* Initialize the hashtable */
  hashtable = ht_create( capacity );
  hashtable->the_locks = init_global( capacity, num_threads );
  /* Initializes the local data */
  local_th_data = (local_data *) malloc( sizeof( local_data ) * num_threads );

  putting_acqs = (ticks *) calloc( num_threads , sizeof( ticks ) );
  putting_rels = (ticks *) calloc( num_threads , sizeof( ticks ) );
  putting_opts = (ticks *) calloc( num_threads , sizeof( ticks ) );
  getting_acqs = (ticks *) calloc( num_threads , sizeof( ticks ) );
  getting_rels = (ticks *) calloc( num_threads , sizeof( ticks ) );
  getting_opts = (ticks *) calloc( num_threads , sizeof( ticks ) );
  removing_acqs = (ticks *) calloc( num_threads , sizeof( ticks ) );
  removing_rels = (ticks *) calloc( num_threads , sizeof( ticks ) );
  removing_opts = (ticks *) calloc( num_threads , sizeof( ticks ) );
  putting_count = (ticks *) calloc( num_threads , sizeof( ticks ) );
  putting_count_succ = (ticks *) calloc( num_threads , sizeof( ticks ) );
  getting_count = (ticks *) calloc( num_threads , sizeof( ticks ) );
  getting_count_succ = (ticks *) calloc( num_threads , sizeof( ticks ) );
  removing_count = (ticks *) calloc( num_threads , sizeof( ticks ) );
  removing_count_succ = (ticks *) calloc( num_threads , sizeof( ticks ) );
    
  pthread_t threads[num_threads];
  pthread_attr_t attr;
  int rc;
  void *status;
    
  barrier_init(&barrier_global, num_threads + 1);
  barrier_init(&barrier, num_threads);
    
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
  uint64_t t;
  for(t = 0; t < num_threads; t++){
    
    //printf("In main: creating thread %ld\n", t);
    rc = pthread_create(&threads[t], &attr, procedure, (void *)t);
    if (rc){
      printf("ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
        
  }
    
  /* Free attribute and wait for the other threads */
  pthread_attr_destroy(&attr);
    
  barrier_cross(&barrier_global);
    
  gettimeofday(&start, NULL);
  nanosleep(&timeout, NULL);

  stop = 1;
  gettimeofday(&end, NULL);
    
    
  for(t = 0; t < num_threads; t++) 
    {
      rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
        
      duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
        
    }
    
  ticks putting_acq_total = 0;
  ticks putting_rel_total = 0;
  ticks putting_opt_total = 0;
  ticks getting_acq_total = 0;
  ticks getting_rel_total = 0;
  ticks getting_opt_total = 0;
  ticks removing_acq_total = 0;
  ticks removing_rel_total = 0;
  ticks removing_opt_total = 0;
  uint64_t putting_count_total = 0;
  uint64_t putting_count_total_succ = 0;
  uint64_t getting_count_total = 0;
  uint64_t getting_count_total_succ = 0;
  uint64_t removing_count_total = 0;
  uint64_t removing_count_total_succ = 0;
    
  for(t=0; t < num_threads; t++) 
    {
      putting_acq_total += putting_acqs[t];
      putting_rel_total += putting_rels[t];
      putting_opt_total += putting_opts[t];
      getting_acq_total += getting_acqs[t];
      getting_rel_total += getting_rels[t];
      getting_opt_total += getting_opts[t];
      removing_acq_total += removing_acqs[t];
      removing_rel_total += removing_rels[t];
      removing_opt_total += removing_opts[t];
      putting_count_total += putting_count[t];
      putting_count_total_succ += putting_count_succ[t];
      getting_count_total += getting_count[t];
      getting_count_total_succ += getting_count_succ[t];
      removing_count_total += removing_count[t];
      removing_count_total_succ += removing_count_succ[t];
    }
  if(putting_count_total == 0) 
    {
      putting_acq_total = 0;
      putting_rel_total = 0;
      putting_opt_total = 0;
      putting_count_total = 1;
    }
    
  if(getting_count_total == 0) 
    {
      getting_acq_total = 0;
      getting_rel_total = 0;
      getting_opt_total = 0;
      getting_count_total = 1;
    }
    
  if(removing_count_total == 0) 
    {
      removing_acq_total = 0;
      removing_rel_total = 0;
      removing_opt_total = 0;
      removing_count_total = 1;
    }
    
#ifndef COMPUTE_THROUGHPUT
#if defined(DEBUG)
  printf("#thread put_acq put_rel put_cs  put_tot get_acq get_rel get_cs  get_tot rem_acq rem_rel rem_cs  rem_tot\n");
#endif
  printf("%d\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
	 num_threads,
	 putting_acq_total / putting_count_total,
	 putting_rel_total / putting_count_total,
	 putting_opt_total / putting_count_total,
	 putting_acq_total / putting_count_total + putting_rel_total / putting_count_total + putting_opt_total / putting_count_total,
	 getting_acq_total / getting_count_total,
	 getting_rel_total / getting_count_total,
	 getting_opt_total / getting_count_total,
	 getting_acq_total / getting_count_total + getting_rel_total / getting_count_total + getting_opt_total / getting_count_total,
	 removing_acq_total / removing_count_total,
	 removing_rel_total / removing_count_total,
	 removing_opt_total / removing_count_total,
	 removing_acq_total / removing_count_total + removing_rel_total / removing_count_total + removing_opt_total / removing_count_total
	 );
#endif
    
    
#ifdef COMPUTE_THROUGHPUT
#define LLU long long unsigned int

#if defined(DEBUG)
  printf("    : %-10s | %-10s | %-11s | %s\n", "total", "success", "succ %", "total %");
  uint64_t total = putting_count_total + getting_count_total + removing_count_total;
  double putting_perc = 100.0 * (1 - ((double)(total - putting_count_total) / total));
  double getting_perc = 100.0 * (1 - ((double)(total - getting_count_total) / total));
  double removing_perc = 100.0 * (1 - ((double)(total - removing_count_total) / total));

  printf("puts: %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) putting_count_total, 
	 (LLU) putting_count_total_succ,
	 (double) (putting_count_total - putting_count_total_succ) / putting_count_total * 100,
	 putting_perc);
  printf("gets: %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) getting_count_total, 
	 (LLU) getting_count_total_succ,
	 (double) (getting_count_total - getting_count_total_succ) / getting_count_total * 100,
	 getting_perc);
  printf("rems; %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) removing_count_total, 
	 (LLU) removing_count_total_succ,
	 (double) (removing_count_total - removing_count_total_succ) / removing_count_total * 100,
	 removing_perc);
  /* printf("puts - gets: %d\n", (int) (putting_count_total_succ - removing_count_total_succ)); */
#endif
  float throughput = (putting_count_total + getting_count_total + removing_count_total) * 1000.0 / duration;
  printf("%d\t%f\n", num_threads, throughput);
#endif
    
  free_global(hashtable->the_locks, hashtable->capacity);
  ht_destroy( hashtable );
    
  /* Last thing that main() should do */
  //printf("Main: program completed. Exiting.\n");
  pthread_exit(NULL);
    
  return 0;
}
