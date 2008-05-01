#DBG         = -g
OPT         = -O3
#NATIVE_CPP  = CC
#CFLAGS      = -bmaxdata:0x80000000 $(DBG) $(OPT)
NATIVE_CPP  = CC
GNU_CPP     = g++
CFLAGS      = $(DBG) $(OPT)
CPP         = $(GNU_CPP) 
EXTRA_CFLGS =
AR          = ar ruv
RANLIB      = ranlib
#EXTRA_LIBS  = -lopcodes
EXTRA_LIBS  = /scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes/libopcodes.a
EXTRA_INC   = -I/scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes

INCLUDE     = ../include
SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = Base.C ElfFile.C BinaryFile.C FileHeader.C ProgramHeader.C SectionHeader.C \
              StringTable.C SymbolTable.C RelocationTable.C main.C
OBJS        = Base.o ElfFile.o BinaryFile.o FileHeader.o ProgramHeader.o SectionHeader.o \
              StringTable.o SymbolTable.o RelocationTable.o main.o

TARGET_BIN  = x86inst

all: $(TARGET_BIN)

$(TARGET_BIN) : $(OBJS)
	$(CPP) -o $@ $(OBJS) -I$(INCLUDE) $(EXTRA_LIBS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM -I$(INCLUDE) $(SRCS) > DEPENDS

include DEPENDS
