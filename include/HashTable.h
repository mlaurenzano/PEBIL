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
    void dump32(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    void dump64(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t read32(BinaryInputFile* binaryInputFile);
    uint32_t read64(BinaryInputFile* binaryInputFile);

    bool verify();

protected:
    uint16_t symTabIdx;

    uint64_t numberOfBuckets;
    uint64_t numberOfChains;

    uint64_t* bucket;
    uint64_t* chain;

    uint32_t hashEntrySize;
public:
    HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~HashTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint64_t getBucket(uint32_t index);
    uint64_t getChain(uint32_t index);
    uint64_t getNumberOfBuckets() { return numberOfBuckets; }
    uint64_t getNumberOfChains() { return numberOfChains; }

    uint64_t findSymbol(const char* symbolName);

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    const char* briefName() { return "HashTable"; }
};

#endif /* _HashTable_h_ */
