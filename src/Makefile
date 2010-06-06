DBG         = -g #-Wall
#PROFILE     = -pg
OPT         = -O2 -w
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


SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib
EXTDIR      = ../external
INCLUDE     = -I../include -I$(EXTDIR)/udis86-1.7/

SRCS  = AddressAnchor.C \
	Base.C \
	BasicBlock.C \
	BinaryFile.C \
	DynamicTable.C \
	DwarfSection.C \
	ElfFile.C \
	ElfFileInst.C \
	FileHeader.C \
	FlowGraph.C \
	Function.C \
	GlobalOffsetTable.C \
	GnuVersion.C \
	HashTable.C \
	Instrumentation.C \
	LengauerTarjan.C \
	LineInformation.C \
	Loop.C \
	MemTrack.C \
	NoteSection.C \
	ProgramHeader.C \
	RawSection.C \
	RelocationTable.C \
	SectionHeader.C \
	StringTable.C \
	SymbolTable.C \
	TextSection.C \
	X86Instruction.C \
	X86InstructionFactory.C

OBJS  = AddressAnchor.o \
	Base.o \
	BasicBlock.o \
	BinaryFile.o \
	DynamicTable.o \
	DwarfSection.o \
	ElfFile.o \
	ElfFileInst.o \
	FileHeader.o \
	FlowGraph.o \
	Function.o \
	GlobalOffsetTable.o \
	GnuVersion.o \
	HashTable.o \
	Instrumentation.o \
	LengauerTarjan.o \
	LineInformation.o \
	Loop.o \
	MemTrack.o \
	NoteSection.o \
	ProgramHeader.o \
	RawSection.o \
	RelocationTable.o \
	SectionHeader.o \
	StringTable.o \
	SymbolTable.o \
	TextSection.o \
	X86Instruction.o \
	X86InstructionFactory.o

TARGET_LIB  = libpebil.a 

all: $(TARGET_LIB) install pebil

pebil: $(TARGET_LIB)
	$(MAKE) -C ../tools

$(TARGET_LIB) : $(OBJS)
	$(AR) $@ $(OBJS)
#	$(CPP) $(CFLAGS) -o $@ $(OBJS) $(CFLAGS) $(INCLUDE) $(EXTRA_LIBS) $(PEBIL_LD_FLAGS)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

%.o: %.c
	$(CC)  $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< $(INCLUDE) $(EXTRA_INC)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f *~ *.ideinst *.fncinst *.jbbinst *.siminst *.ftminst *.static core *.o *.ii *.s *.class $(TARGET_LIB) $(BINDIR)/$(TARGET_LIB) pebil

depend:
	g++ -E -MM $(INCLUDE) $(SRCS) > DEPENDS

install:
	cp $(TARGET_LIB) $(BINDIR)

include DEPENDS
