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
    uint16_t symTabIdx;

    uint32_t numberOfBuckets;
    uint32_t numberOfChains;
    uint32_t numberOfBlooms;
    uint32_t* buckets;
    uint32_t* chains;
    uint64_t* blooms;

    uint32_t hashEntrySize;
    uint32_t bloomEntrySize;

    virtual bool verify();
    uint32_t getBucket(uint32_t index);
    uint32_t getNumberOfBuckets() { return numberOfBuckets; }

    uint32_t getChain(uint32_t index);
    uint32_t getNumberOfChains() { return numberOfChains; }

    uint64_t getBloom(uint32_t index);
    uint32_t getNumberOfBlooms() { return numberOfBlooms; }

    uint32_t findSymbolSysv(const char* symbolName);
    bool verifySysv();
    void dumpSysv(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    uint32_t readSysv(BinaryInputFile* b);
    void printSysv();
    
public:
    HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~HashTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    bool isGnuStyleHash();

    uint32_t findSymbol(const char* symbolName);

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    const char* briefName() { return "HashTable"; }
};

#endif /* _HashTable_h_ */
