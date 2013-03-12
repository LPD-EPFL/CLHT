ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
endif


UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM=OPTERON
CC=gcc
PLATFORM_NUMA=1
PLATFORM=-DOPTERON
OPTIMIZE=-DOPTERON_OPTIMIZE
LIBS+= -lrt -lpthread -lm -lnuma
endif

ifeq ($(UNAME), diassrv8)
PLATFORM=-DXEON
GCC=gcc
PLATFORM_NUMA=1
LIBS+= -lrt -lpthread -lm -lnuma
ALL=latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket sequential
endif

ifeq ($(UNAME), maglite)
PLATFORM=-DSPARC
GCC:=/opt/csw/bin/gcc
LIBS+= -lrt -lpthread -lm
COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
ALL=latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex throughput_clh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex sequential
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM=TILERA
CC=tile-gcc
PERF_CLFAGS= -ltmc
LINK=-ltmc
endif

ifeq ($(UNAME), diascld19)
PLATFORM=XEON2
CC=gcc
# PLATFORM_NUMA=1
endif

ifeq ($(UNAME), diascld9)
PLATFORM=OPTERON2
CC=gcc
# PLATFORM_NUMA=1
endif

COMPILE_FLAGS += $(PLATFORM)
COMPILE_FLAGS += $(OPTIMIZE)


LIBS+=-lsync

PRIMITIVE=-DTEST_FAI

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

LIBS+=-L$(TOP)/external/lib \

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := -I$(MAININCLUDE) -I$(TOP)/external/include
#OBJ_FILES := mcs.o ttas.o rw_ttas.o ticket.o alock.o hclh.o htlock.o mcore_malloc.o
OBJ_FILES := mcore_malloc.o
OBJ_FILES_MP := ssmp.o ssmp_send.o ssmp_recv.o

all: $(ALL)

# latency_hclh latency_clh latency_ttas latency_mcs latency_array latency_ticket latency_spinlock latency_mutex latency_hticket throughput_clh throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_spinlock throughput_mutex throughput_hticket sequential

# ssmp.o: src/ssmp.c include/ssmp.h
# 	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp.c $(LIBS) 

# ssmp_send.o: src/ssmp_send.c include/ssmp_send.h
# 	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp_send.c $(LIBS) 

# ssmp_recv.o: src/ssmp_recv.c include/ssmp_recv.h
# 	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp_recv.c $(LIBS) 

mcore_malloc.o: src/mcore_malloc.c include/mcore_malloc.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)

latency_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hclh $(LIBS)

latency_clh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_CLH_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_clh $(LIBS)


latency_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ttas $(LIBS)

latency_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mcs $(LIBS)

latency_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_array $(LIBS)

latency_ticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ticket $(LIBS)


latency_spinlock: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_SPINLOCK_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_spinlock $(LIBS)


latency_mutex: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mutex $(LIBS)

latency_hticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HTICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hticket $(LIBS)

throughput_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_hclh $(LIBS)

throughput_clh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_CLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_clh $(LIBS)

throughput_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ttas $(LIBS)

throughput_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_mcs $(LIBS)

throughput_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_array $(LIBS)

throughput_ticket: main_lock.c $(OBJ_FILES) src/dht.c
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ticket $(LIBS)

throughput_spinlock: main_lock.c $(OBJ_FILES) src/dht.c
	$(GCC) -DUSE_SPINLOCK_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_spinlock $(LIBS)

throughput_mutex: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_mutex $(LIBS)

throughput_hticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HTICKET_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_hticket $(LIBS)

sequential: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DSEQUENTIAL $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o sequential $(LIBS)

throughput_mp: main_mp.c $(OBJ_FILES_MP) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES_MP) -g main_mp.c src/dht.c -o throughput_mp $(LIBS)

clean:				
	rm -f *.o latency_hclh latency_ttas latency_mcs latency_array latency_ticket latency_mutex latency_hticket throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_mutex throughput_hticket sequential throughput_mp results/*.txt *.eps *.pdf *.ps

