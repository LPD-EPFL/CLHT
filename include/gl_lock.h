//global rw lock
#ifndef _GLLOCK_H_
#define _GLLOCK_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "utils.h"

typedef struct glock{
    volatile unsigned char local_read;
    volatile unsigned char local_write;
    volatile unsigned char global_read;
    volatile unsigned char global_write;
 } glock;

typedef struct glock_2{
    volatile unsigned short local_lock;
    volatile unsigned short global_lock;
 } glock_2;

typedef struct global_lock {
    union {
        volatile unsigned int lock_data;
        glock_2 lock_short;
        glock lock;
        volatile unsigned char padding[CACHE_LINE_SIZE];
    };
} global_lock;


void local_lock_write(global_lock* gl);

void local_unlock_write(global_lock* gl);

void local_lock_read(global_lock* gl);

void local_unlock_read(global_lock* gl);

void global_acquire_write(global_lock* gl);

void global_acquire_read(global_lock* gl);

void global_unlock_write(global_lock* gl);

void global_unlock_read(global_lock* gl);

#endif
