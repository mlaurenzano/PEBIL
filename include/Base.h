#ifndef _Base_h_
#define _Base_h_

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <CStructuresElf.h>

#define __MAX_STRING_SIZE 1024
#define __SHOULD_NOT_ARRIVE assert(0 && "Should not be called")

#define GET_FIELD_BASIS(__type,__field) virtual __type get_ ## __field() \
                                        { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define GET_FIELD_CLASS(__type,__field) inline __type get_ ## __field() \
                                        { return (__type)(entry.__field) ; }
#define GET_FIELD_BASIS_U(__type,__field,__union) virtual __type get_ ## __field() \
                                        { __SHOULD_NOT_ARRIVE; return ( __type )0; }
#define GET_FIELD_CLASS_U(__type,__field,__union) inline __type get_ ## __field() \
                                        { return (__type)(entry.__union.__field) ; }
#define GET_FIELD_BASIS_A(__type,__field,__union) virtual __type get_ ## __union ## _ ## __field() \
                                        { __SHOULD_NOT_ARRIVE; return ( __type )0; }

#define GET_FIELD_CLASS_A(__type,__field,__union) inline __type get_ ## __union ## _ ## __field() 

#define GET(__field) get_ ## __field()
#define GET_A(__field,__union) get_ ## __union ## _ ## __field()

#define PRINT_ERROR(...) fprintf(stderr,"*********** ERROR : "); \
                         fprintf(stderr,## __VA_ARGS__); \
                         fprintf(stderr,"\n"); \
                         exit(-1);

#define PRINT_INFOR(...) fprintf(stdout,"Information : "); \
                         fprintf(stdout,## __VA_ARGS__); \
                         fprintf(stdout,"\n"); \
                         fflush(stdout);

//#define DEVELOPMENT
#ifdef  DEVELOPMENT


#define PRINT_DEBUG(...) fprintf(stdout,"----------- DEBUG : "); \
                         fprintf(stdout,## __VA_ARGS__); \
                         fprintf(stdout,"\n");

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


#define Size__32_bit_ExceptionTable_Entry     EXCEPTSZ
#define Size__64_bit_ExceptionTable_Entry     EXCEPTSZ_64
#define Size__32_bit_LineInfoTable_Entry     LINESZ
#define Size__64_bit_LineInfoTable_Entry     LINESZ_64

#define Size__32_bit_Loader_Section_Header LDHDRSZ
#define Size__64_bit_Loader_Section_Header LDHDRSZ_64
#define Size__32_bit_Loader_Section_Symbol LDSYMSZ
#define Size__64_bit_Loader_Section_Symbol LDSYMSZ_64
#define Size__32_bit_Loader_Section_Relocation LDRELSZ
#define Size__64_bit_Loader_Section_Relocation LDRELSZ_64

#define Type__Auxilary_Symbol_No_Type 0
#define Type__Auxilary_Symbol_Section 1
#define Type__Auxilary_Symbol_Exception _AUX_EXCEPT
#define Type__Auxilary_Symbol_Function _AUX_FCN
#define Type__Auxilary_Symbol_Block _AUX_SYM
#define Type__Auxilary_Symbol_File _AUX_FILE
#define Type__Auxilary_Symbol_CSect _AUX_CSECT

class VersionTag {
protected:
	static char* tag;

public:
	static char* getLibraryVersionTag() { return tag; }
};

typedef enum {
    ElfSectType_undefined = 0,
    ElfSectType_strtab,
    ElfSectType_symtab,
    ElfSectType_reltab,
    ElfSectType_dwarf,
    ElfSectType_Total_Types
} ElfSectType;


typedef enum {
    ElfRelType_undefined = 0,
    ElfRelType_rel,
    ElfRelType_rela,
    ElfRelType_Total_Types
} ElfRelType;

typedef enum {
    ElfClassTypes_no_type = 0,
    ElfClassTypes_file_header,
    ElfClassTypes_program_header,
    ElfClassTypes_sect_header,
    ElfClassTypes_sect_rawdata,
    ElfClassTypes_relocation_table,
    ElfClassTypes_line_info,
    ElfClassTypes_symbol_table,
    ElfClassTypes_string_table,
    ElfClassTypes_dwarf_section,
    ElfClassTypes_text_section,
    ElfClassTypes_Total_Types
} ElfClassTypes;


class BinaryInputFile;
class BinaryOutputFile;
//class Instruction;

class Base {
protected:
    const static uint32_t invalidOffset = 0xffffffff;

    uint8_t type;
    uint32_t sizeInBytes;
    uint32_t fileOffset;

    Base() : type(ElfClassTypes_no_type),sizeInBytes(0),fileOffset(invalidOffset) {}
    Base(uint8_t t) : type(t),sizeInBytes(0),fileOffset(invalidOffset) {}
    virtual ~Base() {}

public:
    uint8_t getType() { return type; }
    uint32_t getSizeInBytes() { return sizeInBytes; }

    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t read(BinaryInputFile* b) { return 0; }


    uint32_t getFileOffset() { return fileOffset; }
    uint32_t setFileOffset(uint32_t offset) { fileOffset = offset; return (fileOffset+sizeInBytes);}
    bool hasInvalidFileOffset() { return (invalidOffset == fileOffset); }

    virtual const char* briefName() { __SHOULD_NOT_ARRIVE; return NULL; }

    bool includesFileOffset(uint32_t offset);
};

class HashCode {
private:
    typedef union {
        struct {
            uint32_t res     : 8;
            uint32_t section : 8;
            uint32_t function: 16;
            uint32_t block   : 16;
            uint32_t memop   : 16;
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

#define FIRST_HALFWORD(__n) ((__n) & 0xffff)
#define SECOND_HALFWORD(__n) (((__n) >> 16) & 0xffff)

extern double timer();
#endif
