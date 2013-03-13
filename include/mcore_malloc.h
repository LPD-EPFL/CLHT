#ifndef MCORE_LIB_H
#define MCORE_LIB_H

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
#include <string.h>

#if defined(MESSAGE_PASSING)
#include <ssmp.h>
#endif

#define MCORE_SIZE (10 * 1024 * 1024)

typedef volatile unsigned char* t_vcharp;

void MCORE_shmalloc_set(void* mem);
void MCORE_shmalloc_init(size_t size);
void MCORE_shmalloc_offset(size_t size);
void* MCORE_shmalloc(size_t size);
void MCORE_shfree(t_vcharp ptr);

#endif
