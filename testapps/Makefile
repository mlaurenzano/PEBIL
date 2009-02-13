include rules.$(COMP)
include rules.$(COMP)

DBG         = -save-temps -g #-Wall
OPT         = -O3 -g
CC          = $(X86INST_CC)  $(OPT)
CPP         = $(X86INST_CPP) $(OPT)
F77         = $(X86INST_F77) $(OPT)

SRCDIR      = ../test
BINDIR      = ../src

SRCS        = foo.c bar.c dum.c main.c
OBJS        = foo.o bar.o dum.o main.o
SRCSCPP		= classes.C
OBJSCPP		= classes.o

EXECUTABLE  = cTest.$(COMP) cppTest.$(COMP) htcoef.$(COMP)

build : all install

all: $(EXECUTABLE) 

%.o: %.c
	$(CC) -c -o $@ $< 

%.o: %.C
	$(CPP) -c -o $@ $< 

cTest.$(COMP): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LINK_FLAGS)

cppTest.$(COMP): $(OBJSCPP)
	$(CPP) -o $@ $(OBJSCPP) $(LINK_FLAGS)

htcoef.$(COMP):
	$(F77) -o $@ htcoef.f $(LINK_FLAGS)

install: $(EXECUTABLE)
	cp $(EXECUTABLE) $(BINDIR)
	cp $(EXECUTABLE) ./compiled/$(BITS)bit/

clean: 
	rm -f core *.new *.o *.i *.ii *.s *.class $(EXECUTABLE) 
	( cd $(BINDIR); rm -f $(EXECUTABLE) )

depend:
	$(GNU_CPP) -E -MM $(SRCS) $(SRCSCPP) > DEPENDS

#include DEPENDS
