#ifndef _SSMP_RECV_H_
#define _SSMP_RECV_H_

#include "ssmp.h"


__attribute__ ((always_inline)) void ssmp_recv_from_inline(unsigned int from, ssmp_msg_t *msg) {
  volatile ssmp_msg_t *tmpm = ssmp_recv_buf[from];
  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_MESSG, BUF_LOCKD)) {
    wait_cycles(WAIT_TIME);
  }

  msg->w0 = tmpm->w0;				
  msg->w1 = tmpm->w1;				
  msg->w2 = tmpm->w2;				
  msg->w3 = tmpm->w3;				
  msg->w4 = tmpm->w4;				
  msg->w5 = tmpm->w5;				

  tmpm->state = BUF_EMPTY;

}



#endif /* _SSMP_RECV_H_ */
