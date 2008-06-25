DBG         = -save-temps -g #-Wall
OPT         = -O0 
LINK_FLAGS  = -Wl,--hash-style=sysv

GNU_CC      = gcc $(DBG) 
GNU_CPP     = g++ $(DBG) 
GNU_F77     = g77

NATIVE_CC   = cc 
NATIVE_CPP  = cc 
NATIVE_F77  = f77

CC          = $(GNU_CC)  $(OPT)
CPP         = $(GNU_CPP) $(OPT) -DCPP
F77         = $(GNU_F77) $(OPT)

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
