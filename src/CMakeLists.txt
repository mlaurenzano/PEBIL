project(pebil_lib CXX)
cmake_minimum_required(VERSION 2.6)
if (POLICY CMP0015)
   cmake_policy(SET CMP0015 NEW)
endif (POLICY CMP0015)

#set(pebil_SOURCES AddressAnchor.C Base.C BasicBlock.C BinaryFile.C DynamicTable.C DwarfSection.C ElfFile.C ElfFileInst.C FileHeader.C FlowGraph.C Function.C GlobalOffsetTable.C GnuVersion.C HashTable.C Instrumentation.C LengauerTarjan.C LineInformation.C Loop.C MemTrack.C NoteSection.C ProgramHeader.C RawSection.C RelocationTable.C SectionHeader.C StringTable.C SymbolTable.C TextSection.C X86Instruction.C X86InstructionFactory.C)

# For a large number of source files you can create it in a simpler way
file(GLOB pebilinst_SOURCES *.C)
include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/external/udis86-1.7/")

add_library(pebilinst SHARED ${pebilinst_SOURCES})
install(TARGETS pebilinst DESTINATION "${CMAKE_BINARY_DIR}/lib")
