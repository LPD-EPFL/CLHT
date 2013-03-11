#ifndef _SSMP_SEND_H_
#define _SSMP_SEND_H_

#include "ssmp.h"

__attribute__ ((always_inline)) void ssmp_send_inline(unsigned int to, ssmp_msg_t *msg) {
  volatile ssmp_msg_t * tmpm = ssmp_send_buf[to];

  while (!__sync_bool_compare_and_swap(&tmpm->state, BUF_EMPTY, BUF_LOCKD)) {
    wait_cycles(WAIT_TIME);
  }
  
  tmpm->w0 = msg->w0;	
  tmpm->w1 = msg->w1;	
  tmpm->w2 = msg->w2;
  tmpm->w3 = msg->w3;
  tmpm->w4 = msg->w4;
  tmpm->w5 = msg->w5;
    
  tmpm->state = BUF_MESSG;
}



#endif /* _SSMP_SEND_H_ */
