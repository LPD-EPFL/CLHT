#include "ssmp.h"

//#define SSMP_DEBUG


uint8_t id_to_core[] =
  {
    0, 1, 2, 3, 4, 5,
    6, 7, 8, 9, 10, 11, 
    12, 13, 14, 15, 16, 17, 
    18, 19, 20, 21, 22, 23, 
    24, 25, 26, 27, 28, 29, 
    30, 31, 32, 33, 34, 35, 
    36, 37, 38, 39, 40, 41, 
    42, 43, 44, 45, 46, 47 
  };


const uint8_t node_to_node_hops[8][8] =
  {
  /* 0  1  2  3  4  5  6  7           */
    {0, 1, 2, 3, 2, 3, 2, 3},	/* 0 */
    {1, 0, 3, 2, 3, 2, 3, 2},	/* 1 */
    {2, 3, 0, 1, 2, 3, 2, 3},	/* 2 */
    {3, 2, 1, 0, 3, 2, 3, 2},	/* 3 */
    {2, 3, 2, 3, 0, 1, 2, 3},	/* 4 */
    {3, 2, 3, 2, 1, 0, 3, 2},	/* 5 */
    {2, 3, 2, 3, 2, 3, 0, 1},	/* 6 */
    {3, 2, 3, 2, 3, 2, 1, 0},	/* 7 */
  };


/* ------------------------------------------------------------------------------- */
/* library variables */
/* ------------------------------------------------------------------------------- */

static ssmp_msg_t *ssmp_mem;
volatile ssmp_msg_t **ssmp_recv_buf;
volatile ssmp_msg_t **ssmp_send_buf;
static ssmp_chunk_t *ssmp_chunk_mem;
ssmp_chunk_t **ssmp_chunk_buf;
int ssmp_num_ues_;
int ssmp_id_;
int last_recv_from;
ssmp_barrier_t *ssmp_barrier;
int *ues_initialized;
static uint32_t ssmp_my_core;


/* ------------------------------------------------------------------------------- */
/* init / term the MP system */
/* ------------------------------------------------------------------------------- */

void ssmp_init(int num_procs)
{
  //create the shared space which will be managed by the allocator
  unsigned int sizeb, sizeckp, sizeui, sizecnk, size;;

  sizeb = SSMP_NUM_BARRIERS * sizeof(ssmp_barrier_t);
  sizeckp = SSMP_NUM_BARRIERS * num_procs * sizeof(ssmp_chk_t);
  sizeui = num_procs * sizeof(int);
  sizecnk = num_procs * sizeof(ssmp_chunk_t);
  size = sizeb + sizeckp + sizeui + sizecnk;

  char keyF[100];
  sprintf(keyF,"/ssmp_mem");

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd<0)
    {
      if (errno != EEXIST)
	{
	  perror("In shm_open");
	  exit(1);
	}

      //this time it is ok if it already exists
      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0)
	{
	  perror("In shm_open");
	  exit(1);
	}
    }
  else
    {
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

  ssmp_mem = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (ssmp_mem == NULL)
    {
      perror("ssmp_mem = NULL\n");
      exit(134);
    }

  long long unsigned int mem_just_int = (long long unsigned int) ssmp_mem;
  ssmp_barrier = (ssmp_barrier_t *) (mem_just_int);
  ssmp_chk_t *chks = (ssmp_chk_t *) (mem_just_int + sizeb);
  ues_initialized = (int *) (mem_just_int + sizeb + sizeckp);
  ssmp_chunk_mem = (ssmp_chunk_t *) (mem_just_int + sizeb + sizeckp + sizeui);

  int ue;
  for (ue = 0; ue < SSMP_NUM_BARRIERS * num_procs; ue++) {
    chks[ue] = 0;
  }

  int bar;
  for (bar = 0; bar < SSMP_NUM_BARRIERS; bar++) {
    ssmp_barrier[bar].checkpoints = (chks + (bar * num_procs));
    ssmp_barrier_init(bar, 0xFFFFFFFFFFFFFFFF, NULL);
  }
  ssmp_barrier_init(1, 0xFFFFFFFFFFFFFFFF, color_app);

}

void ssmp_mem_init(int id, int num_ues) {
  ssmp_id_ = id;
  ssmp_num_ues_ = num_ues;
  last_recv_from = (id + 1) % num_ues;

  ssmp_recv_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_send_buf = (volatile ssmp_msg_t **) malloc(num_ues * sizeof(ssmp_msg_t *));
  ssmp_chunk_buf = (ssmp_chunk_t **) malloc(num_ues * sizeof(ssmp_chunk_t *));
  if (ssmp_recv_buf == NULL || ssmp_send_buf == NULL || ssmp_chunk_buf == NULL) {
    perror("malloc@ ssmp_mem_init\n");
    exit(-1);
  }

  char keyF[100];
  unsigned int size = (num_ues - 1) * sizeof(ssmp_msg_t);
  unsigned int core;
  sprintf(keyF, "/ssmp_core%03d", id);
  
  if (num_ues == 1) return;

  int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
  if (ssmpfd < 0) {
    if (errno != EEXIST) {
      perror("In shm_open");
      exit(1);
    }

    ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd<0) {
      perror("In shm_open");
      exit(1);
    }
  }
  else {
    if (ftruncate(ssmpfd, size) < 0) {
      perror("ftruncate failed\n");
      exit(1);
    }
  }

  ssmp_msg_t * tmp = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
  if (tmp == NULL)
    {
      perror("tmp = NULL\n");
      exit(134);
    }

  for (core = 0; core < num_ues; core++) {
    if (id == core) {
      continue;
    }

    ssmp_recv_buf[core] = tmp + ((core > id) ? (core - 1) : core);
    ssmp_recv_buf[core]->state = 0;
  
    ssmp_chunk_buf[core] = ssmp_chunk_mem + core;
    ssmp_chunk_buf[core]->state = 0;
  }

  /*********************************************************************************
    initialized own buffer
    ********************************************************************************
    */

  ssmp_barrier_wait(0);
  
  for (core = 0; core < num_ues; core++) {
    if (core == id) {
      continue;
    }

    sprintf(keyF, "/ssmp_core%03d", core);
  
    int ssmpfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
    if (ssmpfd < 0) {
      if (errno != EEXIST) {
	perror("In shm_open");
	exit(1);
      }

      ssmpfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (ssmpfd<0) {
	perror("In shm_open");
	exit(1);
      }
    }
    else {
      if (ftruncate(ssmpfd, size) < 0) {
	perror("ftruncate failed\n");
	exit(1);
      }
    }

    ssmp_msg_t * tmp = (ssmp_msg_t *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, ssmpfd, 0);
    if (tmp == NULL)
      {
	perror("tmp = NULL\n");
	exit(134);
      }

    ssmp_send_buf[core] = tmp + ((core < id) ? (id - 1) : id);
  }

  ues_initialized[id] = 1;

  //  SP("waiting for all to be initialized!");
  int ue;
  for (ue = 0; ue < num_ues; ue++) {
    while(!ues_initialized[ue]) {
      _mm_mfence();
    };
  }
  //  SP("\t\t\tall initialized!");
}

void ssmp_term() {
  if (ssmp_id_ == 0)
    {
      shm_unlink("/ssmp_mem");
    }
  char keyF[100];
  sprintf(keyF, "/ssmp_core%03d", ssmp_id_);
  shm_unlink(keyF);
}


/* ------------------------------------------------------------------------------- */
/* color-based initialization fucntions */
/* ------------------------------------------------------------------------------- */

void ssmp_color_buf_init(ssmp_color_buf_t *cbuf, int (*color)(int)) {
  if (cbuf == NULL) {
    cbuf = (ssmp_color_buf_t *) malloc(sizeof(ssmp_color_buf_t));
    if (cbuf == NULL) {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
  }

  uint32_t *participants = (uint32_t *) malloc(ssmp_num_ues_ * sizeof(uint32_t));
  if (participants == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }

  uint32_t ue, num_ues = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
    if (ue == ssmp_id_) {
      participants[ue] = 0;
      continue;
    }

    participants[ue] = color(ue);
    if (participants[ue]) {
      num_ues++;
    }
  }

  cbuf->num_ues = num_ues;

  uint32_t size_buf = num_ues * sizeof(ssmp_msg_t *);
  uint32_t size_pad = 0;

  if (size_buf % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_buf % SSMP_CACHE_LINE_SIZE)) / sizeof(ssmp_msg_t *);
      size_buf += size_pad * sizeof(ssmp_msg_t *);
    }

  cbuf->buf = (volatile ssmp_msg_t **) malloc(size_buf);
  if (cbuf->buf == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }

  cbuf->buf_state = (volatile unsigned int **) malloc(size_buf);
  if (cbuf->buf_state == NULL) {
    perror("malloc @ ssmp_color_buf_init");
    exit(-1);
  }
  
  uint32_t size_from = num_ues * sizeof(uint32_t);

  if (size_from % SSMP_CACHE_LINE_SIZE)
    {
      size_pad = (SSMP_CACHE_LINE_SIZE - (size_from % SSMP_CACHE_LINE_SIZE)) / sizeof(uint32_t);
      size_from += size_pad * sizeof(uint32_t);
    }


  cbuf->from = (uint32_t *) malloc(size_from);
  if (cbuf->from == NULL)
    {
      perror("malloc @ ssmp_color_buf_init");
      exit(-1);
    }
    
  uint32_t buf_num = 0;
  for (ue = 0; ue < ssmp_num_ues_; ue++)
    {
      if (participants[ue])
	{
	  cbuf->buf[buf_num] = ssmp_recv_buf[ue];
	  cbuf->buf_state[buf_num] = &ssmp_recv_buf[ue]->state;
	  cbuf->from[buf_num] = ue;
	  buf_num++;
	}
    }

  free(participants);
}

inline void ssmp_color_buf_free(ssmp_color_buf_t *cbuf) {
  free(cbuf->buf);
}



/* ------------------------------------------------------------------------------- */
/* barrier functions */
/* ------------------------------------------------------------------------------- */

int color_app(int id) {
  return ((id % 2) ? 1 : 0);
}

inline ssmp_barrier_t * ssmp_get_barrier(int barrier_num) {
  if (barrier_num < SSMP_NUM_BARRIERS) {
    return (ssmp_barrier + barrier_num);
  }
  else {
    return NULL;
  }
}

int color(int id) {
  return (10*id);
}

inline void ssmp_barrier_init(int barrier_num, long long int participants, int (*color)(int)) {
  if (barrier_num >= SSMP_NUM_BARRIERS) {
    return;
  }

  ssmp_barrier[barrier_num].participants = 0xFFFFFFFFFFFFFFFF;
  ssmp_barrier[barrier_num].color = color;
  int ue;
  for (ue = 0; ue < ssmp_num_ues_; ue++) {
    ssmp_barrier[barrier_num].checkpoints[ue] = 0;
  }
  ssmp_barrier[barrier_num].version = 0;
}

inline void ssmp_barrier_wait(int barrier_num) {
  if (barrier_num >= SSMP_NUM_BARRIERS) {
    return;
  }

  ssmp_barrier_t *b = &ssmp_barrier[barrier_num];
  unsigned int version = b->version;

  PD(">>Waiting barrier %d\t(v: %d)", barrier_num, version);

  int (*col)(int);
  col= b->color;

  unsigned int *participants = (unsigned int *) malloc(ssmp_num_ues_ * sizeof(unsigned int));
  if (participants == NULL) {
    perror("malloc @ ssmp_barrier_wait");
    exit(-1);
  }
  long long unsigned int bpar = b->participants;
  int from;
  for (from = 0; from < ssmp_num_ues_; from++) {
    /* if there is a color function it has priority */
    if (col != NULL) {
      participants[from] = col(from);
    }
    else {
      participants[from] = (unsigned int) (bpar & 0x0000000000000001);
      bpar >>= 1;
    }
  }
  
  if (participants[ssmp_id_] == 0) {
    PD("<<Cleared barrier %d\t(v: %d)\t[not participant!]", barrier_num, version);
    free(participants);
    return;
  }

  //round 1;
  b->checkpoints[ssmp_id_] = version + 1;
  
  int done = 0;
  while(!done) {
    /* _mm_mfence(); */
    done = 1;
    unsigned int ue;
    for (ue = 0; ue < ssmp_num_ues_; ue++) {
      if (participants[ue] == 0) {
	continue;
      }
      
      /* _mm_mfence(); */
      if ((b->checkpoints[ue] != (version + 1)) && (b->checkpoints[ue] != (version + 2))) {
	done = 0;
	break;
      }
    }
  }

  //round 2;
  b->checkpoints[ssmp_id_] = version + 2;

  done = 0;
  while(!done) {
    done = 1;
    int ue;
    for (ue = 0; ue < ssmp_num_ues_; ue++) {
      if (participants[ue] == 0) {
	continue;
      }
      
      if (b->version > version) {
	PD("<<Cleared barrier %d\t(v: %d)\t[someone was faster]", barrier_num, version);
	b->checkpoints[ssmp_id_] = 0;
	free(participants);
	return;
      }

      if (b->checkpoints[ue] == (version + 1)) {
	done = 0;
	break;
      }
    }
  }

  b->checkpoints[ssmp_id_] = 0;
  if (b->version <= version) {
    b->version = version + 3;
  }

  free(participants);
  PD("<<Cleared barrier %d (v: %d)", barrier_num, version);
}


/* ------------------------------------------------------------------------------- */
/* help funcitons */
/* ------------------------------------------------------------------------------- */

inline void 
wait_cycles(uint64_t cycles)
{
  if (cycles < 256)
    {
      cycles /= 6;
      while (cycles--)
	{
	  _mm_pause();
	}
    }
  else
    {
      ticks _start_ticks = getticks();
      ticks _end_ticks = _start_ticks + cycles - 130;
      while (getticks() < _end_ticks);
    }
}

inline void
_mm_pause_rep(uint32_t num_reps)
{
  while (num_reps--)
    {
      _mm_pause();
    }
}

inline uint32_t 
get_num_hops(uint32_t core1, uint32_t core2)
{
  uint32_t hops = node_to_node_hops[core1 / 6][core2 / 6];
  //  PRINT("%2d is %d hop", core2, hops);
  return hops;
}

inline uint32_t
get_cpu()
{
  return ssmp_my_core;
}

inline int ssmp_id() 
{
  return ssmp_id_;
}

inline int ssmp_num_ues() 
{
  return ssmp_num_ues_;
}

