DBG           = -g
#PROFILE       = -pg
OPT           = -O0
NATIVE_CPP    = CC
GNU_CPP       = g++
CFLAGS        = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP           = $(GNU_CPP)


INCLUDE       = -I../include -I../external/udis86-1.7
SRCDIR        = ../src
BINDIR        = ../bin
EXTRA_LIBS    = -lpebil -L../external/udis86-1.7/lib -ludis86

TARGET_BIN  = quickdump pebil

all: clean $(TARGET_BIN) install

quickdump : quickdump.o
	$(CPP) $(CFLAGS) -o $@ quickdump.o $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS)

pebil : main.o
	$(CPP) $(CFLAGS) -o $@ main.o $(CFLAGS) $(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.static core *.o *.ii *.s *.class $(TARGET_BIN)

install:
	cp $(TARGET_BIN) $(BINDIR)

