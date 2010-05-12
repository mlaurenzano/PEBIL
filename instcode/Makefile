DBG         = -g
SHARED_OPT  = -fPIC
GNU_C       = gcc
NATIVE_C    = icc
AR          = ar
AR_FLAGS    = rcs
# Jade
#OPT         = -O3 -w -I/opt/mpt/3.5.0/xt/mpich2-pgi/include# --save-temps
#CC          = $(GNU_C) $(DBG) $(OPT)
#SHARED_LIB  = -shared
# Diamond
#OPT	    = -O3 -w
#CC          = $(GNU_C) $(DBG) $(OPT)
#SHARED_LIB  = -shared
# Normal
OPT         = -O3 -w
CC          = $(GNU_C) $(DBG) $(OPT)
SHARED_LIB  = -shared

CFLGS64     = -DMETASIM_64_BIT_LIB

#### -DNO_SAMPLING_MODE    : Add to disable sampling (note that MAX visit count is still on).
#### -DEXTENDED_SAMPLING   : Add to vary the ignore and sampling intervals during sampling
#### -DFINE_GRAIN_SAMPLING : Add to increase the granularity of sampling. Smaller intervals.
#### -DPER_SET_RECENT      : Add to simulcate LRU using per set most recent entry for replacement

## Following macros are disjoint. Only one is defined in corresponding C file.
## Check SimulationFunctions.c and SimulationCounter.c

#### -DCOUNT_BB_EXECCOUNT  : Add to print the overall BB visit counts (only for csc type SimulationCounter.c)

#EXTRA_CFLGS = -DNO_SAMPLING_MODE -DPER_SET_RECENT  # default for MultiMAPS tracing
#EXTRA_CFLGS = -DNO_SAMPLING_MODE -DPER_SET_RECENT -DVICTIM_CACHE  # allows victim caching also
EXTRA_CFLGS = -DEXTENDED_SAMPLING -DPER_SET_RECENT -DVICTIM_CACHE  # default for application tracing
#EXTRA_CFLGS = -DEXTENDED_SAMPLING -DPER_SET_RECENT  # default for application tracing
#EXTRA_CFLGS = -DNO_SAMPLING_MODE -DPER_SET_RECENT -DVICTIM_CACHE -DIGNORE_ACTUAL_SIMULATION # allows victim caching also
EXTRA_LIBS  =

INCDIR      = ../instcode
SRCDIR      = ../instcode
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = CounterFunctions.c SimulationFunctions.c CacheSimulationCommon.c TimerFunctions.c TracerFunctions.c
OBJS        = CounterFunctions.o SimulationFunctions.o CacheSimulationCommon.o TimerFunctions.o TracerFunctions.o

TARGET_LIBS_SHARED = libcounter.so libsimulator.so libtimer.so libtracer.so
TARGET_LIBS_STATIC = libcounter.a  libsimulator.a  libtimer.a  libtracer.a

# this might need to be defined differently depending on whether the system supports shared libraries or not
#TARGET_LIBS = $(TARGET_LIBS_SHARED) $(TARGET_LIBS_STATIC)
TARGET_LIBS = $(TARGET_LIBS_SHARED)
#TARGET_LIBS = $(TARGET_LIBS_STATIC)

all: $(TARGET_LIBS) install

shared: $(TARGET_LIBS_SHARED)

static: $(TARGET_LIBS_STATIC)

object: $(OBJS)

%.o: %.c 
	$(CC) $(EXTRA_CFLGS) $(CFLGS64) -I$(INCDIR) -c -o $@ $< $(SHARED_OPT)

libcounter.so : $(OBJS)
	$(CC) $(SHARED_LIB) -o $@ CounterFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)

libcounter.a : $(OBJS)
	$(AR) $(AR_FLAGS) $@ CounterFunctions.o

libsimulator.so : $(OBJS)
	$(CC) $(SHARED_LIB) -o $@ SimulationFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)

libsimulator.a : $(OBJS)
	$(AR) $(AR_FLAGS) $@ SimulationFunctions.o

libtimer.so : $(OBJS)
	$(CC) $(SHARED_LIB) -o $@ TimerFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)

libtimer.a : $(OBJS)
	$(AR) $(AR_FLAGS) $@ TimerFunctions.o

libtracer.so : $(OBJS)
#	$(CC) $(SHARED_LIB) -o $@ TracerFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)
#	jade
#	cc $(SHARED_LIB) -o $@ TracerFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)

#	diamond
	cc $(SHARED_LIB) -o $@ TracerFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)

libtracer.a : $(OBJS)
	$(AR) $(AR_FLAGS) $@ TracerFunctions.o


clean: 
	rm -f core *.o *.O *.i *.s *.ii *.class *.a *.so $(TARGET_LIBS)
	(cd $(LIBDIR); rm -f $(TARGET_LIBS))

install: $(TARGET_LIBS)
	cp $(TARGET_LIBS) $(LIBDIR)

depend:
	g++ -E -MM -I$(INCDIR) $(SRCS) > DEPENDS
