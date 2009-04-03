DBG         = -g
OPT         = -O3
SHARED_OPT  = -fPIC
NATIVE_C    = cc
GNU_C       = gcc
CC          = $(GNU_C) $(DBG) $(OPT)
AR_FLAGS    = -cvq
AR          = ar $(AR_FLAGS)
SHARED_LIB  = -shared
STATIC_LIB  = -c

#### -DNO_SAMPLING_MODE    : Add to disable sampling (note that MAX visit count is still on).
#### -DEXTENDED_SAMPLING   : Add to vary the ignore and sampling intervals during sampling
#### -DFINE_GRAIN_SAMPLING : Add to increase the granularity of sampling. Smaller intervals.
#### -DPER_SET_RECENT      : Add to simulcate LRU using per set most recent entry for replacement

## Following macros are disjoint. Only one is defined in corresponding C file.
## Check SimulationFunctions.c and SimulationCounter.c

#### -DCOUNT_BB_EXECCOUNT  : Add to print the overall BB visit counts (only for csc type SimulationCounter.c)

INCDIR      = ../instcode
SRCDIR      = ../instcode
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = CounterFunctions.c
OBJS        = CounterFunctions.o

TARGET_LIBS = libcounter.so libcounter.a

all: $(TARGET_LIBS) install

%.o: %.c 
	$(CC) $(EXTRA_CFLGS) -I$(INCDIR) -c -o $@ $< 

libcounter.so : $(OBJS)
	$(CC) $(SHARED_LIB) -o $@ CounterFunctions.o -L$(LIBDIR) $(EXTRA_LIBS) $(SHARED_OPT)
libcounter.a : $(OBJS)
	$(AR) $@ CounterFunctions.o

clean: 
	rm -f core *.o *.O *.i *.s *.ii *.class $(TARGET_BINS)
	(cd $(LIBDIR); rm -f $(TARGET_BINS))

install: $(TARGET_LIBS)
	cp $(TARGET_LIBS) $(LIBDIR)

depend:
	g++ -E -MM -I$(INCDIR) $(SRCS) > DEPENDS
