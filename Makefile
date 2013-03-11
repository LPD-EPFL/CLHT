ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
endif

#PLATFORM=-DSPARC
PLATFORM=-DOPTERON
OPTIMIZE=-DOPTERON_OPTIMIZE

COMPILE_FLAGS += $(PLATFORM)
COMPILE_FLAGS += $(OPTIMIZE)

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	GCC:=gcc
	LIBS := -lrt -lpthread -lnuma -lm
endif
ifeq ($(UNAME), SunOS)
	GCC:=/opt/csw/bin/gcc
	LIBS := -lrt -lpthread 
        COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
endif

PRIMITIVE=-DTEST_FAI

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := -I$(MAININCLUDE)
OBJ_FILES := mcs.o ttas.o rw_ttas.o ticket.o alock.o hclh.o htlock.o mcore_malloc.o
OBJ_FILES_MP := ssmp.o ssmp_send.o ssmp_recv.o

# all: latency_hclh latency_ttas latency_mcs latency_array latency_ticket latency_mutex latency_hticket throughput_hclh throughput_ttas throughput_mcs throughput_array throughput_ticket throughput_mutex throughput_hticket sequential

all: latency_ticket throughput_ticket


ttas.o: src/ttas.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ttas.c $(LIBS)

rw_ttas.o: src/rw_ttas.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/rw_ttas.c $(LIBS)

ticket.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ticket.c $(LIBS)

gl_lock.o: src/gl_lock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/gl_lock.c $(LIBS)

mcs.o: src/mcs.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/mcs.c $(LIBS)

hclh.o: src/hclh.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/hclh.c $(LIBS)

alock.o: src/alock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/alock.c $(LIBS)

htlock.o: src/htlock.c include/htlock.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/htlock.c $(LIBS) 

ssmp.o: src/ssmp.c include/ssmp.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp.c $(LIBS) 

ssmp_send.o: src/ssmp_send.c include/ssmp_send.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp_send.c $(LIBS) 

ssmp_recv.o: src/ssmp_recv.c include/ssmp_recv.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ssmp_recv.c $(LIBS) 

mcore_malloc.o: src/mcore_malloc.c include/mcore_malloc.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)

latency_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hclh $(LIBS)

latency_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ttas $(LIBS)

latency_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mcs $(LIBS)

latency_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_array $(LIBS)

latency_ticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_ticket $(LIBS)

latency_mutex: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MUTEX_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_mutex $(LIBS)

latency_hticket: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_HTICKET_LOCKS -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o latency_hticket $(LIBS)

throughput_hclh: main_lock.c $(OBJ_FILES)
	$(GCC) -DUSE_HCLH_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_hclh $(LIBS)

throughput_ttas: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_TTAS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ttas $(LIBS)

throughput_mcs: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_MCS_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_mcs $(LIBS)

throughput_array: main_lock.c $(OBJ_FILES) 
	$(GCC) -DUSE_ARRAY_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_array $(LIBS)

throughput_ticket: main_lock.c $(OBJ_FILES) src/dht.c
	$(GCC) -DUSE_TICKET_LOCKS -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) main_lock.c src/dht.c -o throughput_ticket $(LIBS)

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

