#ifndef _SSMP_H_
#define _SSMP_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sched.h>
#include <inttypes.h>
//#include <emmintrin.h>
#ifdef PLATFORM_NUMA
#include <numa.h>
#endif /* PLATFORM_NUMA */

#include "utils.h"

/* ------------------------------------------------------------------------------- */
/* settings */
/* ------------------------------------------------------------------------------- */
#define USE_ATOMIC_
#define WAIT_TIME 66

extern uint8_t id_to_core[];
extern const uint8_t node_to_node_hops[8][8];
/* ------------------------------------------------------------------------------- */
/* defines */
/* ------------------------------------------------------------------------------- */
#define SSMP_NUM_BARRIERS 16 /*number of available barriers*/
#define SSMP_CHUNK_SIZE 1020
#define SSMP_CACHE_LINE_SIZE 64

#define BUF_EMPTY 0
#define BUF_MESSG 1
#define BUF_LOCKD 2


//#define PREFETCHW(x) asm volatile("prefetchw %0" :: "m" (*(unsigned long *)x)) /* write */
#define PREFETCH(x) asm volatile("prefetch %0" :: "m" (*(unsigned long *)x)) /* read */
#define PREFETCHNTA(x) asm volatile("prefetchnta %0" :: "m" (*(unsigned long *)x)) /* non-temporal */
#define PREFETCHT0(x) asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x)) /* all levels */
#define PREFETCHT1(x) asm volatile("prefetcht1 %0" :: "m" (*(unsigned long *)x)) /* all but L1 */
#define PREFETCHT2(x) asm volatile("prefetcht2 %0" :: "m" (*(unsigned long *)x)) /* all but L1 & L2 */

#define SP(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#ifdef SSMP_DEBUG
#define PD(args...) printf("[%d] ", ssmp_id_); printf(args); printf("\n"); fflush(stdout)
#else
#define PD(args...) 
#endif

#ifndef ALIGNED
#  if __GNUC__ && !SCC
#    define ALIGNED(N) __attribute__ ((aligned (N)))
#  else
#    define ALIGNED(N)
#  endif
#endif

/* ------------------------------------------------------------------------------- */
/* types */
/* ------------------------------------------------------------------------------- */
typedef int ssmp_chk_t; /*used for the checkpoints*/

/*msg type: contains 15 words of data and 1 word flag*/
typedef struct ALIGNED(64) ssmp_msg 
{
  int w0;
  int w1;
  int w2;
  int w3;
  int w4;
  int w5;
  int w6;
  int w7;
  int f[7];
  union 
  {
    volatile uint32_t state;
    volatile uint32_t sender;
  };
} ssmp_msg_t;

typedef struct 
{
  unsigned char data[SSMP_CHUNK_SIZE];
  int state;
} ssmp_chunk_t;

/*type used for color-based function, i.e. functions that operate on a subset of the cores according to a color function*/
typedef struct ALIGNED(64) ssmp_color_buf_struct
{
  uint64_t num_ues;
  volatile uint32_t** buf_state;
  volatile ssmp_msg_t** buf;
  uint32_t* from;
  /* int32_t pad[8]; */
} ssmp_color_buf_t;


/*barrier type*/
typedef struct 
{
  uint64_t participants;                  /*the participants of a barrier can be given either by this, as bits (0 -> no, 1 ->participate */
  int (*color)(int); /*or as a color function: if the function return 0 -> no participant, 1 -> participant. The color function has priority over the lluint participants*/
  ssmp_chk_t * checkpoints; /*the checkpoints array used for sync*/
  uint32_t version; /*the current version of the barrier, used to make a barrier reusable*/
} ssmp_barrier_t;

volatile extern ssmp_msg_t **ssmp_recv_buf;
volatile extern ssmp_msg_t **ssmp_send_buf;

/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

/* initialize the system: called before forking */
extern void ssmp_init(int num_procs);
/* initilize the memory structures of the system: called after forking by every proc */
extern void ssmp_mem_init(int id, int num_ues);
/* terminate the system */
extern void ssmp_term(void);

/* ------------------------------------------------------------------------------- */
/* sending functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking send length words to to */
/* blocking in the sense that the data are copied to the receiver's buffer */
extern inline void ssmp_send(uint32_t to, volatile ssmp_msg_t *msg);
extern inline void ssmp_send_big(int to, void *data, size_t length);

/* ------------------------------------------------------------------------------- */
/* broadcasting functions */
/* ------------------------------------------------------------------------------- */

/* broadcast length bytes using blocking sends */
extern inline void ssmp_broadcast(ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

/* blocking receive from process from length bytes */
extern inline void ssmp_recv_from(uint32_t from, volatile ssmp_msg_t *msg);
extern inline void ssmp_recv_from_big(int from, void *data, size_t length);

/* blocking receive from any proc. 
   Sender at msg->sender */
extern inline void ssmp_recv(ssmp_msg_t *msg, int length);

/* ------------------------------------------------------------------------------- */
/* color-based recv fucntions */
/* ------------------------------------------------------------------------------- */

/* initialize the color buf data structure to be used with consequent ssmp_recv_color calls. A node is considered a participant if the call to color(ID) returns 1 */
extern void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int));
extern void ssmp_color_buf_free(ssmp_color_buf_t *cbuf);

/* blocking receive from any of the participants according to the color function */
extern inline void ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);
extern inline void ssmp_recv_color_start(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg);

/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */
extern int color_app(int id);

extern inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num);
extern inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int));

extern inline void ssmp_barrier_wait(int barrier_num);


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */
extern inline void wait_cycles(uint64_t cycles);
extern inline void _mm_pause_rep(uint32_t num_reps);
extern inline uint32_t get_num_hops(uint32_t core1, uint32_t core2);

extern inline uint32_t get_cpu();

extern inline int ssmp_id();
extern inline int ssmp_num_ues();

#endif
