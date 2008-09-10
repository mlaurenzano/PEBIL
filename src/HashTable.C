#include <HashTable.h>


// while maintaining a fixed-size table, we will increment the number of chains,
// decrement the number of buckets, then rebuild the table
void HashTable::addChain(){
    buildTable(numberOfChains+1, numberOfBuckets-1);
}

void HashTable::buildTable(uint32_t numChains, uint32_t numBuckets){

    ASSERT(buckets && "buckets should be initialized");
    ASSERT(chains && "chains should be initialized");

    numberOfChains = numChains;
    numberOfBuckets = numBuckets;

    ASSERT(numberOfBuckets > 0 && "The hash table must be expanded in order to create room for more buckets");
    
    delete[] chains;
    delete[] buckets;

    chains = new uint32_t[numberOfChains];
    buckets = new uint32_t[numberOfBuckets];

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    ASSERT(symTab->getNumberOfSymbols() == numberOfChains && "Symbol table should have the same number of symbols as there are chains in the hash table");

    PRINT_DEBUG_HASH("Building hash table with c=%d and b=%d", numberOfChains, numberOfBuckets);

    // temporarily set chain[i] to the bucket index a name lookup on chain[i] with have to pass through
    for (uint32_t i = 0; i < numberOfChains; i++){
        chains[i] = elf_sysv_hash(symTab->getSymbolName(i)) % numberOfBuckets;
        PRINT_DEBUG_HASH("Chain[%d] = (%d)%d -- %s", i, chains[i] % numberOfBuckets, chains[i], symTab->getSymbolName(i));
    }

    // set bucket[i] to the last chain index which uses that bucket (ie, where chains[i] == i)
    for (uint32_t i = 0; i < numberOfBuckets; i++){
        buckets[i] = numberOfChains;
        int32_t chainidx = numberOfChains-1;
        while (buckets[i] == numberOfChains && chainidx >= 0){
            if (chains[chainidx] == i){
                buckets[i] = chainidx;
            }
            chainidx--;
        }
        if (buckets[i] == numberOfChains){
            buckets[i] = 0;
        }
        PRINT_DEBUG_HASH("Bucket[%d] = %d", i, buckets[i]);
    }

    // point chain[i] to chain[j] where j<i and the symbol names for symbols i,j hash to the same bucket
    for (int32_t i = numberOfChains-1; i >= 0; i--){
        bool isChanged = false;
        for (int32_t j = i-1; j >= 0; j--){
            if (chains[i] == chains[j]){
                chains[i] = j;
                j = -1;
                isChanged = true;
            }
        }
        if (!isChanged){
            chains[i] = 0;
        }
        PRINT_DEBUG_HASH("real Chain[%d] = %d", i, chains[i]);
    }
    sizeInBytes = hashEntrySize * (2 + numberOfBuckets + numberOfChains);
}


uint32_t HashTable::expandSize(uint32_t amt){
    buildTable(numberOfChains, numberOfBuckets + amt);
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

uint32_t HashTable::findSymbol(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    uint32_t x = buckets[elf_sysv_hash(symbolName)%numberOfBuckets];
    uint32_t chainVal;

    PRINT_DEBUG_HASH("Symbol with name %s has hash buckets[%d]=%d", symbolName, elf_sysv_hash(symbolName)%numberOfBuckets, x);

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
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    if (!symTab){
        PRINT_ERROR("Couldn't get symbol table %d from elfFile", symTabIdx);
    }

    if (isGnuStyleHash()){
        PRINT_ERROR("This hash table should use sysv-style hashing");
    }

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
    
    for (uint32_t i = 0; i < symTab->getNumberOfSymbols(); i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            PRINT_ERROR("Hash Table search failed for symbol %s", symTab->getSymbolName(i));
        }
    }

    if (sizeInBytes != hashEntrySize * (2 + numberOfChains + numberOfBuckets)){
        PRINT_ERROR("Hash Table size is incorrect");
    }
}


void HashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
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

    return sizeInBytes;
}

void HashTable::print(){

    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    PRINT_INFOR("HashTable: sect %d ( for sect(dynsym?) %d) with %d x %d",
        sectionIndex,symTab->getSectionIndex(),numberOfBuckets,numberOfChains);

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("\tbuc%5d",i);
        uint32_t chainidx = buckets[i];
        while (chainidx){
            PRINT_INFOR("\t\tchn%5d -- %s",chainidx,symTab->getSymbolName(chainidx));
            chainidx = chains[chainidx];

        }
    }
#ifdef DEBUG_HASH
    printBytes(0,0);
#endif
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

void HashTable::initFilePointers(){

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


HashTable::HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(ElfClassTypes_HashTable,rawPtr,size,scnIdx,elf)
{
    symTabIdx = elfFile->getNumberOfSymbolTables();

    if (elfFile->is64Bit()){
        hashEntrySize = Size__64_bit_Hash_Entry;
    } else {
        hashEntrySize = Size__32_bit_Hash_Entry;
    }

    numberOfBuckets = 0;
    buckets = NULL;

    numberOfChains = 0;
    chains = NULL;

    if (elfFile->getSectionHeader(scnIdx)->GET(sh_type) != SHT_HASH){
        if (elfFile->getSectionHeader(scnIdx)->GET(sh_type) == SHT_GNU_HASH){
            PRINT_ERROR("Hash table cannot use gnu-style hash table, relink with -Wl,--hash-style=sysv");
            __SHOULD_NOT_ARRIVE;
        } else {
            PRINT_ERROR("Hash table has a non-hash section type; this is very wrong");
            __SHOULD_NOT_ARRIVE;
        }
    }

}


HashTable::~HashTable(){
    if (buckets){
        delete[] buckets;
    }
    if (chains){
        delete[] chains;
    }
}

