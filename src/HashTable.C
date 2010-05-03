#include <HashTable.h>

#include <ElfFile.h>
#include <SectionHeader.h>
#include <SymbolTable.h>

void GnuHashTable::buildTable(uint32_t numEntries, uint32_t numBuckets){
    if (!numberOfEntries){
        return;
    }

    ASSERT(buckets && "buckets should be initialized");
    ASSERT(entries && "entries should be initialized");
    ASSERT(bloomFilters && "bloomFilters should be initialized");

    PRINT_DEBUG_HASH("gonna build gnu hash table with %d entries and %d buckets, %d bloom filters", numEntries, numBuckets, numberOfBloomFilters);

    numberOfEntries = numEntries;
    numberOfBuckets = numBuckets + numberOfBloomFilters - 1;
    numberOfBloomFilters = 1;

    PRINT_DEBUG_HASH("minimizing bloom filters: %d entries and %d buckets, %d bloom filters", numEntries, numBuckets, numberOfBloomFilters);

    PRINT_DEBUG_HASH("re-sorting symbol table");
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    symTab->sortForGnuHash(firstSymIndex, numberOfBuckets);

    PRINT_DEBUG_HASH("bucket listing:");
    for (uint32_t i = firstSymIndex; i < symTab->getNumberOfSymbols(); i++){
        PRINT_DEBUG_HASH("\tsym[%d] hashbuck %d", i, elf_gnu_hash(symTab->getSymbolName(i)) % numberOfBuckets);
    }

    ASSERT(numberOfBuckets);

    delete[] entries;
    delete[] buckets;
    delete[] bloomFilters;

    uint32_t bucketIndex = 0;
    buckets = new uint32_t[numberOfBuckets];
    bzero(buckets, sizeof(uint32_t) * numberOfBuckets);
    for (uint32_t i = symTab->getNumberOfSymbols() - 1; i >= firstSymIndex; i--){
        bucketIndex = elf_gnu_hash(symTab->getSymbolName(i)) % numberOfBuckets;
        buckets[bucketIndex] = i;
    }

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_DEBUG_HASH("buckets[%d] = %d", i, buckets[i]);
    }

    bool stopBits[numberOfEntries];
    bzero(stopBits, sizeof(bool) * numberOfEntries);
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        if (buckets[i] > firstSymIndex){
            PRINT_DEBUG_HASH("accessing stop bit %d/%d -- bucket[%d] is %d", buckets[i] - firstSymIndex - 1, numberOfEntries, i, buckets[i]);
            stopBits[buckets[i] - firstSymIndex - 1] = true;
        }
    }

    entries = new uint32_t[numberOfEntries];
    for (uint32_t i = 0; i < numberOfEntries; i++){
        entries[i] = elf_gnu_hash(symTab->getSymbolName(i + firstSymIndex)) & ~1;
        if (stopBits[i]){
            entries[i] |= 1;
        }
    }

    // create a single-entry bloom filter with all bits set. this will filter nothing.
    // there is a potential for a performance penalty when looking up symbols without a bloom filter
    bloomFilters = new uint64_t[numberOfBloomFilters];
    bloomFilters[0] = 0xffffffffffffffff;
    //bloomFilters[0] = 0;

    sizeInBytes = (4 * sizeof(uint32_t)) + (hashEntrySize * numberOfBloomFilters) + (sizeof(uint32_t) * numberOfBuckets) + (sizeof(uint32_t) * numberOfEntries);
    verify();

}

uint32_t GnuHashTable::findSymbol(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    uint32_t h1, h2, n, bitmask;
    h1 = elf_gnu_hash(symbolName);
    h2 = h1 >> shiftCount;

    n = (h1 / hashEntrySize) & (numberOfBloomFilters - 1);
    bitmask = (1 << (h1 % hashEntrySize)) | (1 << (h2 % hashEntrySize));
    if (bloomFilters[n] & bitmask != bitmask){
        PRINT_ERROR("The symbol being searched (%s) is non-existent (failed bloom filter)", symbolName);
        return -1;
    }

    uint32_t x = buckets[elf_gnu_hash(symbolName) % numberOfBuckets];
    uint32_t entryVal;

    PRINT_DEBUG_HASH("Symbol with name %s has hash buckets[%d]=%d", symbolName, elf_sysv_hash(symbolName) % numberOfBuckets, x);

    while (x - firstSymIndex < numberOfEntries && !entryHasStopBit(x - firstSymIndex) && strcmp(symbolName,symTab->getSymbolName(x))){
        x++;
    }
    if (strcmp(symbolName,symTab->getSymbolName(x))){
        PRINT_ERROR("The symbol being searched (%s) is non-existent", symbolName);
        return -1;
    }
    return x;
    
}

void GnuHashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    uint32_t tmpEntry;

    binaryOutputFile->copyBytes((char*)&numberOfBuckets, sizeof(uint32_t), offset + currByte);
    currByte += sizeof(uint32_t);

    binaryOutputFile->copyBytes((char*)&firstSymIndex, sizeof(uint32_t), offset + currByte);
    currByte += sizeof(uint32_t);

    binaryOutputFile->copyBytes((char*)&numberOfBloomFilters, sizeof(uint32_t), offset + currByte);
    currByte += sizeof(uint32_t);

    binaryOutputFile->copyBytes((char*)&shiftCount, sizeof(uint32_t), offset + currByte);
    currByte += sizeof(uint32_t);

    for (uint32_t i = 0; i < numberOfBloomFilters; i++){
        if (hashEntrySize == sizeof(uint64_t)){
            binaryOutputFile->copyBytes((char*)&bloomFilters[i], sizeof(uint64_t), offset + currByte);
            currByte += sizeof(uint64_t);
        } else {
            uint32_t bloom32 = (uint32_t)bloomFilters[i];
            binaryOutputFile->copyBytes((char*)&bloom32, sizeof(uint32_t), offset + currByte);
            currByte += sizeof(uint32_t);
        }
    }
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        binaryOutputFile->copyBytes((char*)&buckets[i], sizeof(uint32_t), offset + currByte);
        currByte += sizeof(uint32_t);
    }
    
    for (uint32_t i = 0; i < numberOfEntries; i++){
        binaryOutputFile->copyBytes((char*)&entries[i], sizeof(uint32_t), offset + currByte);
        currByte += sizeof(uint32_t);
    }
}


uint32_t GnuHashTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes >= sizeof(uint32_t) * 4 && "Hash Table must contain at least 4 values");

    if (!binaryInputFile->copyBytesIterate((char*)&numberOfBuckets, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read nbucket from Hash Table");
    }
    buckets = new uint32_t[numberOfBuckets];

    if (!binaryInputFile->copyBytesIterate((char*)&firstSymIndex, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read symndx from Hash Table");
    }

    if (!binaryInputFile->copyBytesIterate((char*)&numberOfBloomFilters, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read maskwords from Hash Table");
    }
    bloomFilters = new uint64_t[numberOfBloomFilters];

    if (!binaryInputFile->copyBytesIterate((char*)&shiftCount, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read shift2 from Hash Table");
    }

    for (uint32_t i = 0; i < numberOfBloomFilters; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&bloomFilters[i], hashEntrySize)){
            PRINT_ERROR("Cannot read bloom filter %d", i);
        }
    }

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&buckets[i], sizeof(uint32_t))){
            PRINT_ERROR("Cannot read bucket %d", i);
        }
    }

    // a bit of a hack here since the symbol table isn't initialized. we will just assume
    // that all remaining bytes are entries and verify that this is correct later when 
    // the symbol table is present
    uint32_t remainingBytes = (sizeInBytes - (4 * sizeof(uint32_t)) - (numberOfBloomFilters * hashEntrySize) - (numberOfBuckets * sizeof(uint32_t)));
    ASSERT(remainingBytes % sizeof(uint32_t) == 0);
    numberOfEntries = remainingBytes / sizeof(uint32_t);
    entries = new uint32_t[numberOfEntries];
    for (uint32_t i = 0; i < numberOfEntries; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&entries[i], sizeof(uint32_t))){
            PRINT_ERROR("Cannot read value %d", i);
        }
    }

    verify();
}

void GnuHashTable::initFilePointers(){
    //    printBytes(0,0,0);

    // locate the symbol table for this hash table (should be the dynamic symbol table
    symTabIdx = elfFile->getNumberOfSymbolTables();
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        PRINT_DEBUG_HASH("Symbol Table %d is section %d", i, elfFile->getSymbolTable(i)->getSectionIndex());
        if (elfFile->getSymbolTable(i)->getSectionIndex() == elfFile->getSectionHeader(sectionIndex)->GET(sh_link)){
            ASSERT(elfFile->getSymbolTable(i)->isDynamic() && "Hash table should be linked with a symbol table that is dynamic");
            symTabIdx = i;
        }
    }
    ASSERT(symTabIdx < elfFile->getNumberOfSymbolTables() && "Could not find a symbol table for the Hash Table");

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    ASSERT(!numberOfEntries || numberOfEntries == symTab->getNumberOfSymbols() - firstSymIndex);
}

GnuHashTable::GnuHashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
        : HashTable(PebilClassType_GnuHashTable, rawPtr, size, scnIdx, elf)
{
    numberOfBloomFilters = 0;
    bloomFilters = NULL;

    if (elfFile->is64Bit()){
        hashEntrySize = Size__64_bit_Hash_Entry;
    } else {
        hashEntrySize = Size__32_bit_Hash_Entry;
    }

    firstSymIndex = 0;
    shiftCount = 0;
}

bool GnuHashTable::entryHasStopBit(uint32_t idx){
    ASSERT(idx < numberOfEntries);
    return (entries[idx] & 1);
}

bool GnuHashTable::matchesEntry(uint32_t idx, uint32_t val){
    ASSERT(idx < numberOfEntries);
    return (entries[idx] & ~1) == (val & ~1);
}

void GnuHashTable::print(){

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    PRINT_INFOR("GnuHashTable: sect %d ( for sect(dynsym) %d) with %d x %d, firstSym %d, Bloom [%d,%d]",
                sectionIndex, symTab->getSectionIndex(), numberOfBuckets, numberOfEntries, firstSymIndex, numberOfBloomFilters, shiftCount);

    for (uint32_t i = 0; i < numberOfBloomFilters; i++){
        if (hashEntrySize == sizeof(uint64_t)){
            PRINT_INFOR("\tbloomf(64) %4d: %llx", i, bloomFilters[i]);
        } else {
            PRINT_INFOR("\tbloomf(32) %4d: %llx", i, bloomFilters[i]);
        }
    }

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("buc %5d: %d", i, buckets[i]);
    }

    for (uint32_t i = 0; i < numberOfEntries; i++){
        char stp[6] = "\0";
        if (entryHasStopBit(i)){
            sprintf(stp, "(stp)\0");
        }
        PRINT_INFOR("value %5d: %8x %s", i, entries[i], stp);
    }
}

GnuHashTable::~GnuHashTable(){
    if (bloomFilters){
        delete[] bloomFilters;
    }
}

bool GnuHashTable::verify(){
    if (hashEntrySize == sizeof(uint32_t)){
        if (getSectionHeader()->GET(sh_entsize) != sizeof(uint32_t)){
            PRINT_ERROR("Entry size should be const value for 32-bit hash table");
            return false;
        }
    } else {
        if (getSectionHeader()->GET(sh_entsize) != 0){
            PRINT_ERROR("Entry size should be zero (variable) for 64-bit hash table");
            return false;
        }
    }

    if (!isPowerOfTwo(numberOfBloomFilters)){
        PRINT_ERROR("Expect maskwords to be a power of 2");
        return false;
    }

    if (numberOfEntries){
        SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
        for (uint32_t i = firstSymIndex; i < symTab->getNumberOfSymbols(); i++){
            if (findSymbol(symTab->getSymbolName(i)) != i){
                PRINT_ERROR("Hash Table search failed for symbol %s (idx %d)", symTab->getSymbolName(i), i);
                return false;
            }
        }
    }

    return true;
}

bool SysvHashTable::passedThreshold(){
    return (numberOfBuckets < numberOfEntries/2 || numberOfBuckets < 2);
}

bool GnuHashTable::passedThreshold(){
    return (numberOfBuckets < numberOfEntries/2 || numberOfBuckets < 2);
}

// while maintaining a fixed-size table, we will increment the number of entries,
// decrement the number of buckets, then rebuild the table
void HashTable::addEntry(){
    buildTable(numberOfEntries + 1, numberOfBuckets - 1);
}

void SysvHashTable::buildTable(uint32_t numEntries, uint32_t numBuckets){

    ASSERT(buckets && "buckets should be initialized");
    ASSERT(entries && "entries should be initialized");

    numberOfEntries = numEntries;
    numberOfBuckets = numBuckets;

    ASSERT(numberOfBuckets > 0 && "The hash table must be expanded in order to create room for more buckets");
    
    delete[] entries;
    delete[] buckets;

    entries = new uint32_t[numberOfEntries];
    buckets = new uint32_t[numberOfBuckets];

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    ASSERT(symTab->getNumberOfSymbols() == numberOfEntries && "Symbol table should have the same number of symbols as there are entries in the hash table");

    PRINT_DEBUG_HASH("Building hash table with c=%d and b=%d", numberOfEntries, numberOfBuckets);

    // temporarily set entry[i] to the bucket index a name lookup on entry[i] with have to pass through
    for (uint32_t i = 0; i < numberOfEntries; i++){
        entries[i] = elf_sysv_hash(symTab->getSymbolName(i)) % numberOfBuckets;
        PRINT_DEBUG_HASH("Entry[%d] = (%d)%d -- %s", i, entries[i] % numberOfBuckets, entries[i], symTab->getSymbolName(i));
    }

    // set bucket[i] to the last entry index which uses that bucket (ie, where entries[i] == i)
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        buckets[i] = numberOfEntries;
        int32_t entryidx = numberOfEntries - 1;
        while (buckets[i] == numberOfEntries && entryidx >= 0){
            if (entries[entryidx] == i){
                buckets[i] = entryidx;
            }
            entryidx--;
        }
        if (buckets[i] == numberOfEntries){
            buckets[i] = 0;
        }
        PRINT_DEBUG_HASH("Bucket[%d] = %d", i, buckets[i]);
    }

    // point entry[i] to entry[j] where j < i and the symbol names for symbols i, j hash to the same bucket
    for (int32_t i = numberOfEntries - 1; i >= 0; i--){
        bool isChanged = false;
        for (int32_t j = i - 1; j >= 0; j--){
            if (entries[i] == entries[j]){
                entries[i] = j;
                j = -1;
                isChanged = true;
            }
        }
        if (!isChanged){
            entries[i] = 0;
        }
        PRINT_DEBUG_HASH("real Entry[%d] = %d", i, entries[i]);
    }
    sizeInBytes = sizeof(uint32_t) * (2 + numberOfBuckets + numberOfEntries);
    verify();
}


uint32_t HashTable::expandSize(uint32_t amt){
    buildTable(numberOfEntries, numberOfBuckets + amt);
    return amt;
}

bool HashTable::isGnuStyleHash(){
    SectionHeader* mySection = elfFile->getSectionHeader(sectionIndex);

    ASSERT(mySection && "Section header should exist");
    if (mySection->GET(sh_type) == SHT_HASH){
        return false;
    } else if (mySection->GET(sh_type) == SHT_GNU_HASH){
        return true;
    } else {
        __SHOULD_NOT_ARRIVE;
    }

    return false;
}

uint32_t SysvHashTable::findSymbol(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    uint32_t x = buckets[elf_sysv_hash(symbolName) % numberOfBuckets];
    uint32_t entryVal;

    PRINT_DEBUG_HASH("Symbol with name %s has hash buckets[%d]=%d", symbolName, elf_sysv_hash(symbolName) % numberOfBuckets, x);

    while (strcmp(symbolName,symTab->getSymbolName(x))){
        if (x == entries[x]){
            PRINT_ERROR("The symbol being searched (%s) is non-existent", symbolName);
            return -1;
        }
        x = entries[x];
    }
    return x;

}


bool SysvHashTable::verify(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    if (!symTab){
        PRINT_ERROR("Couldn't get symbol table %d from elfFile", symTabIdx);
        return false;
    }

    if (isGnuStyleHash()){
        PRINT_ERROR("This hash table should use sysv-style hashing");
        return false;
    }

    if (numberOfEntries != symTab->getNumberOfSymbols()){
        PRINT_ERROR("In the hash table, the number of entries should be equal to the number of symbols in the corresponding symbol table");
        return false;
    }
    
    for (uint32_t i = 1; i < numberOfEntries; i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            print();
            PRINT_ERROR("Hash Table search for %s is erroneous", symTab->getSymbolName(i));
            return false;
        }
    }
    
    if (elfFile->getSectionHeader(sectionIndex)->GET(sh_entsize) != sizeof(uint32_t)){
        PRINT_ERROR("Hash table entry size must be %d bytes", sizeof(uint32_t));
        return false;
    }
    
    if (elfFile->getSectionHeader(sectionIndex)->GET(sh_type) != SHT_HASH &&
        elfFile->getSectionHeader(sectionIndex)->GET(sh_type) != SHT_GNU_HASH){
        PRINT_ERROR("Section type for hash table must be SHT_HASH or SHT_GNU_HASH");
        return false;
    }
    
    for (uint32_t i = 0; i < symTab->getNumberOfSymbols(); i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            PRINT_ERROR("Hash Table search failed for symbol %s (idx %d)", symTab->getSymbolName(i), i);
            return false;
        }
    }

    if (sizeInBytes != sizeof(uint32_t) * (2 + numberOfEntries + numberOfBuckets)){
        PRINT_ERROR("Hash Table size is incorrect");
        return false;
    }

    return true;
}


void SysvHashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    uint32_t tmpEntry;

    binaryOutputFile->copyBytes((char*)&numberOfBuckets,sizeof(uint32_t),offset+currByte);
    currByte += sizeof(uint32_t);

    binaryOutputFile->copyBytes((char*)&numberOfEntries,sizeof(uint32_t),offset+currByte);
    currByte += sizeof(uint32_t);
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        binaryOutputFile->copyBytes((char*)&buckets[i],sizeof(uint32_t),offset+currByte);
        currByte += sizeof(uint32_t);
    }
    
    for (uint32_t i = 0; i < numberOfEntries; i++){
        binaryOutputFile->copyBytes((char*)&entries[i],sizeof(uint32_t),offset+currByte);
        currByte += sizeof(uint32_t);
    }
}

uint32_t SysvHashTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes >= sizeof(uint32_t) * 2 && "Hash Table must contain at least 2 entries");
    
    if (!binaryInputFile->copyBytesIterate((char*)&numberOfBuckets, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read nbucket from Hash Table");
    }
    buckets = new uint32_t[numberOfBuckets];
    
    if (!binaryInputFile->copyBytesIterate((char*)&numberOfEntries, sizeof(uint32_t))){
        PRINT_ERROR("Cannot read nentry from Hash Table");
    }
    entries = new uint32_t[numberOfEntries];
    
    ASSERT(sizeInBytes == sizeof(uint32_t)*(numberOfBuckets + numberOfEntries + 2) && "Hash Table size is inconsistent with its internal information");
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&buckets[i], sizeof(uint32_t))){
            PRINT_ERROR("Cannot read bucket[%d] from Hash Table", i);
        }
    }
    
    for (uint32_t i = 0; i < numberOfEntries; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&entries[i], sizeof(uint32_t))){
            PRINT_ERROR("Cannot read entry[%d] from Hash Table)", i);
        }
    }

    return sizeInBytes;
}

void SysvHashTable::print(){

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    PRINT_INFOR("SysvHashTable: sect %d ( for sect(dynsym?) %d) with %d x %d",
        sectionIndex,symTab->getSectionIndex(),numberOfBuckets,numberOfEntries);

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("\tbuc%5d",i);
        uint32_t entryidx = buckets[i];
        while (entryidx){
            PRINT_INFOR("\t\tchn%5d -- %s",entryidx,symTab->getSymbolName(entryidx));
            entryidx = entries[entryidx];

        }
    }

}


uint32_t HashTable::getBucket(uint32_t idx){
    ASSERT(idx < numberOfBuckets && "index into Hash Table bucket array is out of bounds");
    ASSERT(buckets && "bucket array should be initialized");

    return buckets[idx];
}

uint32_t HashTable::getEntry(uint32_t idx){
    ASSERT(idx < numberOfEntries && "index into Hash Table entry array is out of bounds");
    ASSERT(entries && "entry array should be initialized");

    return entries[idx];
}

void SysvHashTable::initFilePointers(){

    // locate the symbol table for this hash table (should be the dynamic symbol table
    symTabIdx = elfFile->getNumberOfSymbolTables();
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        PRINT_DEBUG_HASH("Symbol Table %d is section %d", i, elfFile->getSymbolTable(i)->getSectionIndex());
        if (elfFile->getSymbolTable(i)->getSectionIndex() == elfFile->getSectionHeader(sectionIndex)->GET(sh_link)){
            ASSERT(elfFile->getSymbolTable(i)->isDynamic() && "Hash table should be linked with a symbol table that is dynamic");
            symTabIdx = i;
        }
    }
    ASSERT(symTabIdx < elfFile->getNumberOfSymbolTables() && "Could not find a symbol table for the Hash Table");

}

HashTable::HashTable(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(classType, rawPtr, size, scnIdx, elf)
{
    symTabIdx = elfFile->getNumberOfSymbolTables();

    numberOfBuckets = 0;
    buckets = NULL;
}

SysvHashTable::SysvHashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : HashTable(PebilClassType_SysvHashTable, rawPtr, size, scnIdx, elf)
{
    numberOfEntries = 0;
    entries = NULL;

    hashEntrySize = Size__32_bit_Hash_Entry;
}

HashTable::~HashTable(){
    if (buckets){
        delete[] buckets;
    }
    if (entries){
        delete[] entries;
    }
}


