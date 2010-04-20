include rules/rules.$(COMP)

DBG         = -save-temps -g #-Wall
OPT         = -O3 -g -w #-static
CC          = $(PEBIL_CC)  $(OPT)
CPP         = $(PEBIL_CPP) $(OPT)
F77         = $(PEBIL_F77) $(OPT)
LNK         = ld
LINK_FLAGS  = $(DBG)

SRCDIR      = ../test
BINDIR      = ../src

SRCS        = foo.c bar.c dum.c main.c
OBJS        = foo.o bar.o dum.o main.o
SRCSCPP		= classes.C
OBJSCPP		= classes.o

EXECUTABLE  = cTest.$(COMP) cppTest.$(COMP) htcoef.$(COMP) dynTest.$(COMP)

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

dynTest.$(COMP):
	$(CC) -o $@ dynTest.c $(LINK_FLAGS)

install: $(EXECUTABLE)
	cp $(EXECUTABLE) $(BINDIR)
	cp $(EXECUTABLE) ./compiled/$(BITS)bit/

clean: 
	rm -f core *.new *.o *.i *.ii *.s *.class $(EXECUTABLE) 

fullclean: clean
	( cd $(BINDIR); rm -f $(EXECUTABLE) )

