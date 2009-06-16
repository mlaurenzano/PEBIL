DBG         = -g
#PROFILE     = -pg
OPT         = -O0
#CFLAGS      = -bmaxdata:0x80000000 $(DBG) $(OPT)
NATIVE_CPP  = CC
GNU_CPP     = g++
GNU_CC      = gcc
CFLAGS      = $(DBG) $(OPT) $(PROFILE)##--save-temps
CPP         = $(GNU_CPP)
CC          = $(GNU_CC)
EXTRA_CFLGS =
AR          = ar rus
RANLIB      = ranlib


INCLUDE     = -I../include -I../external/udis86-1.7/
SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS  = AddressAnchor.C Base.C BasicBlock.C BasicBlockCounter.C BinaryFile.C Disassembler.C DynamicTable.C DwarfSection.C \
	ElfFile.C ElfFileInst.C FileHeader.C FlowGraph.C Function.C FunctionCounter.C GlobalOffsetTable.C \
	GnuVersion.C HashTable.C Instruction.C InstructionGenerator.C Instrumentation.C LengauerTarjan.C LineInformation.C Loop.C \
	MemTrack.C NoteSection.C ProgramHeader.C RawSection.C RelocationTable.C SectionHeader.C StringTable.C \
	SymbolTable.C TextSection.C Udis.C
OBJS  = AddressAnchor.o Base.o BasicBlock.o BasicBlockCounter.o BinaryFile.o Disassembler.o DynamicTable.o DwarfSection.o \
	ElfFile.o ElfFileInst.o FileHeader.o FlowGraph.o Function.o FunctionCounter.o GlobalOffsetTable.o \
	GnuVersion.o HashTable.o Instruction.o InstructionGenerator.o Instrumentation.o LengauerTarjan.o LineInformation.o Loop.o \
	MemTrack.o NoteSection.o ProgramHeader.o RawSection.o RelocationTable.o SectionHeader.o StringTable.o \
	SymbolTable.o TextSection.o Udis.o

TARGET_LIB  = libx86inst.a

all: $(TARGET_LIB) install

$(TARGET_LIB) : $(OBJS)
	$(AR) $@ $(OBJS)
#	$(CPP) $(CFLAGS) -o $@ $(OBJS) $(CFLAGS) $(INCLUDE) $(EXTRA_LIBS) $(X86INST_LD_FLAGS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

%.o: %.c
	$(CC)  $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.static core *.o *.ii *.s *.class $(TARGET_LIB) $(BINDIR)/$(TARGET_LIB)

depend:
	g++ -E -MM $(INCLUDE) $(SRCS) > DEPENDS

install:
	cp $(TARGET_LIB) $(BINDIR)

include DEPENDS
