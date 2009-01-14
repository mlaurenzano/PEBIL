DBG         = -g
PROFILE     = #-pg
OPT         = -O0
#CFLAGS      = -bmaxdata:0x80000000 $(DBG) $(OPT)
NATIVE_CPP  = CC
GNU_CPP     = g++
GNU_CC      = gcc
CFLAGS      = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP         = $(GNU_CPP)
CC          = $(GNU_CC)
EXTRA_CFLGS =
AR          = ar ruv
RANLIB      = ranlib
LINKER_FLGS = -Wl,--hash-style=sysv

## batman
#EXTRA_LIBS = -L/scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes/
#EXTRA_INC  = -I/scratch/slocal/michaell/downloads/binutils-2.12.90/opcodes
#EXTRA_LIBS = -lopcodes -lbfd -liberty

## ranger
#EXTRA_LIBS = /share/apps/binutils-amd/070220/lib64/libopcodes.a
#EXTRA_INC  = -I/share/apps/binutils-amd/070220/include/

INCLUDE     = ../include
SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = Base.C BasicBlock.C BasicBlockCounter.C BinaryFile.C Disassembler.C DynamicTable.C DwarfSection.C \
	ElfFile.C ElfFileInst.C FileHeader.C FlowGraph.C Function.C FunctionCounter.C GlobalOffsetTable.C GnuVerneedTable.C \
	GnuVersymTable.C HashTable.C Instruction.C Instrumentation.C LengauerTarjan.C LineInformation.C Loop.C \
	NoteSection.C ProgramHeader.C RawSection.C RelocationTable.C SectionHeader.C StringTable.C \
	SymbolTable.C TextSection.C main.C
OBJS        = Base.o BasicBlock.o BasicBlockCounter.o BinaryFile.o Disassembler.o DynamicTable.o DwarfSection.o \
	ElfFile.o ElfFileInst.o FileHeader.o FlowGraph.o Function.o FunctionCounter.o GlobalOffsetTable.o GnuVerneedTable.o \
	GnuVersymTable.o HashTable.o Instruction.o Instrumentation.o LengauerTarjan.o LineInformation.o Loop.o \
	NoteSection.o ProgramHeader.o RawSection.o RelocationTable.o SectionHeader.o StringTable.o \
	SymbolTable.o TextSection.o main.o

TARGET_BIN  = x86inst

all: $(TARGET_BIN)

$(TARGET_BIN) : $(OBJS)
	$(CPP) -o $@ $(OBJS) $(CFLAGS) -I$(INCLUDE) $(EXTRA_LIBS) $(LINKER_FLGS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

%.o: %.c
	$(CC)  $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE) $(EXTRA_INC)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM -I$(INCLUDE) $(SRCS) > DEPENDS

include DEPENDS
