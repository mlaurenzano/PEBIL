#ifndef _HashTable_h_
#define _HashTable_h_

#include <RawSection.h>

class ElfFile;

class HashTable : public RawSection {
private:
    uint16_t symTabIdx;

    uint32_t numberOfBuckets;
    uint32_t numberOfChains;
    uint32_t* buckets;
    uint32_t* chains;

    uint32_t hashEntrySize;

    virtual bool verify();
    uint32_t getBucket(uint32_t index);
    uint32_t getChain(uint32_t index);
    void buildTable(uint32_t numChains, uint32_t numBuckets);
public:
    HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~HashTable();

    uint32_t getNumberOfBuckets() { return numberOfBuckets; }
    uint32_t getNumberOfChains() { return numberOfChains; }

    void print();
    uint32_t read(BinaryInputFile* b);
    void initFilePointers();
    bool isGnuStyleHash();
    void addChain();
    uint32_t expandSize(uint32_t amt);
    uint32_t getEntrySize() { return hashEntrySize; }

    uint32_t findSymbol(const char* symbolName);

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

#endif /* _HashTable_h_ */
