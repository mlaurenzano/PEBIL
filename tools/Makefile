DBG           = -g
#PROFILE       = -pg
OPT           = -O3 -w -static
NATIVE_CPP    = CC
GNU_CPP       = g++
CFLAGS        = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP           = $(GNU_CPP)


INCLUDE       = -I. -I../include -I../external/udis86-1.7/
SRCDIR        = ../src
BINDIR        = ../bin
EXTRA_LIBS    = -lpebil -L../external/udis86-1.7/lib -ludis86

TARGET_BIN  = pebil ftimer



SRCS  = BasicBlockCounter.C \
	CacheSimulation.C \
	FunctionCounter.C \
	FunctionTimer.C \
	InstrumentationTool.C

OBJS  =	BasicBlockCounter.o \
	CacheSimulation.o \
	FunctionCounter.o \
	FunctionTimer.o \
	InstrumentationTool.o

all: clean $(TARGET_BIN) install

pebil :	$(OBJS) main.o
	$(CPP) $(CFLAGS) -o $@ main.o $(OBJS) $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS) $(PEBIL_LD_FLAGS)

ftimer: $(OBJS) ftimer.o
	$(CPP) $(CFLAGS) -o $@ ftimer.o $(OBJS) $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS) $(PEBIL_LD_FLAGS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.static core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM $(INCLUDE) $(SRCS) > DEPENDS

install:
	cp $(TARGET_BIN) $(BINDIR)

include DEPENDS
