DBG         = -save-temps -g #-Wall
OPT         = -O3
LINK_FLAGS  = -Wl,--hash-style=sysv

GNU_CC      = gcc
GNU_CPP     = g++ -DCPP
GNU_F77     = g77

INTEL_CC    = icc
INTEL_CPP   = icc -lstdc++
INTEL_F77   = ifort

PGI_CC      = pgcc
PGI_CPP     = pgCC
PGI_F77     = pgf95

SUN_CC      = suncc
SUN_CPP     = sunCC
SUN_F77     = sunf77

CC          = $(PGI_CC)  $(OPT)
CPP         = $(PGI_CPP) $(OPT)
F77         = $(PGI_F77) $(OPT)

SRCDIR      = ../test
BINDIR      = ../src

SRCS        = foo.c bar.c dum.c main.c
OBJS        = foo.o bar.o dum.o main.o
SRCSCPP		= classes.C
OBJSCPP		= classes.o

EXECUTABLE  = cTest cppTest htcoef

build : all install

all: $(EXECUTABLE) 

%.o: %.c
	$(CC) -c -o $@ $< 

%.o: %.C
	$(CPP) -c -o $@ $< 

cTest: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LINK_FLAGS)

cppTest: $(OBJSCPP)
	$(CPP) -o $@ $(OBJSCPP) $(LINK_FLAGS)

htcoef:
	$(F77) -o $@ $@.f $(LINK_FLAGS)

install: $(EXECUTABLE)
	cp $(EXECUTABLE) $(BINDIR)

clean: 
	rm -f core *.new *.o *.i *.ii *.s *.class $(EXECUTABLE) 
	( cd $(BINDIR); rm -f $(EXECUTABLE) )

depend:
	$(GNU_CPP) -E -MM $(SRCS) $(SRCSCPP) > DEPENDS

#include DEPENDS
