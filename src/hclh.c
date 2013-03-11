//note: this algo is taken from "a hierarchical clh queue lock" by luchangco et al.
#include "hclh.h"

uint16_t wait_for_grant_or_cluster_master(volatile qnode *q, uint8_t my_cluster) {
  qnode aux;
  while(1) 
    {
      aux.data=q->data;
      if ((aux.fields.cluster_id==my_cluster) && 
	  (aux.fields.tail_when_spliced==0) && 
	  (aux.fields.successor_must_wait==0)) 
	return 1;
      if (aux.fields.tail_when_spliced==1) 
	return 0;
      if (aux.fields.cluster_id!=my_cluster) 
	return 0;
      PAUSE;
    } 
}

volatile qnode * hclh_acquire(local_queue *lq, global_queue *gq, qnode *my_qnode) {
  //splice my_qnode into local queue
  volatile qnode* my_pred;
  do 
    {
#if defined(OPTERON_OPTIMIZE)
      PREFETCHW(lq);
#endif	/* OPTERON_OPTIMIZE */
      my_pred = *lq;
    }  while (CAS_PTR(lq, my_pred, my_qnode)!=my_pred);

  if (my_pred != NULL) 
    {
      uint16_t i_own_lock = wait_for_grant_or_cluster_master(my_pred, my_qnode->fields.cluster_id);
      if (i_own_lock) 
	{
	  //I have the lock; return qnode just released by previous owner
	  return my_pred;
	}
    }
  //at this point, I'm cluster master. Wait to allow time for other acquireres to show up.
  PAUSE;  PAUSE;

  volatile qnode * local_tail;
  //splice local queue into global queue
  do 
    {
#if defined(OPTERON_OPTIMIZE)
      PREFETCHW(gq);
      PREFETCHW(lq);
#endif	/* OPTERON_OPTIMIZE */
      my_pred = *gq;
      local_tail = *lq;
      PAUSE;
    } while(CAS_PTR(gq, my_pred, local_tail)!=my_pred);

  //inform successor that it is the new master
  local_tail->fields.tail_when_spliced = 1;
  //wait for predecessor to release lock
#if defined(OPTERON_OPTIMIZE)
  PREFETCHW(my_pred);
#endif	/* OPTERON_OPTIMIZE */
  while (my_pred->fields.successor_must_wait) {
    PAUSE;
#if defined(OPTERON_OPTIMIZE)
    pause_rep(23);
    PREFETCHW(my_pred);
#endif	/* OPTERON_OPTIMIZE */
  }
  //I have the lock. return qnode just released by previous owner for next lock access
  return my_pred;
}

int is_free_hclh(local_queue *lq, global_queue *gq, qnode *my_qnode) {
    if ((*lq)!=NULL) {
          qnode aux;
          aux.data=(*lq)->data;
      if ((aux.fields.cluster_id==my_qnode->fields.cluster_id) && 
	  (aux.fields.tail_when_spliced==0) && 
	  (aux.fields.successor_must_wait==0)) 
            return 1;
    }
    if ((*gq)->fields.successor_must_wait==0) return 1;
    return 0;
}

qnode* hclh_release(qnode *my_qnode, qnode * my_pred, uint8_t th_cluster) {
  my_qnode->fields.successor_must_wait = 0;
  qnode* pr = my_pred;
  qnode new_node;
  new_node.data=0;
  new_node.fields.cluster_id=th_cluster;
  new_node.fields.successor_must_wait = 1;
  new_node.fields.tail_when_spliced=0;

#if defined(OPTERON_OPTIMIZE)
  PREFETCHW(pr);
#endif	/* OPTERON_OPTIMIZE */
  uint32_t old_data = pr->data;
  while (CAS_U32(&pr->data,old_data,new_node.data)!=old_data) 
    {
      old_data=pr->data; 
      PAUSE;
#if defined(OPTERON_OPTIMIZE)
      PREFETCHW(pr);
#endif	/* OPTERON_OPTIMIZE */
    }
  my_qnode=pr;
  return my_qnode;
}

/*
 *  Methods aiding with array of locks manipulation
 */

#define INIT_VAL 123

hclh_global_params** init_hclh_locks(uint32_t num_locks) {
    DPRINT("sizeof(qnode) is %lu\n", sizeof(qnode));
    DPRINT("num_locks is %d\n", num_locks);
    hclh_global_params** the_params;
    the_params = (hclh_global_params**)malloc(num_locks * sizeof(hclh_global_params*));
    uint32_t i;
    for (i=0;i<num_locks;i++) {
        the_params[i]=(hclh_global_params*)malloc(sizeof(hclh_global_params));
        the_params[i]->local_queues = (local_queue**)malloc(NUMBER_OF_SOCKETS*sizeof(local_queue*));
        the_params[i]->init_done=(uint32_t*)malloc(NUMBER_OF_SOCKETS * sizeof(uint32_t));
        the_params[i]->shared_queue = (global_queue*)malloc(sizeof(global_queue));
        qnode * a_node = (qnode *) malloc(sizeof(qnode));
        a_node->data=0;
        a_node->fields.cluster_id = NUMBER_OF_SOCKETS+1;
        *(the_params[i]->shared_queue) = a_node;
    }
    return the_params;
}


hclh_local_params** init_thread_hclh(uint32_t phys_core, uint32_t num_locks, hclh_global_params** the_params) {
    //assign the thread to the correct core
    set_cpu(phys_core);
    hclh_local_params** local_params;
    local_params = (hclh_local_params**)malloc(num_locks * sizeof(hclh_local_params));
    uint32_t i;
#ifdef XEON
    __sync_synchronize();
    uint32_t real_core_num;
    for (i = 0; i < (NUMBER_OF_SOCKETS * CORES_PER_SOCKET); i++) {
	if (the_cores[i]==phys_core) {
		real_core_num = i;
		break;
	}
    }
    phys_core=real_core_num;
    __sync_synchronize();
    MEM_BARRIER;
#endif
    for (i = 0; i < num_locks; i++) {
        local_params[i]=(hclh_local_params*) malloc(sizeof(hclh_local_params));
        local_params[i]->my_qnode = (qnode*) malloc(sizeof(qnode));
        local_params[i]->my_qnode->data = 0;
        local_params[i]->my_qnode->fields.cluster_id  = phys_core/CORES_PER_SOCKET;
        local_params[i]->my_qnode->fields.successor_must_wait=1;
        local_params[i]->my_pred = NULL;
        if (phys_core%CORES_PER_SOCKET==0) {
            the_params[i]->local_queues[phys_core/CORES_PER_SOCKET] = (local_queue*)malloc(sizeof(local_queue));
            *(the_params[i]->local_queues[phys_core/CORES_PER_SOCKET]) = NULL;
#ifdef __tile__
    	    __sync_synchronize();
#endif
            the_params[i]->init_done[phys_core/CORES_PER_SOCKET]=INIT_VAL;
        }
        while(the_params[i]->init_done[phys_core/CORES_PER_SOCKET]!=INIT_VAL) {}
        local_params[i]->my_queue = the_params[i]->local_queues[phys_core/CORES_PER_SOCKET];
    }
        return local_params;
}

void end_thread_hclh(hclh_local_params** local_params, uint32_t size) {
    uint32_t i;
    for (i = 0; i < size; i++) {
        free(local_params[i]);
    }
    free(local_params);
}

void end_hclh(hclh_global_params** global_params, uint32_t size) {
  uint32_t i;
  for (i = 0; i < size; i++) {
    free(global_params[i]);
  }
  free(global_params); 
}


