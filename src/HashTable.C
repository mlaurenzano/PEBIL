#include <HashTable.h>

bool HashTable::isGnuStyleHash(){
    if (elfFile->getSectionHeader(sectionIndex)->GET(sh_type) == SHT_GNU_HASH){
        return true;
    } else if (elfFile->getSectionHeader(sectionIndex)->GET(sh_type) == SHT_HASH){
        return false;
    }
    ASSERT(0 && "Hash table type should be either SHT_HASH or SHT_GNU_HASH");
}

uint32_t HashTable::findSymbol(const char* symbolName){
    if (!isGnuStyleHash()){
        return findSymbolSysv(symbolName);
    } else {
        ASSERT(0 && "GNU hash tables not supported -- try to relink the target with `-Wl,--hash-style=sysv`");
        return 0;
    }
}

uint32_t HashTable::findSymbolSysv(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    uint32_t x = buckets[elf_sysv_hash(symbolName)%numberOfBuckets];
    uint32_t chainVal;

    while (strcmp(symbolName,symTab->getSymbolName(x))){
        if (x == chains[x]){
            PRINT_ERROR("The symbol being searched (%s) is non-existent", symbolName);
            return -1;
        }
        x = chains[x];
    }
    return x;

}

bool HashTable::verify(){
    if (!isGnuStyleHash()){
        verifySysv();
    } else {
        PRINT_ERROR("GNU hash tables not supported -- try to relink the target with `-Wl,--hash-style=sysv`");
    }
    return true;
}

bool HashTable::verifySysv(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    if (numberOfChains != symTab->getNumberOfSymbols()){
        PRINT_ERROR("In the hash table, the number of chains should be equal to the number of symbols in the corresponding symbol table");
    }
    
    for (uint32_t i = 1; i < numberOfChains; i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            PRINT_ERROR("Hash Table search function is erroneous");
        }
    }
    
    if (elfFile->getSectionHeader(sectionIndex)->GET(sh_entsize) != hashEntrySize){
        PRINT_ERROR("Hash table entry size must be %d bytes", hashEntrySize);
    }
    
    if (elfFile->getSectionHeader(sectionIndex)->GET(sh_type) != SHT_HASH &&
        elfFile->getSectionHeader(sectionIndex)->GET(sh_type) != SHT_GNU_HASH){
        PRINT_ERROR("Section type for hash table must be SHT_HASH or SHT_GNU_HASH");
    }
    
    if (isGnuStyleHash()){
        PRINT_ERROR("This hash table should use sysv-style hashing");
    }
}

void HashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (!isGnuStyleHash()){
        dumpSysv(binaryOutputFile, offset);
    } else {
        ASSERT(0 && "GNU hash tables not supported -- try to relink the target with `-Wl,--hash-style=sysv`");
    }
}

void HashTable::dumpSysv(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    uint32_t tmpEntry;

    binaryOutputFile->copyBytes((char*)&numberOfBuckets,hashEntrySize,offset+currByte);
    currByte += hashEntrySize;

    binaryOutputFile->copyBytes((char*)&numberOfChains,hashEntrySize,offset+currByte);
    currByte += hashEntrySize;
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        binaryOutputFile->copyBytes((char*)&buckets[i],hashEntrySize,offset+currByte);
        currByte += hashEntrySize;
    }
    
    for (uint32_t i = 0; i < numberOfChains; i++){
        binaryOutputFile->copyBytes((char*)&chains[i],hashEntrySize,offset+currByte);
        currByte += hashEntrySize;
    }
}

uint32_t HashTable::read(BinaryInputFile* binaryInputFile){
    if (!isGnuStyleHash()){
        return readSysv(binaryInputFile);
    } else {
        ASSERT(0 && "GNU hash tables not supported -- try to relink the target with `-Wl,--hash-style=sysv`");
    }
    return sizeInBytes;   
}

uint32_t HashTable::readSysv(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes >= hashEntrySize * 2 && "Hash Table must contain at least 2 entries");
    
    if (!binaryInputFile->copyBytesIterate((char*)&numberOfBuckets, hashEntrySize)){
        PRINT_ERROR("Cannot read nbucket from Hash Table");
    }
    buckets = new uint32_t[numberOfBuckets];
    
    if (!binaryInputFile->copyBytesIterate((char*)&numberOfChains, hashEntrySize)){
        PRINT_ERROR("Cannot read nchain from Hash Table");
    }
    chains = new uint32_t[numberOfChains];
    
    ASSERT(sizeInBytes == hashEntrySize*(numberOfBuckets + numberOfChains + 2) && "Hash Table size is inconsistent with its internal information");
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&buckets[i], hashEntrySize)){
            PRINT_ERROR("Cannot read bucket[%d] from Hash Table", i);
        }
    }
    
    for (uint32_t i = 0; i < numberOfChains; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&chains[i], hashEntrySize)){
            PRINT_ERROR("Cannot read chain[%d] from Hash Table)", i);
        }
    }
}

void HashTable::print(){
    if (!isGnuStyleHash()){
        printSysv();
    } else {
        ASSERT(0 && "GNU hash tables not supported -- try to relink the target with `-Wl,--hash-style=sysv`");
    }
}

void HashTable::printSysv(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    PRINT_INFOR("Hash Table: section %hd, %d buckets, %d chains", sectionIndex, numberOfBuckets, numberOfChains);
    
    printBytes(0,0);
    
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("BKT[%d]\t%d", i, buckets[i]);
    }
    for (uint32_t i = 0; i < numberOfChains; i++){
        PRINT_INFOR("CHN[%d]\t%d\t%s", i, chains[i], symTab->getSymbolName(i));
    }
}

uint64_t HashTable::getBloom(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfBlooms && "index into Hash table bloom array is out of bounds");
    ASSERT(blooms && "bloom array should be initialized");

    return blooms[idx];
}

uint32_t HashTable::getBucket(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfBuckets && "index into Hash Table bucket array is out of bounds");
    ASSERT(buckets && "bucket array should be initialized");

    return buckets[idx];
}

uint32_t HashTable::getChain(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfChains && "index into Hash Table chain array is out of bounds");
    ASSERT(chains && "chain array should be initialized");

    return chains[idx];
}


HashTable::HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(ElfClassTypes_hash_table,rawPtr,size,scnIdx,elf)
{
    // locate the symbol table for this hash table (should be the dynamic symbol table
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        if (elfFile->getSymbolTable(i)->getSectionIndex() == elfFile->getSectionHeader(sectionIndex)->GET(sh_link)){
            ASSERT(elfFile->getSymbolTable(i)->isDynamic() && "Hash table should be linked with a symbol table that is dynamic");
            symTabIdx = i;
        }

    }

    if (elfFile->is64Bit()){
        hashEntrySize = Size__64_bit_Hash_Entry;
        bloomEntrySize = Size__64_bit_GNU_Hash_Bloom_Entry;
    } else {
        hashEntrySize = Size__32_bit_Hash_Entry;
        bloomEntrySize = Size__32_bit_GNU_Hash_Bloom_Entry;
    }

    numberOfBuckets = 0;
    buckets = NULL;

    numberOfChains = 0;
    chains = NULL;

    numberOfBlooms = 0;
    blooms = NULL;
}


HashTable::~HashTable(){
    if (buckets){
        delete[] buckets;
    }
    if (chains){
        delete[] chains;
    }
    if (blooms){
        delete[] blooms;
    }
}

