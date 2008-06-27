#ifndef _ProgramHeader_h_
#define _ProgramHeader_h_

#include <Base.h>
#include <defines/ProgramHeader.d>

class ProgramHeader : public Base {
protected:
    ProgramHeader() : Base(ElfClassTypes_program_header),index(0) {}
    virtual ~ProgramHeader() {}

    uint16_t index;
public:

    PROGRAMHEADER_MACROS_BASIS("For the get_X field macros check the defines directory");

    void print();
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    bool isReadable() { return ISPF_R(GET(p_flags)); }
    bool isWritable() { return ISPF_W(GET(p_flags)); } 
    bool isExecutable() { return ISPF_X(GET(p_flags)); } 

    bool inRange(uint64_t addr);
    virtual void setOffset(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }
    virtual void setVirtualAddress(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }
    virtual void setPhysicalAddress(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }
    virtual void setMemorySize(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }
    virtual void setFileSize(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }
    virtual void setFlags(uint64_t newVal) { __SHOULD_NOT_ARRIVE; }

    const char* briefName() { return "ProgramHeader"; }
    bool verify();
};

class ProgramHeader32 : public ProgramHeader {
protected:
    Elf32_Phdr entry;

public:

    PROGRAMHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");

    ProgramHeader32(uint32_t idx) { sizeInBytes = Size__32_bit_Program_Header; index = idx; }
    ~ProgramHeader32() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool inRange(uint64_t addr);
    void setOffset(uint64_t newVal);
    void setVirtualAddress(uint64_t newVal);
    void setPhysicalAddress(uint64_t newVal);
    void setMemorySize(uint64_t newVal);
    void setFileSize(uint64_t newVal);
    void setFlags(uint64_t newVal);
};

class ProgramHeader64 : public ProgramHeader {
protected:
    Elf64_Phdr entry;

public:

    PROGRAMHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");

    ProgramHeader64(uint32_t idx) { sizeInBytes = Size__64_bit_Program_Header; index = idx; }
    ~ProgramHeader64() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool inRange(uint64_t addr);
    void setOffset(uint64_t newVal);
    void setVirtualAddress(uint64_t newVal);
    void setPhysicalAddress(uint64_t newVal);
    void setMemorySize(uint64_t newVal);
    void setFileSize(uint64_t newVal);
    void setFlags(uint64_t newVal);
};


#endif
