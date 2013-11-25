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
#  include "include/dht.h"
#  include "include/mcore_malloc.h"
#else
#  include "dht_res.h"
#  include "mcore_malloc.h"
#endif

/* #define DETAILED_THROUGHPUT */

/* ################################################################### *
 * GLOBALS
 * ################################################################### */


hashtable_t** hashtable;
int num_buckets = 256;
int num_threads = 1;
int num_elements = 2048;
int duration = 1000;
float filling_rate = 0.5;
float update_rate = 0.1;
float get_rate = 0.9;

int seed = 0;
__thread unsigned long * seeds;
uint32_t rand_max;
#define rand_min 1

static volatile int stop;
__thread uint32_t phys_id;

volatile ticks *putting_succ;
volatile ticks *putting_fail;
volatile ticks *getting_succ;
volatile ticks *getting_fail;
volatile ticks *removing_succ;
volatile ticks *removing_fail;
volatile ticks *putting_count;
volatile ticks *putting_count_succ;
volatile ticks *getting_count;
volatile ticks *getting_count_succ;
volatile ticks *removing_count;
volatile ticks *removing_count_succ;
volatile ticks *total;


/* ################################################################### *
 * LOCALS
 * ################################################################### */

#ifdef DEBUG
extern __thread uint32_t put_num_restarts;
extern __thread uint32_t put_num_failed_expand;
extern __thread uint32_t put_num_failed_on_new;
#endif

/* ################################################################### *
 * BARRIER
 * ################################################################### */

typedef struct barrier 
{
  pthread_cond_t complete;
  pthread_mutex_t mutex;
  int count;
  int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n) 
{
  pthread_cond_init(&b->complete, NULL);
  pthread_mutex_init(&b->mutex, NULL);
  b->count = n;
  b->crossing = 0;
}

void barrier_cross(barrier_t *b) 
{
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


#if defined(COMPUTE_THROUGHPUT)
#  define START_TS()
#  define END_TS()
#  define ADD_DUR(tar)
#  define ADD_DUR_FAIL(tar)
#else
#  define START_TS()   start_acq = getticks()
#  define END_TS()     end_acq = getticks()
#  define ADD_DUR(tar) tar += (end_acq - start_acq - correction)
#  define ADD_DUR_FAIL(tar)					\
  else								\
    {								\
      ADD_DUR(tar);						\
    }

#endif

typedef struct thread_data
{
  uint8_t id;
  hashtable_t** ht;
} thread_data_t;


void*
test(void* thread) 
{
  thread_data_t* td = (thread_data_t*) thread;
  uint8_t ID = td->id;
  phys_id = the_cores[ID];
  set_cpu(phys_id);

  hashtable_t** hashtable = td->ht;
    
    
#if !defined(COMPUTE_THROUGHPUT)
  volatile ticks my_putting_succ = 0;
  volatile ticks my_putting_fail = 0;
  volatile ticks my_getting_succ = 0;
  volatile ticks my_getting_fail = 0;
  volatile ticks my_removing_succ = 0;
  volatile ticks my_removing_fail = 0;
#endif
  uint64_t my_putting_count = 0;
  uint64_t my_getting_count = 0;
  uint64_t my_removing_count = 0;

  uint64_t my_putting_count_succ = 0;
  uint64_t my_getting_count_succ = 0;
  uint64_t my_removing_count_succ = 0;
    
#if !defined(COMPUTE_THROUGHPUT)
  volatile ticks start_acq, end_acq;
  volatile ticks correction = getticks_correction_calc();
#endif
    
  seeds = seed_rand();
    
  uint64_t key;
  int c = 0;
  int scale_update = (int)(update_rate * 256);
  uint8_t putting = 1;
    
  int i;
  uint32_t num_elems_thread = (uint32_t) (num_elements * filling_rate / num_threads);
  int32_t missing = (uint32_t) (num_elements * filling_rate) - (num_elems_thread * num_threads);
  if (ID < missing)
    {
      num_elems_thread++;
    }
    
  for(i = 0; i < num_elems_thread; i++) 
    {
      key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
      
      if(!ht_put(hashtable, key)) 
	{
	  i--;
	}
    }
  MEM_BARRIER;


  barrier_cross(&barrier);

#if defined(DEBUG)
  if (!ID)
    {
      printf("size of ht is: %lu\n", ht_size(*hashtable));
      /* ht_print(hashtable, num_buckets); */
    }
#else
  if (!ID)
    {
      if(ht_size(hashtable, num_buckets) == 3321445)
	{
	  printf("size of ht is: %u\n", ht_size(hashtable, num_buckets));
	}
    }  
#endif

  barrier_cross(&barrier_global);

  uint8_t update = false;
  int succ = 1;
  while (stop == 0) 
    {
      key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
        
      if(succ)
      	{
      	  c = (uint8_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])));
      	  update = (c < scale_update);

      	  succ = 0;
      	}

      uint32_t resize = (my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) % (50000)) < 1;
      if (resize)
      	{
	  START_TS();
      	  if (ht_resize_pes(hashtable))
	    {
	      END_TS();
#if !defined(COMPUTE_THROUGHPUT)
	      printf("** resize cost: %lu\n", end_acq - start_acq - correction);
#endif
	    }
      	}

      if(update) 
	{
	  if(putting) 
	    {
	      START_TS();
	      int res = ht_put(hashtable, key);
	      END_TS();
	      if(res)
		{
		  ADD_DUR(my_putting_succ);
		  succ = 1;
		  my_putting_count_succ++;
		}
	      ADD_DUR_FAIL(my_putting_fail);
	      my_putting_count++;
	      putting = false;
	    } 
	  else 
	    {
	      START_TS();
	      ssht_addr_t removed = ht_remove(hashtable, key);
	      END_TS();
	      if(removed != 0) 
		{
		  ADD_DUR(my_removing_succ);
		  succ = 1;
		  my_removing_count_succ++;
		}
	      ADD_DUR_FAIL(my_removing_fail);
	      my_removing_count++;
	      putting = true;
	    }
	} 
      else
	{ 
	  START_TS();
	  void* res = ht_get(hashtable, key);
	  END_TS();
	  if(res != NULL) 
	    {
	      ADD_DUR(my_getting_succ);
	      succ = 1;
	      my_getting_count_succ++;
	    }
	  ADD_DUR_FAIL(my_getting_fail);
	  my_getting_count++;
	}
    }
        
  /* #if !defined(COMPUTE_THROUGHPUT) */
  /*       end_rel = getticks(); */
  /* #endif */
        
  /* #if defined(DETAILED_THROUGHPUT) */
  /*       if(update)  */
  /* 	{ */
  /* 	  if(putting)  */
  /* 	    { */
  /* #  if defined(DEBUG) */
  /* 	      my_putting_count_succ += succ; */
  /* #  endif	/\* debug *\/ */
  /* #  ifndef COMPUTE_THROUGHPUT */
  /* 	      my_putting_succ += (end_acq - start_acq - correction); */
  /* 	      my_putting_fail += (end_rel - start_rel - correction); */
  /* 	      my_putting_opts += (start_rel - end_acq - correction); */
  /* #  endif */
  /* 	      my_putting_count++; */
  /* 	    }  */
  /* 	  else 			/\* removing *\/ */
  /* 	    { */
  /* #  if defined(DEBUG) */
  /* 	      my_removing_count_succ += succ; */
  /* #  endif	/\* debug *\/ */
  /* #  ifndef COMPUTE_THROUGHPUT */
  /* 	      my_removing_succ += (end_acq - start_acq - correction); */
  /* 	      my_removing_fail += (end_rel - start_rel - correction); */
  /* 	      my_removing_opts += (start_rel - end_acq - correction); */
  /* #  endif */
  /* 	      my_removing_count++; */
  /* 	    } */
  /* 	}  */
  /*       else */
  /* 	{ //if(c < scale_update_get) { */
  /* #  if defined(DEBUG) */
  /* 	  my_getting_count_succ += succ; */
  /* #  endif */
  /* #  ifndef COMPUTE_THROUGHPUT */
  /* 	  my_getting_succ += (end_acq - start_acq - correction); */
  /* 	  my_getting_fail += (end_rel - start_rel - correction); */
  /* 	  my_getting_opts += (start_rel - end_acq - correction); */
  /* #  endif */
  /* 	  my_getting_count++; */
  /* 	} */

  /* #else  /\* not detaild throughput *\/ */
  /*       my_getting_count++; */
  /* #endif */
    
#if defined(DEBUG)
  if (put_num_restarts | put_num_failed_expand | put_num_failed_on_new)
    {
      printf("put_num_restarts = %3u / put_num_failed_expand = %3u / put_num_failed_on_new = %3u \n", 
	     put_num_restarts, put_num_failed_expand, put_num_failed_on_new);
    }
#endif
    
  /* printf("gets: %-10llu / succ: %llu\n", num_get, num_get_succ); */
  /* printf("rems: %-10llu / succ: %llu\n", num_rem, num_rem_succ); */
  barrier_cross(&barrier);
#if defined(DEBUG)
  if (!ID)
    {
      printf("size of ht is: %lu\n", ht_size(*hashtable));
    }
#else
  if (!ID)
    {
      if(ht_size(hashtable, num_buckets) == 3321445)
	{
	  printf("size of ht is: %u\n", ht_size(hashtable, num_buckets));
	}
    }  
#endif

#if !defined(COMPUTE_THROUGHPUT)
  putting_succ[ID] += my_putting_succ;
  putting_fail[ID] += my_putting_fail;
  getting_succ[ID] += my_getting_succ;
  getting_fail[ID] += my_getting_fail;
  removing_succ[ID] += my_removing_succ;
  removing_fail[ID] += my_removing_fail;
#endif
  putting_count[ID] += my_putting_count;
  getting_count[ID] += my_getting_count;
  removing_count[ID]+= my_removing_count;

  putting_count_succ[ID] += my_putting_count_succ;
  getting_count_succ[ID] += my_getting_count_succ;
  removing_count_succ[ID]+= my_removing_count_succ;

  pthread_exit(NULL);
}

int
main(int argc, char **argv) 
{
  set_cpu(the_cores[0]);
    
  struct option long_options[] = {
    // These options don't set a flag
    {"help",                      no_argument,       NULL, 'h'},
    {"duration",                  required_argument, NULL, 'd'},
    {"initial-size",              required_argument, NULL, 'i'},
    {"num-threads",               required_argument, NULL, 'n'},
    {"range",                     required_argument, NULL, 'r'},
    {"update-rate",               required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0}
  };

  size_t initial = 1024, range = 2048, update = 20, load_factor = 2;

  int i, c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hAf:d:i:n:r:s:u:m:a:l:x:", long_options, &i);
		
      if(c == -1)
	break;
		
      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;
		
      switch(c) {
      case 0:
	/* Flag is automatically set */
	break;
      case 'h':
	printf("intset -- STM stress test "
	       "(linked list)\n"
	       "\n"
	       "Usage:\n"
	       "  intset [options...]\n"
	       "\n"
	       "Options:\n"
	       "  -h, --help\n"
	       "        Print this message\n"
	       "  -d, --duration <int>\n"
	       "        Test duration in milliseconds\n"
	       "  -i, --initial-size <int>\n"
	       "        Number of elements to insert before test)\n"
	       "  -n, --num-threads <int>\n"
	       "        Number of threads)\n"
	       "  -r, --range <int>\n"
	       "        Range of integer values inserted in set)\n"
	       "  -u, --update-rate <int>\n"
	       "        Percentage of update transactions)\n"
	      );
	exit(0);
      case 'd':
	duration = atoi(optarg);
	break;
      case 'i':
	initial = atoi(optarg);
	break;
      case 'n':
	num_threads = atoi(optarg);
	break;
      case 'r':
	range = atol(optarg);
	break;
      case 'u':
	update = atoi(optarg);
	break;
      case 'l':
	load_factor = atoi(optarg);
	break;
      case '?':
	printf("Use -h or --help for help\n");
	exit(0);
      default:
	exit(1);
      }
    }


  if (range <= initial)
    {
      range = 2 * initial;
    }


  num_buckets = initial / load_factor;

  if (!is_power_of_two(num_buckets))
    {
      size_t num_buckets_pow2 = pow2roundup(num_buckets);
      printf("** rounding up num_buckets (to make it power of 2): old: %lu / new: %lu\n", num_buckets, num_buckets_pow2);
      num_buckets = num_buckets_pow2;
    }


  double kb = num_buckets * sizeof(bucket_t) / 1024.0;
  double mb = kb / 1024.0;
  printf("Sizeof initial: %.2f KB = %.2f MB\n", kb, mb);

  if (!is_power_of_two(range))
    {
      size_t range_pow2 = pow2roundup(range);
      printf("** rounding up range (to make it power of 2): old: %lu / new: %lu\n", range, range_pow2);
      range = range_pow2;
    }

  num_elements = range;
  filling_rate = (double) initial / range;
  update_rate = update / 100.0;
  get_rate = 1 - update_rate;


  /* printf("num_threads = %u\n", num_threads); */
  /* printf("cap: = %u\n", num_buckets); */
  /* printf("num elem = %u\n", num_elements); */
  /* printf("filing rate= %f\n", filling_rate); */
  /* printf("update = %f\n", update_rate); */


  rand_max = num_elements - 1;
    
  struct timeval start, end;
  struct timespec timeout;
  timeout.tv_sec = duration / 1000;
  timeout.tv_nsec = (duration % 1000) * 1000000;
    
  stop = 0;
    
  /* Initialize the hashtable */

  hashtable_t** hashtable = (hashtable_t**) memalign(CACHE_LINE_SIZE, CACHE_LINE_SIZE);
  assert(hashtable != NULL);
  *hashtable = ht_create(num_buckets);

  /* Initializes the local data */
  putting_succ = (ticks *) calloc(num_threads , sizeof(ticks));
  putting_fail = (ticks *) calloc(num_threads , sizeof(ticks));
  getting_succ = (ticks *) calloc(num_threads , sizeof(ticks));
  getting_fail = (ticks *) calloc(num_threads , sizeof(ticks));
  removing_succ = (ticks *) calloc(num_threads , sizeof(ticks));
  removing_fail = (ticks *) calloc(num_threads , sizeof(ticks));
  putting_count = (ticks *) calloc(num_threads , sizeof(ticks));
  putting_count_succ = (ticks *) calloc(num_threads , sizeof(ticks));
  getting_count = (ticks *) calloc(num_threads , sizeof(ticks));
  getting_count_succ = (ticks *) calloc(num_threads , sizeof(ticks));
  removing_count = (ticks *) calloc(num_threads , sizeof(ticks));
  removing_count_succ = (ticks *) calloc(num_threads , sizeof(ticks));
    
  pthread_t threads[num_threads];
  pthread_attr_t attr;
  int rc;
  void *status;
    
  barrier_init(&barrier_global, num_threads + 1);
  barrier_init(&barrier, num_threads);
    
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
  thread_data_t* tds = (thread_data_t*) malloc(num_threads * sizeof(thread_data_t));

  long t;
  for(t = 0; t < num_threads; t++)
    {
      tds[t].id = t;
      tds[t].ht = hashtable;
      //printf("In main: creating thread %ld\n", t);
      rc = pthread_create(&threads[t], &attr, test, tds + t);
      if (rc)
	{
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
  duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
    
  for(t = 0; t < num_threads; t++) 
    {
      rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }

  free(tds);
    
  volatile ticks putting_suc_total = 1;
  volatile ticks putting_fal_total = 1;
  volatile ticks getting_suc_total = 1;
  volatile ticks getting_fal_total = 1;
  volatile ticks removing_suc_total = 1;
  volatile ticks removing_fal_total = 1;
  volatile uint64_t putting_count_total = 1;
  volatile uint64_t putting_count_total_succ = 2;
  volatile uint64_t getting_count_total = 1;
  volatile uint64_t getting_count_total_succ = 2;
  volatile uint64_t removing_count_total = 1;
  volatile uint64_t removing_count_total_succ = 2;
    
  for(t=0; t < num_threads; t++) 
    {
      putting_suc_total += putting_succ[t];
      putting_fal_total += putting_fail[t];
      getting_suc_total += getting_succ[t];
      getting_fal_total += getting_fail[t];
      removing_suc_total += removing_succ[t];
      removing_fal_total += removing_fail[t];
      putting_count_total += putting_count[t];
      putting_count_total_succ += putting_count_succ[t];
      getting_count_total += getting_count[t];
      getting_count_total_succ += getting_count_succ[t];
      removing_count_total += removing_count[t];
      removing_count_total_succ += removing_count_succ[t];
    }

  if(putting_count_total == 0) 
    {
      putting_suc_total = 0;
      putting_fal_total = 0;
      putting_count_total = 1;
      putting_count_total_succ = 2;
    }
    
  if(getting_count_total == 0) 
    {
      getting_suc_total = 0;
      getting_fal_total = 0;
      getting_count_total = 1;
      getting_count_total_succ = 2;
    }
    
  if(removing_count_total == 0) 
    {
      removing_suc_total = 0;
      removing_fal_total = 0;
      removing_count_total = 1;
      removing_count_total_succ = 2;
    }
    
#if !defined(COMPUTE_THROUGHPUT)
#  if defined(DEBUG)
  printf("#thread get_suc get_fal put_suc put_fal rem_suc rem_fal\n"); fflush(stdout);
#  endif
  printf("%d\t%lu\t%lu\t%lu\t%lu\t%lu\t%lu\n",
	 num_threads,
	 getting_suc_total / getting_count_total_succ,
	 getting_fal_total / (getting_count_total - getting_count_total_succ),
	 putting_suc_total / putting_count_total_succ,
	 putting_fal_total / (putting_count_total - putting_count_total_succ),
	 removing_suc_total / removing_count_total_succ,
	 removing_fal_total / (removing_count_total - removing_count_total_succ)
	);
#endif

    
#define LLU long long unsigned int

  int pr = (int) (putting_count_total_succ - removing_count_total_succ);
  int size_after = ht_size(*hashtable);
#if defined(DEBUG)
  printf("puts - rems  : %d\n", pr);
#endif
  assert(size_after == (initial + pr));

#if defined(DEBUG)
  printf("    : %-10s | %-10s | %-11s | %s\n", "total", "success", "succ %", "total %");
  uint64_t total = putting_count_total + getting_count_total + removing_count_total;
  double putting_perc = 100.0 * (1 - ((double)(total - putting_count_total) / total));
  double getting_perc = 100.0 * (1 - ((double)(total - getting_count_total) / total));
  double removing_perc = 100.0 * (1 - ((double)(total - removing_count_total) / total));
  printf("puts: %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) putting_count_total, 
	 (LLU) putting_count_total_succ,
	 (1 - (double) (putting_count_total - putting_count_total_succ) / putting_count_total) * 100,
	 putting_perc);
  printf("gets: %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) getting_count_total, 
	 (LLU) getting_count_total_succ,
	 (1 - (double) (getting_count_total - getting_count_total_succ) / getting_count_total) * 100,
	 getting_perc);
  printf("rems: %-10llu | %-10llu | %10.1f%% | %.1f%%\n", (LLU) removing_count_total, 
	 (LLU) removing_count_total_succ,
	 (1 - (double) (removing_count_total - removing_count_total_succ) / removing_count_total) * 100,
	 removing_perc);
#endif
  float throughput = (putting_count_total + getting_count_total + removing_count_total) * 1000.0 / duration;
  printf("#txs %d\t(%f\n", num_threads, throughput);
    
  ht_destroy(hashtable);
    
  /* Last thing that main() should do */
  //printf("Main: program completed. Exiting.\n");
  pthread_exit(NULL);
    
  return 0;
}
