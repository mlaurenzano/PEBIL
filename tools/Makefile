DBG           = -g
#PROFILE       = -pg
OPT           = -O3 -w
NATIVE_CPP    = CC
GNU_CPP       = g++
CFLAGS        = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP           = $(GNU_CPP)


SRCDIR        = ../src
BINDIR        = ../bin
EXTDIR        = ../external
INCLUDE       = -I. -I../include -I$(EXTDIR)/udis86-1.7/
LIBS          = -lpebil
EXTRA_LIBS    = -L$(EXTDIR)/udis86-1.7/lib -ludis86

TARGET_BIN  = pebil

SRCS  = BasicBlockCounter.C \
	CacheSimulation.C \
	CallReplace.C \
	FunctionCounter.C \
	FunctionTimer.C \
	InstrumentationTool.C

OBJS  =	BasicBlockCounter.o \
	CacheSimulation.o \
	CallReplace.o \
	FunctionCounter.o \
	FunctionTimer.o \
	InstrumentationTool.o

all: clean $(TARGET_BIN) install

pebil :	$(OBJS) pebil.o
	$(CPP) $(CFLAGS) -o $@ pebil.o $(OBJS) $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(LIBS) $(EXTRA_LIBS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

clean: 
	rm -f *.???inst *.static core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM $(INCLUDE) $(SRCS) > DEPENDS

install:
	cp $(TARGET_BIN) $(BINDIR)

include DEPENDS
