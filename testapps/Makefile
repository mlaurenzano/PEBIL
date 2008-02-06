DBG         = -save-temps -g #-Wall
OPT         = -O0 

GNU_CC      = gcc $(DBG) 
GNU_CPP     = g++ $(DBG) 
GNU_F77     = f77

NATIVE_CC   = icc 
NATIVE_CPP  = icc 
NATIVE_F77  = ifort

CC          = $(GNU_CC)  $(OPT)
CPP         = $(GNU_CPP) $(OPT) -DCPP
F77         = $(GNU_F77) $(OPT)

SRCDIR      = ../test
BINDIR      = ./compiled

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
	$(CC) -o $@ $(OBJS)

cppTest: $(OBJSCPP)
	$(CPP) -o $@ $(OBJSCPP)

htcoef:
	$(F77) -o $@ $@.f

install: $(EXECUTABLE)
	cp $(EXECUTABLE) $(BINDIR)

clean: 
	rm -f core *.new *.o *.i *.ii *.s *.class $(EXECUTABLE) 
	( cd $(BINDIR); rm -f $(EXECUTABLE) )

depend:
	$(GNU_CPP) -E -MM $(SRCS) $(SRCSCPP) > DEPENDS

#include DEPENDS
