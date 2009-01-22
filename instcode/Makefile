DBG         = -g
OPT         = -O3 -fPIC
NATIVE_C    = cc
GNU_C       = gcc
CC          = $(GNU_C) $(OPT) $(DBG)
SHARED_LIB  = -shared

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

TARGET_BINS = libcounter.so

all: $(TARGET_BINS) install

%.o: %.c 
	$(CC) $(EXTRA_CFLGS) -I$(INCDIR) -c -o $@ $< 

libcounter.so : $(OBJS)
	$(CC) $(SHARED_LIB) -o $@ CounterFunctions.o -L$(LIBDIR) $(EXTRA_LIBS)

clean: 
	rm -f core *.o *.O *.i *.s *.ii *.class $(TARGET_BINS)
	(cd $(LIBDIR); rm -f $(TARGET_BINS))

install: $(TARGET_BINS)
	cp $(TARGET_BINS) $(LIBDIR)

depend:
	g++ -E -MM -I$(INCDIR) $(SRCS) > DEPENDS
