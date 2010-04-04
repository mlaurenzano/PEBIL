DBG           = -g
#PROFILE       = -pg
OPT           = -O3 -w
NATIVE_CPP    = CC
GNU_CPP       = g++
CFLAGS        = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP           = $(GNU_CPP)


INCLUDE       = -I. -I../include -I../external/udis86-1.7/ -I$(HOME)/instructionAPI/include
SRCDIR        = ../src
BINDIR        = ../bin
LIBS          = -lpebil
EXTRA_LIBS    = -L../external/udis86-1.7/lib -ludis86 -L/home/michaell/instructionAPI//x86_64-unknown-linux2.4/lib -linstructionAPI -lbfd

TARGET_BIN  = pebil

SRCS  = BasicBlockCounter.C \
	CacheSimulation.C \
	FunctionCounter.C \
	FunctionTimer.C \
	IOTracer.C \
	InstrumentationTool.C

OBJS  =	BasicBlockCounter.o \
	CacheSimulation.o \
	FunctionCounter.o \
	FunctionTimer.o \
	IOTracer.o \
	InstrumentationTool.o

all: clean $(TARGET_BIN) install

pebil :	$(OBJS) pebil.o
	$(CPP) $(CFLAGS) -o $@ pebil.o $(OBJS) $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(LIBS) $(EXTRA_LIBS) $(PEBIL_LD_FLAGS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.static core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM $(INCLUDE) $(SRCS) > DEPENDS

install:
	cp $(TARGET_BIN) $(BINDIR)

include DEPENDS
