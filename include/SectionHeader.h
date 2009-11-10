#ifndef _SectionHeader_h_
#define _SectionHeader_h_

#include <Base.h>
#include <defines/SectionHeader.d>

class ElfFile;

class SectionHeader : public Base {
protected:
    char* relocationPtr;
    char* lineInfoPointer;
    uint32_t numOfRelocations;
    uint32_t numOfLineInfo;

    char* sectionNamePtr;
    PebilClassTypes sectionType;

    uint16_t index;
protected:
    SectionHeader() : Base(PebilClassType_SectionHeader),
        relocationPtr(NULL), lineInfoPointer(NULL),
        numOfRelocations(0),numOfLineInfo(0),
        index(0),sectionNamePtr(NULL),sectionType(PebilClassType_no_type) {}

public:
    bool verify();

    ~SectionHeader() {}
    SECTIONHEADER_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    virtual char* charStream() { __SHOULD_NOT_ARRIVE; }

    void print();
    void initFilePointers(BinaryInputFile* b);

    bool hasWriteBit();
    bool hasAllocBit();
    bool hasExecInstrBit();
    bool isReadOnly() { return (!hasWriteBit() && hasAllocBit() && !hasExecInstrBit()); }
    virtual bool hasBitsInFile() { __SHOULD_NOT_ARRIVE; }


    void setSectionType();
    PebilClassTypes getSectionType() { return sectionType; }

    void setSectionNamePtr(char* namePtr) { sectionNamePtr = namePtr; }
    char* getSectionNamePtr() { return sectionNamePtr; }
    const char* getTypeName();
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }

    uint64_t getRawDataSize() { return GET(sh_size); }

    uint16_t getIndex() { return index; }
    bool inRange(uint64_t addr);

    void setIndex(uint16_t newidx) { index = newidx; }
};

class SectionHeader32 : public SectionHeader {
protected:
    Elf32_Shdr entry;

public:

    SECTIONHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    SectionHeader32(uint16_t idx);
    ~SectionHeader32() {}
    uint32_t read(BinaryInputFile* b);
    bool hasBitsInFile() { return (GET(sh_type) != SHT_NOBITS); }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    char* charStream() { return (char*)&entry; }
//    uint32_t instrument(char* buffer,ElfFileGen* xCoffGen,BaseGen* gen);
};

class SectionHeader64 : public SectionHeader {
protected:
    Elf64_Shdr entry;

public:

    SECTIONHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    SectionHeader64(uint16_t idx);
    ~SectionHeader64() {}
    uint32_t read(BinaryInputFile* b);
    bool hasBitsInFile() { return (GET(sh_type) != SHT_NOBITS); }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    char* charStream() { return (char*)&entry; }
//    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

#endif /* _SectionHeader_h_ */
