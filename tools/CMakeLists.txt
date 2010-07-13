project(pebil_tools CXX)
cmake_minimum_required(VERSION 2.6)

#set(pebil_SOURCES AddressAnchor.C Base.C BasicBlock.C BinaryFile.C DynamicTable.C DwarfSection.C ElfFile.C ElfFileInst.C FileHeader.C FlowGraph.C Function.C GlobalOffsetTable.C GnuVersion.C HashTable.C Instrumentation.C LengauerTarjan.C LineInformation.C Loop.C MemTrack.C NoteSection.C ProgramHeader.C RawSection.C RelocationTable.C SectionHeader.C StringTable.C SymbolTable.C TextSection.C X86Instruction.C X86InstructionFactory.C)

add_definitions(-DPEBIL_VER="${PEBIL_FULL_VER}")

# For a large number of source files you can create it in a simpler way
file(GLOB pebil_SOURCES *.C)
include_directories("." "../include" "../external/udis86-1.7" "../instcode")
link_directories("../src/" "../external/udis86-1.7/libudis86/.libs")

add_executable(pebil ${pebil_SOURCES})
target_link_libraries(pebil "libpebilinst.so" "libudis86.a")

install(
  TARGETS pebil
  RUNTIME DESTINATION "bin"
)