DBG         = -g
#PROFILE     = -pg
OPT         = -O0
NATIVE_CPP  = CC
GNU_CPP     = g++
CFLAGS      = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP         = $(GNU_CPP)


INCLUDE     = ../include
SRCDIR      = ../src
BINDIR      = ../bin
EXTRA_LIBS  = -lx86inst

TARGET_BIN  = quickdump x86inst

all: $(TARGET_BIN) install

quickdump : quickdump.o
	$(CPP) $(CFLAGS) -o $@ quickdump.o $(CFLAGS) -I$(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS)

x86inst : main.o
	$(CPP) $(CFLAGS) -o $@ main.o $(CFLAGS) -I$(INCLUDE) -L$(BINDIR) $(EXTRA_LIBS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.static core *.o *.ii *.s *.class $(TARGET_BIN)

install:
	cp $(TARGET_BIN) $(BINDIR)

