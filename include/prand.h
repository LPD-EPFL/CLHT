#ifndef _PRAND_H_
#define _PRAND_H_

#include <assert.h>
#include "utils.h"

#define PRAND_LEN 8192
/* #define PRAND_LEN 65536 */
/* #define PRAND_LEN 2097152 */

typedef uint32_t prand_gen_t;

static inline prand_gen_t*
prand_new()
{
  prand_gen_t* g = (prand_gen_t*) malloc(sizeof(prand_gen_t) * PRAND_LEN);
  assert(g != NULL);

  unsigned long* s = seed_rand();

  int i;
  for (i = 0; i < PRAND_LEN; i++)
    {
      g[i] = (uint32_t) xorshf96(s, s+1, s+2);
    }

  free(s);
  return g;
}

static inline prand_gen_t*
prand_new_range(size_t min, size_t max)
{
  prand_gen_t* g = (prand_gen_t*) malloc(sizeof(prand_gen_t) * PRAND_LEN);
  assert(g != NULL);

  unsigned long* s = seed_rand();

  int i;
  for (i = 0; i < PRAND_LEN; i++)
    {
      g[i] = (uint32_t) (xorshf96(s, s+1, s+2) % max) + min;
    }

  free(s);
  return g;
}

static inline prand_gen_t
prand_nxt(const prand_gen_t* g, int* idx)
{
  return g[*idx++ & (PRAND_LEN - 1)];
}

#define PRAND_FOR(g, i, key)			\
  for (i = 0; i < PRAND_LEN; key = g[i], i++)



  


#endif
