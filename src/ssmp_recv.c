#include "ssmp.h"

extern volatile ssmp_msg_t **ssmp_recv_buf;
extern volatile ssmp_msg_t **ssmp_send_buf;
extern ssmp_chunk_t **ssmp_chunk_buf;
extern int ssmp_num_ues_;
extern int ssmp_id_;
extern int last_recv_from;
extern ssmp_barrier_t *ssmp_barrier;

/* ------------------------------------------------------------------------------- */
/* receiving functions : default is blocking */
/* ------------------------------------------------------------------------------- */

inline 
void ssmp_recv_from(uint32_t from, volatile ssmp_msg_t *msg) 
{
  volatile ssmp_msg_t* tmpm = ssmp_recv_buf[from];

#ifdef USE_ATOMIC
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_MESSG, BUF_LOCKD)) {
    wait_cycles(WAIT_TIME);
  }
#else
  PREFETCHW(tmpm);
  int32_t wted = 0;

  while(tmpm->state != BUF_MESSG) 
    {
      _mm_pause_rep(wted++);
      asm("");
      PREFETCHW(tmpm);
    }
#endif
  
  memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
  tmpm->state = BUF_EMPTY;
}

inline void 
ssmp_recv_color(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg)
{
  uint32_t from;
  uint32_t num_ues = cbuf->num_ues;
  volatile uint32_t** cbuf_state = cbuf->buf_state;

  while(1)
    {
      for (from = 0; from < num_ues; from++) 
	{
	  if (
#ifdef USE_ATOMIC
	      __sync_bool_compare_and_swap(cbuf_state[from], BUF_MESSG, BUF_LOCKD)
#else
	      *cbuf_state[from] == BUF_MESSG
#endif
	      )
	    {
	      volatile ssmp_msg_t* tmpm = cbuf->buf[from];
	      memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
	      msg->sender = cbuf->from[from];

	      tmpm->state = BUF_EMPTY;
	      return;
	    }
	}
    }
}


static uint32_t start_recv_from = 0; /* keeping from which core to start the recv from next */

inline void
ssmp_recv_color_start(ssmp_color_buf_t *cbuf, ssmp_msg_t *msg)
{
  uint32_t num_ues = cbuf->num_ues;
  volatile uint32_t** cbuf_state = cbuf->buf_state;
  volatile ssmp_msg_t** buf = cbuf->buf;

  while(1) {
    for (; start_recv_from < num_ues; start_recv_from++)
      {

#ifdef USE_ATOMIC
	if(__sync_bool_compare_and_swap(cbuf_state[start_recv_from], BUF_MESSG, BUF_LOCKD))
#else
	PREFETCHW(buf[start_recv_from]);
	if (*cbuf_state[start_recv_from] == BUF_MESSG)
#endif
	  {
	    volatile ssmp_msg_t* tmpm = cbuf->buf[start_recv_from];
	    memcpy((void*) msg, (const void*) tmpm, SSMP_CACHE_LINE_SIZE);
	    msg->sender = cbuf->from[start_recv_from];

	    tmpm->state = BUF_EMPTY;

	    if (++start_recv_from == num_ues)
	      {
		start_recv_from = 0;
	      }
	    PREFETCHW(ssmp_send_buf[msg->sender]);
	    PREFETCHW(buf[start_recv_from]);

	    return;
	  }
      }
    start_recv_from = 0;
  }
}

inline 
void ssmp_recv_from_big(int from, void *data, size_t length) 
{
  int last_chunk = length % SSMP_CHUNK_SIZE;
  int num_chunks = length / SSMP_CHUNK_SIZE;

  while(num_chunks--) {

    while(!ssmp_chunk_buf[from]->state);

    memcpy(data, ssmp_chunk_buf[from], SSMP_CHUNK_SIZE);
    data = ((char *) data) + SSMP_CHUNK_SIZE;

    ssmp_chunk_buf[from]->state = 0;
  }

  if (!last_chunk) {
    return;
  }

  while(!ssmp_chunk_buf[from]->state);

  memcpy(data, ssmp_chunk_buf[from], last_chunk);

  ssmp_chunk_buf[from]->state = 0;

  PD("recved from %d\n", from);
}

