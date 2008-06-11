#ifndef _HashTable_h_
#define _HashTable_h_

#include <Base.h>
#include <RawSection.h>
#include <ElfFile.h>
#include <SectionHeader.h>
#include <SymbolTable.h>

class ElfFile;

class HashTable : public RawSection {
private:
    bool verify();

protected:
    uint16_t symTabIdx;

    uint32_t numberOfBuckets;
    uint32_t numberOfChains;

    uint32_t* bucket;
    uint32_t* chain;

    uint32_t hashEntrySize;
public:
    HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~HashTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint32_t getBucket(uint32_t index);
    uint32_t getChain(uint32_t index);
    uint32_t getNumberOfBuckets() { return numberOfBuckets; }
    uint32_t getNumberOfChains() { return numberOfChains; }

    uint32_t findSymbol(const char* symbolName);

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    const char* briefName() { return "HashTable"; }
};

#endif /* _HashTable_h_ */
