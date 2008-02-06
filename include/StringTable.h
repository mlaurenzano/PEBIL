#ifndef _StringTable_h_
#define _StringTable_h_

#include <Base.h>
#include <SectionHeader.h>

class SectionHeader;

class StringTable : public Base {
protected:
    char* stringTablePtr;
    uint32_t sizeInBytes;
    uint32_t index;
    SectionHeader* sectionHeader;
    StringTable() : Base(ElfClassTypes_string_table),stringTablePtr(NULL),sizeInBytes(0){}

public:

    StringTable(char* ptr, uint64_t size, uint32_t idx, SectionHeader* stSectionHeader) : Base(ElfClassTypes_string_table),
                stringTablePtr(ptr),sizeInBytes(size),index(idx),sectionHeader(stSectionHeader){}
    ~StringTable() {}

    char* getStringTablePtr() { return stringTablePtr; }
    uint64_t getStringTableSize() { return sizeInBytes; }
    SectionHeader* getSectionHeader() { return sectionHeader; }

    void print();
    uint32_t read(BinaryInputFile* b);

    char* getString(uint32_t offset);
    uint32_t getIndex() { return index; }
    uint32_t getSectionIndex() { ASSERT(sectionHeader); return sectionHeader->getIndex(); }

    const char* briefName() { return "StringTable"; }

//    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

#endif /* _StringTable_h_ */
