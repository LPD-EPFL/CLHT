ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else ifeq ($(DEBUG),2)
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING -DDEBUG
endif

ifeq ($(M),1)
LIBS += -lsspfd
COMPILE_FLAGS += -DUSE_SSPFD
endif

# ALL= hyht hyht_lat hyhtp hyht_lat hyhtp_lat hyht_res hyht_res_lat
ALL= hyht_res math_cache lfht math_cache_lf math_cache_nogc_lf math_cache_lf_dup lfht lfht_only_map_rem lfht_dup hyht_lock_ins lfht_res hyht_linked

LIBS_MP += -lssmp

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

ifeq ($(UNAME), lpdxeon2680)
PLATFORM=-DXEON2
GCC=gcc
PLATFORM_NUMA=1
OPTIMIZE=
LIBS+= -lrt -lpthread -lm -lnuma
LIBS_MP+= -lrt -lm -lnuma
endif


ifeq ($(UNAME), lpdpc4)
PLATFORM=-DCOREi7
GCC=gcc
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm
LIBS_MP+= -lrt -lm
endif

ifeq ($(UNAME), lpdpc34)
PLATFORM=-DCOREi7 -DRTM
GCC=gcc-4.8
PLATFORM_NUMA=0
OPTIMIZE=
LIBS+= -lrt -lpthread -lm -mrtm
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

BMARKS := bmarks

default: all

all: $(ALL)

normal: hyht_res hyht_res_lat hyht_linked hyht_linked_lat lfht_res lfht_res_lat 

dht.o: src/mcore_malloc.c include/mcore_malloc.h include/dht.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) -c src/mcore_malloc.c $(LIBS)

hyht: main_lock.c $(OBJ_FILES) src/dht.c include/dht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/dht.c -o hyht $(LIBS)

hyht_res: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/dht_res.c src/hyht_gc.c -o hyht $(LIBS)

hyht_linked: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/dht_linked.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DHYHT_LINKED  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/dht_linked.c src/hyht_gc.c -o hyht_linked $(LIBS)

hyht_linked_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/dht_linked.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE -DHYHT_LINKED  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/dht_linked.c src/hyht_gc.c -o hyht_linked_lat $(LIBS)

lfht: $(BMARKS)/main_lock.c $(OBJ_FILES) src/lfht.c include/lfht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/lfht.c -o lfht $(LIBS)

lfht_res: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/lfht_res.c include/lfht_res.h src/hyht_gc.c
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE_RES  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/lfht_res.c src/hyht_gc.c -o lfht_res $(LIBS)

lfht_res_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/lfht_res.c include/lfht_res.h src/hyht_gc.c
	$(GCC) -D_GNU_SOURCE -DLOCKFREE_RES  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/lfht_res.c src/hyht_gc.c -o lfht_res_lat $(LIBS)

hyht_lock_ins: $(BMARKS)/main_lock.c $(OBJ_FILES) src/hyht_lock_ins.c include/hyht_lock_ins.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCK_INS $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/hyht_lock_ins.c -o hyht_lock_ins $(LIBS)

lfht_dup: $(BMARKS)/main_lock.c $(OBJ_FILES) src/lfht_dup.c include/lfht_dup.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/lfht_dup.c -o lfht_dup $(LIBS)


lfht_assembly: src/lfht.c include/lfht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE  -S $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) src/lfht.c $(LIBS)

lfht_only_map_rem: $(BMARKS)/main_lock.c $(OBJ_FILES) src/lfht_only_map_rem.c include/lfht_only_map_rem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/lfht_only_map_rem.c -o lfht_only_map_rem $(LIBS)

hyht_res_lat: $(BMARKS)/main_lock_res.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_res.c src/dht_res.c src/hyht_gc.c -o hyht_lat $(LIBS)

hyht_mem: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/dht_res.c src/hyht_gc.c src/ssmem.c -o hyhtm $(LIBS)

hyht_mem_lat: $(BMARKS)/main_lock_mem.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock_mem.c src/dht_res.c src/hyht_gc.c src/ssmem.c -o hyht_latm $(LIBS)

math_cache: $(BMARKS)/math_cache.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c src/ssmem.c include/dht_res.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/dht_res.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache $(LIBS)

math_cache_lf: $(BMARKS)/math_cache.c $(OBJ_FILES) src/lfht.c src/ssmem.c include/lfht.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/lfht.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache_lf $(LIBS)

snap_stress: $(BMARKS)/snap_stress.c $(OBJ_FILES) src/lfht.c src/ssmem.c include/lfht.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES)  $(BMARKS)/snap_stress.c src/lfht.c src/hyht_gc.c src/ssmem.c -o  $(BMARKS)/snap_stress $(LIBS)

full_stress_lf: full_stress.c $(OBJ_FILES) src/lfht.c src/ssmem.c include/lfht.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) full_stress.c src/lfht.c src/hyht_gc.c src/ssmem.c -o full_stress_lf $(LIBS)

math_cache_lf_s: $(BMARKS)/math_cache.c $(OBJ_FILES) src/lfht.c src/ssmem.c include/lfht.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/lfht.c src/hyht_gc.c src/ssmem.c $(LIBS) -S

lfht_s_annot: src/lfht.c include/lfht.h
	$(GCC) -c -g -Wa,-a,-ad -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) src/lfht.c $(LIBS) > lfht.lst

math_cache_lf_dup: $(BMARKS)/math_cache.c $(OBJ_FILES) src/lfht_dup.c src/ssmem.c include/lfht_dup.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/lfht_dup.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache_lf_dup $(LIBS)

math_cache_lock_ins: $(BMARKS)/math_cache.c $(OBJ_FILES) src/hyht_lock_ins.c src/ssmem.c include/hyht_lock_ins.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCK_INS $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/hyht_lock_ins.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache_lock_ins $(LIBS)

math_cache_nogc_lf: $(BMARKS)/math_cache_no_gc.c $(OBJ_FILES) src/lfht.c src/ssmem.c include/lfht.h include/ssmem.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT -DLOCKFREE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache_no_gc.c src/lfht.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache_nogc_lf $(LIBS)

math_cache_lat: $(BMARKS)/math_cache.c $(OBJ_FILES) src/dht_res.c src/hyht_gc.c include/dht_res.h
	$(GCC) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/math_cache.c src/dht_res.c src/hyht_gc.c src/ssmem.c -o $(BMARKS)/math_cache_lat $(LIBS)


hyhtp: $(BMARKS)/main_lock.c $(OBJ_FILES) src/dht_packed.c include/dht_packed.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/dht_packed.c -o hyhtp $(LIBS)

hyhtp_lat: $(BMARKS)/main_lock.c $(OBJ_FILES) src/dht_packed.c include/dht_packed.h
	$(GCC) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/dht_packed.c -o hyhtp_lat $(LIBS)


hyht_lat: $(BMARKS)/main_lock.c $(OBJ_FILES) src/dht.c include/dht.h
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/dht.c -o hyht_lat $(LIBS)


hylzht: $(BMARKS)/main_lock.c $(OBJ_FILES) src/hylzht.c include/hylzht.h
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) $(BMARKS)/main_lock.c src/hylzht.c  -o hylzht $(LIBS)


noise: noise.c
	$(GCC) -D_GNU_SOURCE -DCOMPUTE_THROUGHPUT  $(COMPILE_FLAGS) $(PRIMITIVE)  $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) noise.c -o noise $(LIBS)

clean:				
	rm -f *.o hyht* math_cache math_cache_lf* math_cache_nogc_lf lfht* full_stress_lf snap_stress
