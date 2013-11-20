ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING -DDEBUG
endif

ifeq ($(M),1)
MC=-DMEASURE_CONTENTION
endif


ALL= hyht hylzht

LIBS+=
LIBS_MP+=-lssmp

# default setings
PLATFORM=-DDEFAULT
GCC=gcc
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm 
LIBS_MP+= -lrt -lm


UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM=-DOPTERON
GCC=gcc
PLATFORM_NUMA=1
OPTIMIZE=-DOPTERON_OPTIMIZE
LIBS+= -lrt -lpthread -lm -lnuma
LIBS_MP+= -lrt -lm -lnuma
endif

ifeq ($(UNAME), lpdpc4)
PLATFORM=-DOPTERON
GCC=gcc
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
endif

ifeq ($(UNAME), diascld9)
PLATFORM=-DOPTERON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
endif

ifeq ($(UNAME), diassrv8)
PLATFORM=-DXEON
GCC=gcc
PLATFORM_NUMA=1
LIBS+= -lrt -lpthread -lm -lnuma
LIBS_MP+= -lrt -lm -lnuma
endif

ifeq ($(UNAME), diascld19)
PLATFORM=-DXEON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
endif

ifeq ($(UNAME), maglite)
PLATFORM=-DSPARC
GCC:=/opt/csw/bin/gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
LIBS_MP+= -lrt -lm -ltmc
endif

ifeq ($(UNAME), smal1.sics.se)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
LIBS_MP+= -lrt -lm -ltmc
endif


COMPILE_FLAGS += $(PLATFORM)
COMPILE_FLAGS += $(OPTIMIZE)


PRIMITIVE=-DLOCKS

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

LIBS+=-L$(TOP)/external/lib
LIBS_MP+=-L$(TOP)/external/lib

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := -I$(MAININCLUDE) -I$(TOP)/external/include
OBJ_FILES := 
OBJ_FILES_MP :=

all: hyht hyht_lat hylzht



dht.o: src/mcore_malloc.c include/mcore_malloc.h include/dht.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)


hyht: main_lock.c $(OBJ_FILES) src/dht.c include/dht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o hyht $(LIBS)

hyht_lat: main_lock.c $(OBJ_FILES) src/dht.c include/dht.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o hyhtl $(LIBS)


hylzht: main_lock.c $(OBJ_FILES) src/hylzht.c include/hylzht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/hylzht.c  -o hylzht $(LIBS)


clean:				
	rm -f *.o hyht hyht_lat hylzht
