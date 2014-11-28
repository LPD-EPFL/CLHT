#################################
# Architecture dependent settings
#################################

ifndef ARCH
    ARCH_NAME = $(shell uname -m)
endif

ifeq ($(ARCH_NAME), i386)
    ARCH = x86
    CFLAGS += -m32
    LDFLAGS += -m32
    SSPFD = -lsspfd_x86
    LDFLAGS += -L$(LIBSSMEM)/lib -lssmem_x86
endif

ifeq ($(ARCH_NAME), i686)
    ARCH = x86
    CFLAGS += -m32
    LDFLAGS += -m32
    SSPFD = -lsspfd_x86
    LDFLAGS += -L$(LIBSSMEM)/lib -lssmem_x86
endif

ifeq ($(ARCH_NAME), x86_64)
    ARCH = x86_64
    CFLAGS += -m64
    LDFLAGS += -m64
    SSPFD = -lsspfd_x86_64
    LDFLAGS += -L$(LIBSSMEM)/lib -lssmem_x86_64
endif

ifeq ($(ARCH_NAME), sun4v)
    ARCH = sparc64
    CFLAGS += -DSPARC=1 -DINLINED=1 -m64
    LDFLAGS += -lrt -m64
    SSPFD = -lsspfd_sparc64
    LDFLAGS += -L$(LIBSSMEM)/lib -lssmem_sparc64
endif

ifeq ($(ARCH_NAME), tile)
    LDFLAGS += -L$(LIBSSMEM)/lib -lssmem_tile
    SSPFD = -lsspfd_tile
endif

ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -g -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else ifeq ($(DEBUG),2)
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else ifeq ($(DEBUG),3)
  DEBUG_FLAGS=-Wall -g -ggdb 
  COMPILE_FLAGS=-O3 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
endif

ifeq ($(SET_CPU),0)
	COMPILE_FLAGS += -DNO_SET_CPU
endif

ifeq ($(LATENCY),1)
	COMPILE_FLAGS += -DCOMPUTE_LATENCY -DDO_TIMINGS
endif

ifeq ($(LATENCY),2)
	COMPILE_FLAGS += -DCOMPUTE_LATENCY -DDO_TIMINGS -DUSE_SSPFD -DLATENCY_ALL_CORES=0
	LIBS += $(SSPFD) -lm
endif

ifeq ($(LATENCY),3)
	COMPILE_FLAGS += -DCOMPUTE_LATENCY -DDO_TIMINGS -DUSE_SSPFD -DLATENCY_ALL_CORES=1
	LIBS += $(SSPFD) -lm
endif

ifeq ($(LATENCY),4)
	COMPILE_FLAGS += -DCOMPUTE_LATENCY -DDO_TIMINGS -DUSE_SSPFD -DLATENCY_PARSING=1
	LIBS += $(SSPFD) -lm
endif

ifeq ($(LATENCY),5)
	COMPILE_FLAGS += -DCOMPUTE_LATENCY -DDO_TIMINGS -DUSE_SSPFD -DLATENCY_PARSING=1 -DLATENCY_ALL_CORES=1
	LIBS += $(SSPFD) -lm
endif

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

LIBS+=-L$(TOP)/external/lib

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

ifeq ($(M),1)
LIBS += -lsspfd
COMPILE_FLAGS += -DUSE_SSPFD
endif

# ALL= clht clht_lat clhtp clht_lat clhtp_lat clht_res clht_res_lat
ALL= clht_lb_res clht_lbp math_cache lfht math_cache_lf math_cache_nogc_lf math_cache_lf_dup lfht lfht_only_map_rem lfht_dup clht_lb_lock_ins lfht_res clht_lb_linked

# default setings
PLATFORM=-DDEFAULT
GCC=gcc
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm -lssmem

UNAME := $(shell uname -n)

ifeq ($(UNAME), lpd48core)
PLATFORM=-DOPTERON
GCC=gcc-4.8
PLATFORM_NUMA=1
OPTIMIZE=-DOPTERON_OPTIMIZE
LIBS+= -lrt -lpthread -lm -lnuma
endif

ifeq ($(UNAME), lpdxeon2680)
PLATFORM=-DXEON2
GCC=gcc
PLATFORM_NUMA=1
OPTIMIZE=
LIBS+= -lrt -lpthread -lm -lnuma
endif

ifeq ($(UNAME), lpdpc4)
PLATFORM=-DCOREi7
GCC=gcc
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm
endif

ifeq ($(UNAME), lpdpc34)
PLATFORM=-DCOREi7 -DRTM
GCC=gcc-4.8
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm -mrtm
endif

ifeq ($(UNAME), diascld9)
PLATFORM=-DOPTERON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
endif

ifeq ($(UNAME), diassrv8)
PLATFORM=-DXEON
GCC=gcc
PLATFORM_NUMA=1
LIBS+= -lrt -lpthread -lm -lnuma
endif

ifeq ($(UNAME), diascld19)
PLATFORM=-DXEON2
GCC=gcc
LIBS+= -lrt -lpthread -lm
endif

ifeq ($(UNAME), maglite)
PLATFORM=-DSPARC
GCC:=/opt/csw/bin/gcc
LIBS+= -lrt -lpthread -lm
COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
endif

ifeq ($(UNAME), parsasrv1.epfl.ch)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
endif

ifeq ($(UNAME), smal1.sics.se)
PLATFORM=-DTILERA
GCC=tile-gcc
LIBS+= -lrt -lpthread -lm -ltmc
endif

ifeq ($(UNAME), ol-collab1)
PLATFORM=-DT44
GCC=/usr/sfw/bin/gcc
COMPILE_FLAGS += -m64
LIBS+= -lrt -lpthread -lm
endif

COMPILE_FLAGS += $(PLATFORM)
COMPILE_FLAGS += $(OPTIMIZE)

INCLUDES := -I$(MAININCLUDE) -I$(TOP)/external/include
OBJ_FILES := 

BMARKS := bmarks

default: normal

all: $(ALL)

.PHONY: $(ALL)

normal: clean clht_lb_res lfht_res clht_lb_mem lfht_mem

clht_lb: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb.c include/clht_lb.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DNO_RESIZE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb.c src/clht_lb_gc.c -o clht_lb $(LIBS)

clht_lb_res: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(INCLUDES) $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lb_res.c src/clht_lb_gc.c -o clht_lb $(LIBS)

clht_lb_res_no_next: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lb_res_no_next.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lb_res_no_next.c src/clht_lb_gc.c -o clht_lb_nn $(LIBS)

clht_lb_ro: $(BMARKS)/test_ro.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h include/prand.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/test_ro.c src/clht_lb_res.c src/clht_lb_gc.c -o clht_lb_ro $(LIBS)

clht_lb_linked: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lb_linked.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DCLHT_LB_LINKED $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lb_linked.c src/clht_lb_gc.c -o clht_lb_linked $(LIBS)

clht_lb_linked_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lb_linked.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE -DCLHT_LB_LINKED $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lb_linked.c src/clht_lb_gc.c -o clht_lb_linked_lat $(LIBS)

clht_lf: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h include/prand.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lf.c -o clht_lf $(LIBS)

clht_lf_res: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lf_res.c include/clht_lf_res.h src/clht_lb_gc.c
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE_RES $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lf_res.c src/clht_lb_gc.c -o clht_lf_res $(LIBS)

clht_lf_res_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lf_res.c include/clht_lf_res.h src/clht_lb_gc.c
	$(GCC) -D_GNU_SOURCE -DLOCKFREE_RES $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lf_res.c src/clht_lb_gc.c -o clht_lf_res_lat $(LIBS)

lfht_mem: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lf_res.c include/clht_lf_res.h src/clht_lb_gc.c
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE_RES $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lf_res.c src/clht_lb_gc.c -o lfhtm $(LIBS)


clht_lb_lock_ins: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb_lock_ins.c include/clht_lb_lock_ins.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCK_INS $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb_lock_ins.c src/clht_lb_gc.c -o clht_lb_lock_ins $(LIBS)

lfht_dup: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/lfht_dup.c include/lfht_dup.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/lfht_dup.c -o lfht_dup $(LIBS)


lfht_assembly: src/clht_lf.c include/clht_lf.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE -S $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) src/clht_lf.c $(LIBS)

clht_lf_only_map_rem: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lf_only_map_rem.c include/clht_lf_only_map_rem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lf_only_map_rem.c src/clht_lb_gc.c -o clht_lf_only_map_rem $(LIBS)

clht_lb_res_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/clht_lb_res.c src/clht_lb_gc.c -o clht_lb_lat $(LIBS)

clht_lb_mem: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb_res.c src/clht_lb_gc.c -o clht_lbm $(LIBS)

clht_lb_mem_lat: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb_res.c src/clht_lb_gc.c -o clht_lb_latm $(LIBS)

math_cache: $(BMARKS)/math_cache.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(INCLUDES) $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(OBJ_FILES) $(BMARKS)/math_cache.c src/clht_lb_res.c src/clht_lb_gc.c -o math_cache $(LIBS)

math_cache_lf: $(BMARKS)/math_cache.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/clht_lf.c src/clht_lb_gc.c -o $(BMARKS)/math_cache_lf $(LIBS)

snap_stress: $(BMARKS)/snap_stress.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/snap_stress.c src/clht_lf.c src/clht_lb_gc.c -o $(BMARKS)/snap_stress $(LIBS)

full_stress_lf: full_stress.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) full_stress.c src/clht_lf.c src/clht_lb_gc.c -o full_stress_lf $(LIBS)

math_cache_lf_s: $(BMARKS)/math_cache.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/clht_lf.c src/clht_lb_gc.c $(LIBS) -S

lfht_s_annot: src/clht_lf.c include/clht_lf.h
	$(GCC) -c -g -Wa,-a,-ad -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) src/clht_lf.c $(LIBS) > lfht.lst

math_cache_lf_dup: $(BMARKS)/math_cache.c $(OBJ_FILES) src/lfht_dup.c include/lfht_dup.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/lfht_dup.c src/clht_lb_gc.c -o $(BMARKS)/math_cache_lf_dup $(LIBS)

math_cache_lock_ins: $(BMARKS)/math_cache.c $(OBJ_FILES) src/clht_lb_lock_ins.c include/clht_lb_lock_ins.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCK_INS $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/clht_lb_lock_ins.c src/clht_lb_gc.c -o $(BMARKS)/math_cache_lock_ins $(LIBS)

math_cache_nogc_lf: $(BMARKS)/math_cache_no_gc.c $(OBJ_FILES) src/clht_lf.c include/clht_lf.h 
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache_no_gc.c src/clht_lf.c src/clht_lb_gc.c -o $(BMARKS)/math_cache_nogc_lf $(LIBS)

math_cache_lat: $(BMARKS)/math_cache.c $(OBJ_FILES) src/clht_lb_res.c src/clht_lb_gc.c include/clht_lb_res.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/clht_lb_res.c src/clht_lb_gc.c -o $(BMARKS)/math_cache_lat $(LIBS)


clht_lbp: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb_packed.c include/clht_lb_packed.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb_packed.c -o clht_lbp $(LIBS)

clht_lbp_lat: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb_packed.c include/clht_lb_packed.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb_packed.c -o clht_lbp_lat $(LIBS)


clht_lb_lat: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/clht_lb.c include/clht_lb.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/clht_lb.c -o clht_lb_lat $(LIBS)

noise: noise.c
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT $(COMPILE_FLAGS) $(PRIMITIVE) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) noise.c -o noise $(LIBS)

clean:				
	rm -f *.o clht_lb* math_cache math_cache_lf* math_cache_nogc_lf lfht* full_stress_lf snap_stress
