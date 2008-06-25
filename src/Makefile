#DBG         = -g
OPT         = -O3
#CFLAGS      = -bmaxdata:0x80000000 $(DBG) $(OPT)
NATIVE_CPP  = CC
GNU_CPP     = g++
GNU_CC      = gcc
CFLAGS      = $(DBG) $(OPT) ##--save-temps
CPP         = $(GNU_CPP)
CC          = $(GNU_CC)
EXTRA_CFLGS =
AR          = ar ruv
RANLIB      = ranlib

## batman
#EXTRA_LIBS  = -L/scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes/
#EXTRA_INC   = -I/scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes
#EXTRA_LIBS   = -lopcodes -lbfd -liberty

## ranger
#EXTRA_LIBS  = /share/apps/binutils-amd/070220/lib64/libopcodes.a
#EXTRA_INC   = -I/share/apps/binutils-amd/070220/include/

INCLUDE     = ../include
SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = Base.C ElfFile.C BinaryFile.C FileHeader.C ProgramHeader.C SectionHeader.C \
              StringTable.C SymbolTable.C RelocationTable.C RawSection.C Disassembler.C \
              Instruction.C GlobalOffsetTable.C DynamicTable.C HashTable.C NoteSection.C main.C
OBJS        = Base.o ElfFile.o BinaryFile.o FileHeader.o ProgramHeader.o SectionHeader.o \
              StringTable.o SymbolTable.o RelocationTable.o RawSection.o Disassembler.o \
              Instruction.o GlobalOffsetTable.o DynamicTable.o HashTable.o NoteSection.o main.o

TARGET_BIN  = x86inst

all: $(TARGET_BIN)

$(TARGET_BIN) : $(OBJS)
	$(CPP) -o $@ $(OBJS) -I$(INCLUDE) $(EXTRA_LIBS) -Wl,--hash-style=sysv

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

%.o: %.c
	$(CC)  $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM -I$(INCLUDE) $(SRCS) > DEPENDS

include DEPENDS
