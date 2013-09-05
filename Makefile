ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
endif

ifeq ($(M),1)
MC=-DMEASURE_CONTENTION
endif

LIBS+=-lsync
LIBS_MP+=-lssmp

UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM=-DOPTERON
GCC=gcc
PLATFORM_NUMA=1
OPTIMIZE=-DOPTERON_OPTIMIZE
LIBS+= -lrt -lpthread -lm -lnuma
LIBS_MP+= -lrt -lm -lnuma
ALL=latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket throughput_mp sequential
endif

ifeq ($(UNAME), diascld9)
PLATFORM=-DOPTERON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
ALL=latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket throughput_mp sequential
endif

ifeq ($(UNAME), diassrv8)
PLATFORM=-DXEON
GCC=gcc
PLATFORM_NUMA=1
LIBS+= -lrt -lpthread -lm -lnuma
LIBS_MP+= -lrt -lm -lnuma
ALL=latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket throughput_mp sequential
endif

ifeq ($(UNAME), diascld19)
PLATFORM=-DXEON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
ALL=latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket throughput_mp sequential
endif

ifeq ($(UNAME), maglite)
PLATFORM=-DSPARC
GCC:=/opt/csw/bin/gcc
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
ALL=latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex throughput_clh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_mp sequential
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
LIBS_MP+= -lrt -lm -ltmc
ALL=latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex throughput_clh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_mp sequential
endif

ifeq ($(UNAME), smal1.sics.se)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
LIBS_MP+= -lrt -lm -ltmc
ALL=throughput_mp
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
#OBJ_FILES := mcs.o ttas.o rw_ttas.o ticket.o alock.o hclh.o htlock.o mcore_malloc.o
OBJ_FILES := mcore_malloc.o
OBJ_FILES_MP := mcore_malloc_mp.o dht_mp.o

all: $(ALL)

# latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket sequential

mcore_malloc.o: src/mcore_malloc.c include/mcore_malloc.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)

mcore_malloc_mp.o: src/mcore_malloc.c include/mcore_malloc.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c -o mcore_malloc_mp.o $(LIBS_MP)


dht.o: src/mcore_malloc.c include/mcore_malloc.h include/dht.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)

latency_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hclh $(LIBS)

latency_clh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_CLH_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_clh $(LIBS)


latency_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ttas $(LIBS)

latency_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mcs $(LIBS)

latency_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_array $(LIBS)

latency_ticket: main_lock.c src/dht.c $(OBJ_FILES) 
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ticket $(LIBS)

latency_spinlock: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_SPINLOCK_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_spinlock $(LIBS)

latency_mutex: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mutex $(LIBS)

latency_hticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HTICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hticket $(LIBS)

throughput_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_hclh $(LIBS)

throughput_clh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_CLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_clh $(LIBS)

throughput_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ttas $(LIBS)

throughput_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_mcs $(LIBS)

throughput_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_array $(LIBS)

throughput_ticket: main_lock.c $(OBJ_FILES) src/dht.c include/dht.h
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ticket $(LIBS)

throughput_spinlock: main_lock.c $(OBJ_FILES) src/dht.c
	$(GCC) -DUSE_SPINLOCK_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_spinlock $(LIBS)

throughput_mutex: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_mutex $(LIBS)

throughput_hticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HTICKET_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_hticket $(LIBS)

sequential: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DSEQUENTIAL $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o sequential $(LIBS)


dht_mp.o: src/dht.c include/dht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) -DMESSAGE_PASSING $(DEBUG_FLAGS) $(INCLUDES) -c src/dht.c -o dht_mp.o $(LIBS_MP)

throughput_mp: main_mp.c $(OBJ_FILES_MP) 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) -DMESSAGE_PASSING $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES_MP) main_mp.c -o throughput_mp $(LIBS_MP)

clean:				
	rm -f *.o latency_hclh latency_ttas latency_mcs latency_array latency_ticket latency_mutex latency_hticket throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_mutex throughput_hticket sequential throughput_mp results/*.txt *.eps *.pdf *.ps

