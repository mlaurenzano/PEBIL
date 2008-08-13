#ifndef _GlobalOffsetTable_h_
#define _GlobalOffsetTable_h_

#include <Base.h>
#include <RawSection.h>
#include <ElfFile.h>
#include <SectionHeader.h>

class ElfFile;

class GlobalOffsetTable : public RawSection {
protected:
    uint32_t numberOfEntries;
    uint32_t entrySize;
    uint32_t tableBaseIdx;
    uint64_t baseAddress;

    uint64_t* entries;
public:
    GlobalOffsetTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint64_t gotSymAddr, ElfFile* elf);
    ~GlobalOffsetTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint64_t getEntry(uint32_t index);
    uint32_t getNumberOfEntries() { return numberOfEntries; }
    uint32_t minIndex() { return -1*tableBaseIdx; }
    uint32_t maxIndex() { return numberOfEntries-tableBaseIdx; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    const char* briefName() { return "GlobalOffsetTable"; }
};

#endif /* _GlobalOffsetTable_h_ */
