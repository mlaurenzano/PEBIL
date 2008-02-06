DBG         = -save-temps -g #-Wall
OPT         = -O0 

GNU_CC      = gcc $(DBG) 
GNU_CPP     = g++ $(DBG) 

NATIVE_CC   = xlc -g 
NATIVE_CPP  = xlC -g 

CC          = $(GNU_CC) $(OPT)
CPP         = $(GNU_CPP) $(OPT) -DCPP

SRCDIR      = ../test
BINDIR      = ./compiled

SRCS        = foo.c bar.c dum.c main.c
OBJS        = foo.o bar.o dum.o main.o
SRCSCPP		= classes.C
OBJSCPP		= classes.o

EXECUTABLE  = cTest cppTest

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
	
install: $(EXECUTABLE)
	cp $(EXECUTABLE) $(BINDIR)

clean: 
	rm -f core *.new *.o *.i *.ii *.s *.class $(EXECUTABLE) 
	( cd $(BINDIR); rm -f $(EXECUTABLE) )

depend:
	$(GNU_CPP) -E -MM $(SRCS) $(SRCSCPP) > DEPENDS

#include DEPENDS
