DBG         = -g
OPT         = -O3 -fPIC
NATIVE_C    = cc
GNU_C       = gcc
CC          = $(GNU_C) $(OPT)
SHARED_LIB  = -shared


CFLGS32     = -DX86INST_32_BIT_LIB
CFLGS64     = -DX86INST_64_BIT_LIB

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

SRCS        = TestFunctions.c
OBJS_32     = TestFunctions.o
OBJS_64     = TestFunctions.O

TARGET_BINS = libtest.so libtest_64.so

all: $(TARGET_BINS) install

%.o: %.c 
	$(CC) $(EXTRA_CFLGS) $(CFLGS32) -I$(INCDIR) -c -o $@ $< 

%.O: %.c 
	$(CC) $(EXTRA_CFLGS) $(CFLGS64) -I$(INCDIR) -c -o $@ $< 

libtest.so : TestFunctions.o
	$(CC) $(SHARED_LIB) $(CFLGS32) -o $@ TestFunctions.o -L$(LIBDIR) $(EXTRA_LIBS)

libtest_64.so : TestFunctions.O
	$(CC) $(SHARED_LIB) $(CFLGS64) -o $@ TestFunctions.O -L$(LIBDIR) $(EXTRA_LIBS)

clean: 
	rm -f core *.o *.O *.i *.ii *.s *.class $(TARGET_BINS)
	(cd $(LIBDIR); rm -f $(TARGET_BINS))

install: $(TARGET_BINS)
	cp $(TARGET_BINS) $(LIBDIR)

depend:
	g++ -E -MM -I$(INCDIR) $(SRCS) > DEPENDS
