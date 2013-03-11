#ifndef MCORE_LIB_H
#define MCORE_LIB_H

#include <string.h>
#define MCORE_SIZE (256 * 1024 * 1024)

typedef volatile unsigned char* t_vcharp;

void MCORE_shmalloc_set(void* mem);
void MCORE_shmalloc_init(size_t size);
t_vcharp MCORE_shmalloc(size_t size);
void MCORE_shfree(t_vcharp ptr);

#endif
