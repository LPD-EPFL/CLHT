
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>

#ifdef __sparc__
#  include "include/common.h"
#  include "include/dht.h"
#else
#  include "common.h"
#  include "dht.h"
#endif

#include <ssmp.h>
#include "main_mp.h"
#include "mcore_malloc.h"

#define DEBUG_

/*
 * Useful macros to work with transactions. Note that, to use nested
 * transactions, one should check the environment returned by
 * stm_get_env() and only call sigsetjmp() if it is not null.
 */

#define REF_SPEED_GHZ                 2.1
#define RO                              1
#define RW                              0

#define DEFAULT_USE_LOCKS               1
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0
#define DEFAULT_DSL_PER_CORE            3

#define ONCE					\
  if (ID == 1)


double duration__ = 0;

static inline unsigned long* seed_rand() {
  unsigned long* seeds;
  seeds = (unsigned long*) malloc(3 * sizeof(unsigned long));
  seeds[0] = getticks() % 123456789;
  seeds[1] = getticks() % 362436069;
  seeds[2] = getticks() % 521288629;
  return seeds;
}


/* ################################################################### *
 * GLOBALS
 * ################################################################### */

int capacity;
int num_elements;
int duration;
float filling_rate;
float update_rate;
float get_rate;
uint32_t payload_size;
uint32_t payload_size_cl;


int seed = 0;
unsigned long* seeds;
int rand_max, rand_min;

static volatile int stop;

int dsl_seq[80];
uint8_t* bucket_to_dsl;
uint8_t ID, num_dsl, num_app;
int num_procs;

int write_threads = DEFAULT_WRITE_THREADS;
int disjoint = DEFAULT_DISJOINT;
int dsl_per_core = DEFAULT_DSL_PER_CORE;

volatile int work = 1;

void
alarm_handler(int sig)
{
  work = 0;
}

static inline void
touch_buffer(volatile char* buf, uint32_t size_cl)
{
  uint32_t i;
  for (i = 0; i < size_cl; i += CACHE_LINE_SIZE)
    {
      buf[i] = i;
    }
  _mm_sfence();
}

ssmp_msg_t* msg;
ssht_rpc_t* rpc;

/* ################################################################### *
 * DISTRIBUTED HASHTABLE
 * ################################################################### */

/* help functions */
int color_dsl(int id);
int nth_dsl(int id);
int nth_app(int id);
int color_app1(int id);
int color_all(int id);

/* dht funcitonality */
uint64_t
_put( uint64_t key, void *value, int payload_size, int dsl )
{
  /* PRINT("putting: %-5u , val: %p", key, value); */
  rpc->value = value;
  rpc->key = key;
  rpc->op = SSHT_PUT;

  ssmp_send(dsl, msg);
  _mm_pause();
  ssmp_recv_from(dsl, msg);
    
  return rpc->resp;
}

void*
_get( uint64_t key, void* buffer, int dsl)
{
  /* PRINT("getting: %-5u , val: %p", key, buffer); */
  rpc->value = buffer;
  rpc->key = key;
  rpc->op = SSHT_GET;

  ssmp_send(dsl, msg);
  _mm_pause();
  ssmp_recv_from(dsl, msg);
  return rpc->value;
}

void*
_remove( uint64_t key, int dsl)
{
  /* PRINT("removing: %-5u , val: %p", key); */
  rpc->key = key;
  rpc->op = SSHT_REM;
  ssmp_send(dsl, msg);
  _mm_pause();
  ssmp_recv_from(dsl, msg);
    
  return rpc->value;
}

size_t
_size(uint32_t num_dsl)
{
  size_t size = 0;
  rpc->op = SSHT_SIZ;
  uint32_t s;
  for (s = 0; s < num_dsl; s++)
    {
      ssmp_send(dsl_seq[s], msg);
      ssmp_recv_from(dsl_seq[s], msg);
      size += rpc->resp;
    }

  return size;
}

void
_print(uint32_t num_dsl)
{
  rpc->op = SSHT_PRN;
  uint32_t s;
  for (s = 0; s < num_dsl; s++)
    {
      ssmp_send(dsl_seq[s], msg);
      ssmp_recv_from(dsl_seq[s], msg);
    }
}



static inline int
get_dsl(int bin, int num_dsl)
{
  return bucket_to_dsl[bin];
    //dsl_seq[bin / num_dsl];
}

void
dht_app()
{
  seeds = seed_rand();    

  int bin;
  uint64_t key;
  void* value = NULL, * local, * remote;
  int c = 0;
  uint8_t scale_update = (update_rate * 128);
  uint32_t bucket_per_dsl;
  if (capacity >= num_dsl)
    {
      bucket_per_dsl = capacity / num_dsl;
    }
  else
    {
      bucket_per_dsl = 1;
    }

  int dsl;
    
  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);
  rpc = (ssht_rpc_t *) msg;
    
  local = MCORE_shmalloc(payload_size);
  remote = MCORE_shmalloc(payload_size);
  
    
  ssmp_barrier_wait(4);

  /* populate the table */
  ONCE
    {
#if defined(DEBUG)
      PRINT("inserting elements");
#endif	/* debug */
      int i;
      for(i = 0; i < num_elements * filling_rate; i++) 
	{
	  key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
	  bin = key % capacity;
	  dsl = get_dsl(bin, bucket_per_dsl);
	  value = MCORE_shmalloc(payload_size);
	  _mm_mfence();
	  if (!_put(key, value, payload_size, dsl))
	    {
	      i--;
	    }
	  _mm_mfence();
	}

#if defined(DEBUG)
      PRINT("inserted..");
      PRINT("size bf : %lu", _size(num_dsl)); 
#endif	/* debug */
    }
  ssmp_barrier_wait(1);

  signal (SIGALRM, alarm_handler);
  alarm(duration / 1000);

  ssmp_barrier_wait(1);

  uint64_t my_total_count = 0, my_succ_count = 0;
    
  uint8_t succ = true;
  uint8_t putting = true;
  uint8_t update = false;

  double __start = wtime();

  while (work)
    {
      key = (my_random(&(seeds[0]), &(seeds[1]), &(seeds[2])) % rand_max) + rand_min;
      bin = key % capacity;
      dsl = get_dsl(bin, bucket_per_dsl);
        
      if(succ) 
	{
	  c = (uint8_t)(my_random(&(seeds[0]),&(seeds[1]),&(seeds[2])) & 0x7f);
	  update = (c < scale_update);
	}

      if (update && putting && succ)
      	{
	  value = MCORE_shmalloc(payload_size);
      	  /* memset(value, 'O', payload_size); */
	  /* touch_buffer(value, payload_size_cl); */
      	}

      succ = 0;        

      if(update) 
	{
	  if(putting) 
	    {
	      if(_put( key, value, payload_size, dsl))
		{
		  succ = 1;
		  putting = false;
		}
	    } 
	  else 
	    {
	      void* removed = _remove(key, dsl);
	      if(removed != NULL) 
		{
		  succ = 1;
		  MCORE_shfree(removed);
		  putting = true;
		}
	    }
            
	} 
      else
	{ 
	  value = _get( key, remote, dsl );
	  if(value != NULL) 
	    {
	      succ = 1;
	      memcpy(local, value, payload_size);
	    }
	}

      my_succ_count += succ;
      my_total_count++;
    }
  double __end = wtime();
  duration__ =  __end - __start;

  ssmp_barrier_wait(1);

#if defined(DEBUG)
#define LLU long long unsigned int
  PRINT("total: %llu / succ: %llu ", (LLU) my_total_count, (LLU) my_succ_count);
  if (ID == 1)
    {
      PRINT("size af : %lu", _size(num_dsl)); 
      /* _print(num_dsl); */
    }
#endif	/* debug */

  ssmp_barrier_wait(1);
  if (ssmp_id() == 1)
    {
      uint32_t i;
      for (i = 0; i < num_dsl; i++)
        {
	  rpc->op = SSHT_EXT;
	  ssmp_send(dsl_seq[i], msg);
	  ssmp_recv_from(dsl_seq[i], msg);
        }
    }
    
  ssmp_barrier_wait(1);
    
  double throughput = (my_total_count) / duration__;
    
  _mm_mfence();

  /* PRINT("Completed in %10f secs | throughput: %f", duration__, throughput); */
  memcpy(msg, &throughput, sizeof(double));
  ssmp_send(0, msg);
  _mm_mfence();
}

void
dht_dsl()
{
  uint32_t capacity_mine = 0;
      uint32_t b;
      for (b = 0; b < capacity; b++)
	{
	  if (bucket_to_dsl[b] == dsl_seq[ID])
	    {
	      capacity_mine++;
	    }
	}
      /* PRINT("DSL -- handling %4d buckets", capacity_mine); */
  

    
  hashtable_t *hashtable = ht_create(capacity_mine);
    
  ssmp_color_buf_t *cbuf = NULL;
  cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
  assert(cbuf != NULL);
  ssmp_color_buf_init(cbuf, color_app1);
    
  msg = (ssmp_msg_t *) malloc(sizeof(ssmp_msg_t));
  assert(msg != NULL);
  rpc = (ssht_rpc_t *) msg;
  
  int done = 0;
    
  ssmp_barrier_wait(4);
    
  while(done == 0)
    {
      ssmp_recv_color_start(cbuf, msg);

      ssht_addr_t key = rpc->key;
      uint32_t bin = ht_hash(hashtable, key);
      switch (rpc->op)
        {
	case SSHT_PUT:
	  {
	    /* PRINT("from %-3d : PUT for %-5d : %p", msg->sender, key, rpc->value); */
	    rpc->resp = ht_put( hashtable, key, rpc->value, bin);
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	case SSHT_GET:
	  {
	    void* data = ht_get(hashtable, key, bin);
	    if (data != NULL)
	      {
		memcpy(rpc->value, data, payload_size);
	      }
	    else
	      {
		rpc->value = NULL;
	      }
	    /* PRINT("from %-3d : GET for %-5d : %p", msg->sender, key, rpc->value); */
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	case SSHT_REM:
	  {
	    rpc->value = ht_remove(hashtable, key, bin);
	    /* PRINT("from %-3d : REM for %-5d : %p", msg->sender, key, rpc->value); */
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	case SSHT_SIZ:
	  {
	    rpc->resp = ht_size(hashtable, capacity_mine);
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	case SSHT_PRN:
	  {
	    ht_print(hashtable, capacity_mine);
	    ssmp_send(msg->sender, msg);
	    break;
	  }
	default:
	  done = 1;
	  ssmp_send(msg->sender, msg);
	  ht_destroy( hashtable );
	  break;
        }
    }


}


int
main(int argc, char **argv)
{

  /* before doing any allocations */
#if defined(__tile__)
  if (tmc_cpus_get_my_affinity(&cpus) != 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_get_my_affinity()'.");
    }
#endif

  if (argc == 10 || argc == 9) 
    {
      capacity = atoi( argv[1] );
      num_procs = atoi( argv[2] );
      num_elements = atoi( argv[3] );
      filling_rate = atof( argv[4] );
      payload_size = atoi( argv[5] );
      duration = atoi( argv[6] );
      update_rate = atof( argv[7] );
      get_rate = atof( argv[8] );
      dsl_per_core = DEFAULT_DSL_PER_CORE;
    } 
  else 
    {
      printf("ERROR; usage ./main table_capacity num_procs num_elements filling_rate payload_size duration put_rate get_rate remove_rate dsl_per_cores\n");
      exit(-1);
    }
    
  if (argc == 10)
    {
      dsl_per_core = atoi( argv[9] );
    }

  if (num_procs == 1)
    {
      MCORE_shmalloc_term();
      printf("1 0\n");
      return 0;
    }

  payload_size_cl = payload_size / CACHE_LINE_SIZE;

  rand_min = 1;
  rand_max = num_elements;
    
  //capacity = pow2roundup(capacity);
    
  if (seed == 0)
    srand((int)time(NULL));
  else
    srand(seed);
    
  int dsl_seq_idx = 0;
  long t;
  for (t = 0; t < num_procs; t++)
    {
      if (color_dsl(t))
        {
	  num_dsl++;
	  dsl_seq[dsl_seq_idx++] = t;
        }
      else
        {
	  num_app++;
        }
    }
    
#if defined(DEBUG)
  printf("dsl: %2d | app: %d\n", num_dsl, num_app);
#endif
    
  bucket_to_dsl = calloc(capacity, sizeof(uint8_t));
  assert(bucket_to_dsl != NULL);

  if (capacity % num_dsl != 0)
    {
#if defined(DEBUG)
      printf("*** table capacity (%d) is not dividable by number of DSL (%d)\n", capacity, num_dsl);
#endif	/* debug */
      /* capacity++; */
      uint32_t left = capacity % num_dsl;
      uint32_t cap_mod = capacity - left;
      uint32_t cap_per_dsl = cap_mod / num_dsl;
      uint32_t b;
      for (b = 0; b < cap_mod; b++)
	{
	  bucket_to_dsl[b] = dsl_seq[b / cap_per_dsl];
	}
      for (; b < cap_mod + left; b++)
	{
	  bucket_to_dsl[b] = dsl_seq[b % num_dsl];
	}
    }
  else
    {
      uint32_t cap_per_dsl = capacity / num_dsl;
      uint32_t b;
      for (b = 0; b < capacity; b++)
	{
	  bucket_to_dsl[b] = dsl_seq[b / cap_per_dsl];
	}
    }

    
  ssmp_init(num_procs);
    
  ssmp_barrier_init(2, 0, color_dsl);
  ssmp_barrier_init(1, 0, color_app1);
  ssmp_barrier_init(0, 0, color_all);
  ssmp_barrier_init(3, 0, color_all);
  ssmp_barrier_init(4, 0, color_all);

  MCORE_shmalloc_init(num_app * MCORE_SIZE);

  int rank;
  for (rank = 1; rank < num_procs; rank++) 
    {
      pid_t child = fork();
      if (child < 0) {
	printf("Failure in fork():\n%s", strerror(errno));
      } else if (child == 0) 
        {
	  goto fork_done;
        }
    }
  rank = 0;

 fork_done:
    
  ID = rank;
    
  set_cpu(id_to_core[ID]);
  ssmp_mem_init(ID, num_procs);
  
  if (color_dsl(ID))
    {
      dht_dsl();
    }
  else
    {
      uint32_t nthapp = nth_app(ID);
      MCORE_shmalloc_offset(nthapp * MCORE_SIZE);
      dht_app();
    }
    

  ssmp_barrier_wait(0);

  double total_throughput = 0;
  if (ssmp_id() == 0)
    {
      int c;
      for (c = 0; c < ssmp_num_ues(); c++)
        {
	  if (color_app1(c))
            {
	      ssmp_recv_from(c, msg);
	      double throughput = *(double*) msg;
	      /* PRINT("received th from %02d : %f", c, throughput); */
	      total_throughput += throughput;
            };
        }
        
      printf("%d\t%f\n", num_procs, total_throughput);
    }

  //printf("core %d, \n", rank);
  ssmp_barrier_wait(3);
  ssmp_term();
  if (ssmp_id() == 0)
    {
      MCORE_shmalloc_term();
    }
  return 0;
}

/* help functions */

int color_dsl(int id)
{
    return (id % dsl_per_core == 0);
}

int nth_dsl(int id)
{
    int i, id_seq = 0;
    for (i = 0; i < ssmp_num_ues(); i++)
    {
        if (i == id)
        {
            return id_seq;
        }
        id_seq++;
    }
    return id_seq;
}

int color_app1(int id)
{
    return !(color_dsl(id));
}

int nth_app(int id)
{
  uint32_t i, s = 0;
  for (i = 0; i < ssmp_num_ues(); i++)
    {
      if (i == id)
	{
	  break;
	}
      else if (color_app1(i))
	{
	  s++;
	}
    }
  return s;
}

int color_all(int id)
{
  return 1;
}
