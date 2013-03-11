#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

#include "mcore_malloc.h"

#define MAX_FILENAME_LENGTH 100

static void* mcore_app_mem;
static size_t alloc_next = 0;
static void* mcore_free_list[256] = {0};
static uint8_t mcore_free_cur = 0;
static uint8_t mcore_free_num = 0;


void
MCORE_shmalloc_set(void* mem)
{
  mcore_app_mem = mem;
}


/* 
   On TILERA we open the shmem in sys_tm_init
 */
#if !defined(PLATFORM_TILERA)
//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shmalloc_init
//--------------------------------------------------------------------------------------
// initialize memory allocator
//--------------------------------------------------------------------------------------
// size (bytes) of managed space
void
MCORE_shmalloc_init(size_t size)
{
   //create the shared space which will be managed by the allocator

   char keyF[MAX_FILENAME_LENGTH];
   sprintf(keyF,"/mcore_mem2");

   int shmfd = shm_open(keyF, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
   if (shmfd<0)
   {
      if (errno != EEXIST)
      {
         perror("In shm_open");
         exit(1);
      }

      //this time it is ok if it already exists
      shmfd = shm_open(keyF, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
      if (shmfd<0)
      {
         perror("In shm_open");
         exit(1);
      }
   }
   else
   {
      //only if it is just created
     if (!ftruncate(shmfd,size))
       {
	 printf("ftruncate failed\n");
       }
   }

  t_vcharp mem = (t_vcharp) mmap(NULL, size,PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
  assert(mem != NULL);

  // create one block containing all memory for truly dynamic memory allocator
  mcore_app_mem = (void*) mem;
}

#endif	/* PLATFORM_TILERA */

//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shmalloc
//--------------------------------------------------------------------------------------
// Allocate memory in off-chip shared memory. This is a collective call that should be
// issued by all participating cores if consistent results are required. All cores will
// allocate space that is exactly overlapping. Alternatively, determine the beginning of
// the off-chip shared memory on all cores and subsequently let just one core do all the
// allocating and freeing. It can then pass offsets to other cores who need to know what
// shared memory regions were involved.
//--------------------------------------------------------------------------------------
 // requested space
t_vcharp
MCORE_shmalloc(size_t size)
{
  void *ret;
  if (mcore_free_num > 2)
    {
      uint8_t spot = mcore_free_cur - mcore_free_num;
      ret = mcore_free_list[spot];
      mcore_free_num--;
    }
  else
    {
      ret = mcore_app_mem + alloc_next;
      alloc_next += size;
    }

  /* PRINT("[lib] allocated %p [offs: %lu]", ret, mcore_app_addr_offs(ret)); */

  return (t_vcharp) ret;
}

//--------------------------------------------------------------------------------------
// FUNCTION: MCORE_shfree
//--------------------------------------------------------------------------------------
// Deallocate memory in off-chip shared memory. Also collective, see MCORE_shmalloc
//--------------------------------------------------------------------------------------
// pointer to data to be freed
void
MCORE_shfree(t_vcharp ptr)
{
  mcore_free_num++;
  /* PRINT("free %3d (num_free after: %3d)", mcore_free_cur, mcore_free_num); */
  mcore_free_list[mcore_free_cur++] = (void*) ptr;
}
