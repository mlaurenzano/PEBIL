#ifndef _HashTable_h_
#define _HashTable_h_

#include <RawSection.h>

class ElfFile;

class HashTable : public RawSection {
protected:
    uint16_t symTabIdx;

    uint32_t numberOfBuckets;
    uint32_t* buckets;

    uint32_t numberOfEntries;
    uint32_t* entries;

    uint32_t hashEntrySize;

    virtual bool verify() { __SHOULD_NOT_ARRIVE; }
    uint32_t getBucket(uint32_t index);
    virtual void buildTable(uint32_t numEntries, uint32_t numBuckets) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t findSymbol(const char* symbolName) { __SHOULD_NOT_ARRIVE; }

public:
    HashTable(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~HashTable();

    uint32_t getNumberOfBuckets() { return numberOfBuckets; }
    void addEntry();
    uint32_t getEntry(uint32_t idx);
    uint32_t getNumberOfEntries() { return numberOfEntries; }


    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t read(BinaryInputFile* b) { __SHOULD_NOT_ARRIVE; }
    virtual void initFilePointers() { __SHOULD_NOT_ARRIVE; }
    bool isGnuStyleHash();
    uint32_t expandSize(uint32_t amt);
    uint32_t getEntrySize() { return hashEntrySize; }

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }

    virtual bool passedThreshold() { __SHOULD_NOT_ARRIVE; }
};

class GnuHashTable : public HashTable {
private:
    uint32_t numberOfBloomFilters;
    uint64_t* bloomFilters;
    uint32_t firstSymIndex;
    uint32_t shiftCount;

    bool verify();
    bool entryHasStopBit(uint32_t idx);
    bool matchesEntry(uint32_t idx, uint32_t val);
    uint32_t findSymbol(const char* symbolName);
    void buildTable(uint32_t numEntries, uint32_t numBuckets);

public:
    GnuHashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~GnuHashTable();
    uint32_t read(BinaryInputFile* b);
    void print();
    void initFilePointers();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool passedThreshold();
};

class SysvHashTable : public HashTable {
private:
    bool verify();
    uint32_t findSymbol(const char* symbolName);
    void buildTable(uint32_t numEntries, uint32_t numBuckets);

public:
    SysvHashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~SysvHashTable() {}

    void print();
    uint32_t read(BinaryInputFile* b);
    void initFilePointers();

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool passedThreshold();
};

#endif /* _HashTable_h_ */
