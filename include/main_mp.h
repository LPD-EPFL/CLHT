#ifndef __MAIN_MP_H__
#define __MAIN_MP_H__

#include "dht.h"

typedef uint8_t ssht_rpc_type;
#define  SSHT_PUT 0
#define  SSHT_GET 1
#define  SSHT_REM 2
#define  SSHT_EXT 3
#define  SSHT_SIZ 4
#define  SSHT_PRN 5

typedef struct ssht_rpc
{
  union
  {
    void* value;
    uint64_t resp;
  };
  uint32_t key;
  uint8_t  op;
} ssht_rpc_t;

#endif	/*  __MAIN_MP_H__ */
