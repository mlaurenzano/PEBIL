#ifndef _Base_h_
#define _Base_h_

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <CStructuresElf.h>
#include <CStructuresDwarf.h>

typedef void (*fprintf_ftype)(FILE*, const char*, ...);

// debugging macros -- these can produce copious amounts of output

#define WARNING_SEVERITY 10
//#define DEVELOPMENT
//#define DEBUG_MEMTRACK
//#define DEBUG_OPERAND
//#define DEBUG_OPTARGET
//#define DEBUG_OPCODE
//#define DEBUG_HASH
//#define DEBUG_NOTE
//#define DEBUG_LINEINFO
//#define DEBUG_BASICBLOCK
//#define DEBUG_HASHCODE
//#define DEBUG_CFG
//#define DEBUG_LOOP
//#define DEBUG_INST
//#define DEBUG_ANCHOR
//#define DEBUG_FUNC_RELOC
//#define DEBUG_JUMP_TABLE
//#define DEBUG_POINT_CHAIN
//#define DEBUG_LEAF_OPT
//#define DEBUG_DATA_PLACEMENT
//#define DEBUG_ADDR_ALIGN

#define __MAX_STRING_SIZE 1024
#define __SHOULD_NOT_ARRIVE ASSERT(0 && "Control should not reach this point")
#define __FUNCTION_NOT_IMPLEMENTED PRINT_ERROR("Function %s in file %s not implemented", __FUNCTION__, __FILE__); __SHOULD_NOT_ARRIVE;

#define GET_FIELD_BASIS(__type,__field) virtual __type get_ ## __field() \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define GET_FIELD_CLASS(__type,__field) inline __type get_ ## __field() \
    { return (__type)(entry.__field); }
#define GET_FIELD_BASIS_A(__type,__field,__union) virtual __type get_ ## __union ## _ ## __field() \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define GET_FIELD_CLASS_A(__type,__field,__union) inline __type get_ ## __union ## _ ## __field() \
    { return (__type)(entry.__union.__field); }
#define GET(__field) get_ ## __field()
#define GET_A(__field,__union) get_ ## __union ## _ ## __field()


#define SET_FIELD_BASIS(__type,__field) virtual __type set_ ## __field(__type __value) \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define SET_FIELD_CLASS(__type,__field) inline __type set_ ## __field(__type __value) \
    { entry.__field = __value; return (__type)(entry.__field); }
#define SET_FIELD_BASIS_A(__type,__field,__union) virtual __type set_ ## __union ## _ ## __field(__type __value) \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define SET_FIELD_CLASS_A(__type,__field,__union) inline __type set_ ## __union ## _ ## __field(__type __value) \
    { entry.__union.__field = __value; return (__type)(entry.__union.__field); }
#define SET(__field,__value) set_ ## __field(__value)
#define SET_A(__field,__union,__value) set_ ## __union ## _ ## __field(__value)


#define INCREMENT_FIELD_BASIS(__type,__field) virtual __type increment_ ## __field(__type __value) \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define INCREMENT_FIELD_CLASS(__type,__field) inline __type increment_ ## __field(__type __value) \
    { entry.__field += __value; return (__type)(entry.__field); }
#define INCREMENT_FIELD_BASIS_A(__type,__field,__union) virtual __type increment_ ## __union ## _ ## __field(__type __value) \
    { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define INCREMENT_FIELD_CLASS_A(__type,__field,__union) inline __type increment_ ## __union ## _ ## __field(__type __value) \
    { entry.__union.__field += __value; return (__type)(entry.__union.__field); }
#define INCREMENT(__field,__value) increment_ ## __field(__value)
#define INCREMENT_A(__field,__union,__value) increment_ ## __union ## _ ## __field(__value)


#define PRINT_ERROR(...) fprintf(stderr,"*********** ERROR : "); \
    fprintf(stderr, "At line %d in file %s, function %s\n", __LINE__, __FILE__,__FUNCTION__);    \
    fprintf(stderr,## __VA_ARGS__);                              \
    fprintf(stderr,"\n");                                \
    ASSERT(0); \
    exit(-1);

#ifdef WARNING_SEVERITY
#define WARN_FILE stderr
#define PRINT_WARN(__severity,...)  if (__severity >= WARNING_SEVERITY){ \
    fprintf(WARN_FILE,"*** WARNING : ");                            \
    fprintf(WARN_FILE,## __VA_ARGS__);                              \
    fprintf(WARN_FILE,"\n");                                        \
    fflush(WARN_FILE); }
#else
#define PRINT_WARN(...)
#endif

#define PRINT_INFOR(...) fprintf(stdout,"Information : "); \
    fprintf(stdout,## __VA_ARGS__);                        \
    fprintf(stdout,"\n");                                  \
    fflush(stdout);

#define PRINT_INFO() fprintf(stdout,"Information : "); \
    fflush(stdout);

#define PRINT_OUT(...) fprintf(stdout,## __VA_ARGS__); \
    fflush(stdout);

#ifdef DEBUG_MEMTRACK
#include <MemTrack.h>
#define PRINT_DEBUG_MEMTRACK(...) fprintf(stdout,"MEMTRACK : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#define PRINT_MEMTRACK_STATS(...) \
    PRINT_DEBUG_MEMTRACK("-----------------------------------------------------------"); \
    PRINT_DEBUG_MEMTRACK("Memory Stats @ line %d in file %s in function %s", ## __VA_ARGS__); \
    MemTrack::TrackListMemoryUsage((double)1.00); \
    PRINT_DEBUG_MEMTRACK("-----------------------------------------------------------");

#else
#define PRINT_DEBUG_MEMTRACK(...)
#define PRINT_MEMTRACK_STATS(...)
#endif

#ifdef DEBUG_OPCODE
#define PRINT_DEBUG_OPCODE(...) fprintf(stdout,"OPCODE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPCODE(...)
#endif

#ifdef DEBUG_OPERAND
#define PRINT_DEBUG_OPERAND(...) fprintf(stdout,"OPERAND : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPERAND(...)
#endif

#ifdef DEBUG_OPTARGET
#define PRINT_DEBUG_OPTARGET(...) fprintf(stdout,"OPTARGET : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_OPTARGET(...)
#endif

#ifdef DEBUG_HASH
#define PRINT_DEBUG_HASH(...) fprintf(stdout,"HASH : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_HASH(...)
#endif

#ifdef DEBUG_LINEINFO
#define PRINT_DEBUG_LINEINFO(...) fprintf(stdout,"LINEINFO : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_LINEINFO(...)
#endif

#ifdef DEBUG_BASICBLOCK
#define PRINT_DEBUG_BASICBLOCK(...) fprintf(stdout,"BASICBLOCK : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_BASICBLOCK(...)
#endif

#ifdef DEBUG_HASHCODE
#define PRINT_DEBUG_HASHCODE(...) fprintf(stdout,"HASHCODE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_HASHCODE(...)
#endif

#ifdef DEBUG_CFG
#define PRINT_DEBUG_CFG(...) fprintf(stdout,"CFG : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_CFG(...)
#endif

#ifdef DEBUG_LOOP
#define PRINT_DEBUG_LOOP(...) fprintf(stdout,"LOOP : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_LOOP(...)
#endif

#ifdef DEBUG_INST
#define PRINT_DEBUG_INST(...) fprintf(stdout,"INST : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_INST(...)
#endif

#ifdef DEBUG_ANCHOR
#define PRINT_DEBUG_ANCHOR(...) fprintf(stdout,"ANCHOR : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_ANCHOR(...)
#endif

#ifdef DEBUG_FUNC_RELOC
#define PRINT_DEBUG_FUNC_RELOC(...) fprintf(stdout,"FUNC_RELOC : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_FUNC_RELOC(...)
#endif

#ifdef DEBUG_JUMP_TABLE
#define PRINT_DEBUG_JUMP_TABLE(...) fprintf(stdout,"JUMP_TABLE : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_JUMP_TABLE(...)
#endif

#ifdef DEBUG_POINT_CHAIN
#define PRINT_DEBUG_POINT_CHAIN(...) fprintf(stdout,"POINT_CHAIN : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_POINT_CHAIN(...)
#endif

#ifdef DEBUG_LEAF_OPT
#define PRINT_DEBUG_LEAF_OPT(...) fprintf(stdout,"LEAF_OPT : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_LEAF_OPT(...)
#endif

#ifdef DEBUG_DATA_PLACEMENT
#define PRINT_DEBUG_DATA_PLACEMENT(...) fprintf(stdout,"DATA_PLACEMENT : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_DATA_PLACEMENT(...)
#endif


#ifdef DEBUG_ADDR_ALIGN
#define PRINT_DEBUG_ADDR_ALIGN(...) fprintf(stdout,"ADDR_ALIGN : "); \
    fprintf(stdout,## __VA_ARGS__); \
    fprintf(stdout,"\n"); \
    fflush(stdout);
#else
#define PRINT_DEBUG_ADDR_ALIGN(...)
#endif



#ifdef  DEVELOPMENT

#define PRINT_DEBUG(...) fprintf(stdout,"----------- DEBUG : "); \
                         fprintf(stdout,## __VA_ARGS__); \
                         fprintf(stdout,"\n"); \
                         fflush(stdout);


#define ASSERT(__str) assert(__str);
#define DEBUG(...) __VA_ARGS__
#define DEBUG_MORE(...)
#define TIMER(...) __VA_ARGS__
#define INNER_TIMER(...) __VA_ARGS__

#else

#define PRINT_DEBUG(...)
#define DEBUG(...)
#define DEBUG_MORE(...)
#define ASSERT(__str) assert(__str);
#define TIMER(...) __VA_ARGS__
#define INNER_TIMER(...) 

#endif

#define MINIMUM(I,J) ((I) < (J) ? : (I) : (J))


#define Invalid_UInteger_ID                 (uint32_t)-1
#define Size__32_bit_File_Header            sizeof(Elf32_Ehdr)
#define Size__64_bit_File_Header            sizeof(Elf64_Ehdr)
#define Size__32_bit_Program_Header         sizeof(Elf32_Phdr)
#define Size__64_bit_Program_Header         sizeof(Elf64_Phdr)
#define Size__32_bit_Section_Header         sizeof(Elf32_Shdr)
#define Size__64_bit_Section_Header         sizeof(Elf64_Shdr)
#define Size__32_bit_Symbol                 sizeof(Elf32_Sym)
#define Size__64_bit_Symbol                 sizeof(Elf64_Sym)
#define Size__32_bit_Relocation             sizeof(Elf32_Rel)
#define Size__64_bit_Relocation             sizeof(Elf64_Rel)
#define Size__32_bit_Relocation_Addend      sizeof(Elf32_Rela)
#define Size__64_bit_Relocation_Addend      sizeof(Elf64_Rela)
#define Size__32_bit_Global_Offset_Table_Entry sizeof(uint32_t)
#define Size__64_bit_Global_Offset_Table_Entry sizeof(uint64_t)
#define Size__32_bit_Dynamic_Entry          sizeof(Elf32_Dyn)
#define Size__64_bit_Dynamic_Entry          sizeof(Elf64_Dyn)
#define Size__32_bit_Hash_Entry             sizeof(uint32_t)
#define Size__64_bit_Hash_Entry             sizeof(uint32_t)
#define Size__32_bit_GNU_Hash_Bloom_Entry   sizeof(uint32_t)
#define Size__64_bit_GNU_Hash_Bloom_Entry   sizeof(uint64_t)
#define Size__32_bit_Note_Section_Entry     sizeof(uint32_t)
#define Size__64_bit_Note_Section_Entry     sizeof(uint32_t)
#define Size__32_bit_Gnu_Verneed            sizeof(Elf32_Verneed)
#define Size__64_bit_Gnu_Verneed            sizeof(Elf64_Verneed)
#define Size__32_bit_Gnu_Vernaux            sizeof(Elf32_Vernaux)
#define Size__64_bit_Gnu_Vernaux            sizeof(Elf64_Vernaux)
#define Size__32_bit_Gnu_Versym             sizeof(uint16_t)
#define Size__64_bit_Gnu_Versym             sizeof(uint16_t)
#define Size__Dwarf_LineInfo_Header         sizeof(DWARF2_Internal_LineInfo)
#define Size__Dwarf_LineInfo                sizeof(DWARF2_LineInfo_Registers)


#define Print_Code_All                      0x00000001
#define Print_Code_FileHeader               0x00000002
#define Print_Code_SectionHeader            0x00000004
#define Print_Code_ProgramHeader            0x00000008
#define Print_Code_StringTable              0x00000010
#define Print_Code_SymbolTable              0x00000020
#define Print_Code_NoteSection              0x00000040
#define Print_Code_RelocationTable          0x00000080
#define Print_Code_GlobalOffsetTable        0x00000100
#define Print_Code_HashTable                0x00000200
#define Print_Code_DynamicTable             0x00000400
#define Print_Code_GnuVersymTable           0x00000800
#define Print_Code_GnuVerneedTable          0x00001000
#define Print_Code_Disassemble              0x00002000
#define Print_Code_Instruction              0x00004000
#define Print_Code_Instrumentation          0x00008000
#define Print_Code_DwarfSection             0x00010000
#define Print_Code_Loops                    0x00020000

#define HAS_PRINT_CODE(__value,__Print_Code) ((__value & __Print_Code) || (__value & Print_Code_All))
#define SET_PRINT_CODE(__value,__Print_Code) (__value |= __Print_Code)

typedef enum {
    TableMode_undefined = 0,
    TableMode_direct,
    TableMode_indirect,
    TableMode_instructions,
    TableMode_Total_Types
} TableModes;

typedef enum {
    BloatType_undefined = 0,
    BloatType_BasicBlock,
    BloatType_MemoryInstruction,
    BloatType_Total_Types
} BloatTypes;

typedef enum {
    InstLocation_dont_care = 0,
    InstLocation_prior,
    InstLocation_after,
    InstLocation_exact,
    InstLocation_Total_Types
} InstLocations;

typedef enum {
    DebugFormat_undefined = 0,
    DebugFormat_DWARF2_32bit,
    DebugFormat_DWARF2_64bit,
    DebugFormat_Total_Types
} DebugFormats;

typedef enum {
    ElfRelType_undefined = 0,
    ElfRelType_rel,
    ElfRelType_rela,
    ElfRelType_Total_Types
} ElfRelTypes;

typedef enum {
    PebilClassType_no_type = 0, // 0
    PebilClassType_BasicBlock,
    PebilClassType_CodeBlock,
    PebilClassType_DataReference,
    PebilClassType_DataSection,
    PebilClassType_DwarfSection, // 5
    PebilClassType_DwarfLineInfoSection,
    PebilClassType_Dynamic,
    PebilClassType_DynamicTable,
    PebilClassType_FileHeader,
    PebilClassType_FreeText, // 10
    PebilClassType_Function,
    PebilClassType_GlobalOffsetTable,
    PebilClassType_GnuVerneed,
    PebilClassType_GnuVerneedTable,
    PebilClassType_GnuVersym, // 15
    PebilClassType_GnuVersymTable,
    PebilClassType_HashTable,
    PebilClassType_Instruction,
    PebilClassType_InstrumentationFunction,
    PebilClassType_InstrumentationPoint, // 20
    PebilClassType_InstrumentationSnippet,
    PebilClassType_Note,
    PebilClassType_NoteSection,
    PebilClassType_ProgramHeader,
    PebilClassType_RawBlock, // 25
    PebilClassType_RawSection,
    PebilClassType_RelocationTable,
    PebilClassType_Relocation,
    PebilClassType_SectionHeader, 
    PebilClassType_StringTable, // 30
    PebilClassType_Symbol,
    PebilClassType_SymbolTable,
    PebilClassType_TextSection,
    PebilClassType_Total_Types
} PebilClassTypes;

typedef enum {
    ByteSource_no_source = 0,
    ByteSource_Application,
    ByteSource_Application_FreeText,
    ByteSource_Application_Function,
    ByteSource_Instrumentation,
    ByteSource_Total_Types
} ByteSources;

#define IS_BYTE_SOURCE_APPLICATION(__src) \
    ((__src == ByteSource_Application) || \
     (__src == ByteSource_Application_FreeText) || \
     (__src == ByteSource_Application_Function))

class BinaryInputFile;
class BinaryOutputFile;
class Instruction;
template <class anonymous> class Vector;

class Base {
protected:
    const static uint32_t invalidOffset = 0xffffffff;

    PebilClassTypes type;
    uint32_t sizeInBytes;
    uint32_t fileOffset;

    Base() : type(PebilClassType_no_type),sizeInBytes(0),fileOffset(invalidOffset),baseAddress(0) {}
    Base(PebilClassTypes t) : type(t),sizeInBytes(0),fileOffset(invalidOffset),baseAddress(0) {}
    virtual ~Base() {}

public:
    uint64_t baseAddress;

    PebilClassTypes getType() { return type; }
    uint32_t getSizeInBytes() { return sizeInBytes; }

    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t read(BinaryInputFile* b) { __SHOULD_NOT_ARRIVE; return 0; }


    uint32_t getFileOffset() { return fileOffset; }
    void setFileOffset(uint32_t offset) { fileOffset = offset; }
    bool hasInvalidFileOffset() { return (invalidOffset == fileOffset); }

    bool includesFileOffset(uint32_t offset);


    bool containsProgramBits() { return (type == PebilClassType_Instruction             || 
                                         type == PebilClassType_BasicBlock              || 
                                         type == PebilClassType_Function                || 
                                         type == PebilClassType_TextSection             ||
                                         type == PebilClassType_InstrumentationSnippet  ||
                                         type == PebilClassType_InstrumentationFunction ||
                                         type == PebilClassType_DataReference
                                         ); }
    virtual Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements) { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual uint64_t findInstrumentationPoint(uint32_t size, InstLocations loc) { __SHOULD_NOT_ARRIVE; return 0; }
    virtual uint64_t getBaseAddress() { __SHOULD_NOT_ARRIVE; }
};

class HashCode {
private:
    typedef union {
        struct {
            uint32_t memop   : 16;
            uint32_t block   : 16;
            uint32_t function: 16;
            uint32_t section : 8;
            uint32_t res     : 8;
        } fields;
        uint64_t bits;
    } HashCodeEntry;

    const static uint64_t INVALID_FIELD = 0;

    HashCodeEntry entry;

    inline bool hasSection()       { return (entry.fields.section   != INVALID_FIELD); }
    inline bool hasFunction()      { return (entry.fields.function  != INVALID_FIELD); }
    inline bool hasBlock()         { return (entry.fields.block     != INVALID_FIELD); }
    inline bool hasMemop()         { return (entry.fields.memop     != INVALID_FIELD); }

    inline static bool validSection(uint32_t s)    { return ((0 <= s) && (s < (0x1 << 8))); }
    inline static bool validFunction(uint32_t f){ return ((0 <= f) && (f < ((0x1 << 16) - 1))); }
    inline static bool validBlock(uint32_t b)    { return ((0 <= b) && (b < ((0x1 << 16) - 1))); }
    inline static bool validMemop(uint32_t m)    { return ((0 <= m) && (m < ((0x1 << 16) - 1))); }
public:

    inline uint64_t getValue(){ return entry.bits; }

    inline HashCode() { entry.bits = INVALID_FIELD; }
    inline HashCode(uint64_t a) { entry.bits = a; }

    HashCode(uint32_t s);
    HashCode(uint32_t s,uint32_t f);
    HashCode(uint32_t s,uint32_t f,uint32_t b);
    HashCode(uint32_t s,uint32_t f,uint32_t b,uint32_t m);

    inline bool isSection()  { return (hasSection() && !hasFunction() && !hasBlock() && !hasMemop()); }
    inline bool isFunction() { return (hasSection() && hasFunction() && !hasBlock() && !hasMemop()); }
    inline bool isBlock()    { return (hasSection() && hasFunction() && hasBlock() && !hasMemop()); }
    inline bool isMemop()    { return (hasSection() && hasFunction() && hasBlock() && hasMemop()); }
    inline bool isValid()    { return (isSection() || isFunction() || isBlock() || isMemop()); }

    inline uint32_t getSection()  { return entry.fields.section; }
    inline uint32_t getFunction() { return (hasFunction() ? (entry.fields.function - 1) : INVALID_FIELD); }
    inline uint32_t getBlock()      { return (hasBlock() ? (entry.fields.block - 1) : INVALID_FIELD); }
    inline uint32_t getMemop()    { return (hasMemop() ? (entry.fields.memop - 1) : INVALID_FIELD); }
};


extern bool isAddressAligned(uint64_t addr, uint32_t align);
extern bool isPowerOfTwo(uint32_t n);
extern uint64_t nextAlignAddress(uint64_t addr, uint32_t align);
extern uint64_t nextAlignAddressHalfWord(uint64_t addr);
extern uint64_t nextAlignAddressWord(uint64_t addr);
extern uint64_t nextAlignAddressDouble(uint64_t addr);

extern int compareBaseAddress(const void* arg1,const void* arg2);
extern int searchBaseAddressExact(const void* arg1, const void* arg2);
extern int searchBaseAddress(const void* arg1, const void* arg2);

extern uint64_t getUInt64(char* buf);
extern uint32_t getUInt32(char* buf);
extern uint16_t getUInt16(char* buf);
extern int64_t absoluteValue(uint64_t d);

extern int32_t scmp(const void *a, const void *b);

extern char mapCharsToByte(char c1, char c2);
extern void printBufferPretty(char* buff, uint32_t sizeInBytes, uint64_t baseAddress, uint32_t bytesPerWord, uint32_t bytesPerLine);

#define FIRST_HALFWORD(__n) ((__n) & 0xffff)
#define SECOND_HALFWORD(__n) (((__n) >> 16) & 0xffff)

extern double timer();
#endif
