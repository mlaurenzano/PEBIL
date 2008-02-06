DBG         = -g
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

INCLUDE     = ../include 
SRCDIR      = ../src
BINDIR      = ../bin
LIBDIR      = ../lib

SRCS        = Base.C ElfFile.C BinaryFile.C FileHeader.C ProgramHeader.C SectionHeader.C \
              StringTable.C SymbolTable.C RelocationTable.C
OBJS        = Base.o ElfFile.o BinaryFile.o FileHeader.o ProgramHeader.o SectionHeader.o \
              StringTable.o SymbolTable.o RelocationTable.o

TARGET_BIN  = x86inst

all: $(TARGET_BIN)

$(TARGET_BIN) : $(OBJS)
	$(CPP) -o $@ main.C $(OBJS) $(EXTRA_LIBS) -I$(INCLUDE)

%.o: %.C
	$(CPP) $(CFLAGS) $(EXTRA_CFLGS) -c -o $@ $< -I$(INCLUDE)

# it used to be shared library and compiled with -G option $(CPP) -G -o $@ $(OBJS)
# but now it is static library that is included in executable 

clean: 
	rm -f core *.o *.ii *.s *.class $(TARGET_BIN)

depend:
	g++ -E -MM -I$(INCLUDE) $(SRCS) > DEPENDS

include DEPENDS
