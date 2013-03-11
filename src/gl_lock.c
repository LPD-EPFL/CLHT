#include "gl_lock.h"

void local_lock_write(global_lock* gl) {
    while(1) {
        while (gl->lock_short.global_lock != 0) {}
        unsigned int aux = (unsigned int) gl->lock_short.local_lock;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+0x100) == (aux)) {
            return;
        }
        //else { backoff();} //???
    }
}

void local_unlock_write(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.local_write),1);
}

void local_lock_read(global_lock* gl) {
    while(1) {
        while (gl->lock.global_write != 0) {}
        unsigned int aux = (unsigned int) gl->lock_data & 0x00ffffff;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+1) == (aux)) {
            return;
        }
        //else { backoff();} //???
    }
}

void local_unlock_read(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.local_read),1);
}


void global_acquire_write(global_lock* gl) {
    while(1) {
        while (gl->lock_data != 0) {}
        unsigned short aux = (unsigned short) 0x1000000;
        if (__sync_val_compare_and_swap(&gl->lock_data, 0, aux) == 0) {
            return;
        }
        //else { backoff();} //???
    }
}


void global_unlock_write(global_lock* gl) {
    gl->lock_data = 0;
}

void global_acquire_read(global_lock* gl) {
    while(1) {
        while ((gl->lock.global_write != 0) || (gl->lock.local_write != 0)) {}
        unsigned int aux = (unsigned int) gl->lock_data & 0x00ff00ff;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+0x10000) == (aux)) {
            return;
        }
        //else { backoff();} //???
    }
}

void global_unlock_read(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.global_read),1);
}


