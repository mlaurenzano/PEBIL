#include <HashTable.h>

uint64_t HashTable::findSymbol(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    uint64_t x = bucket[elf_hash(symbolName)%numberOfBuckets];
    uint64_t chainVal;

    while (strcmp(symbolName,symTab->getSymbolName(x))){
        x = chain[x];
    }
    return x;
}


bool HashTable::verify(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    if (numberOfChains != symTab->getNumberOfSymbols()){
        PRINT_ERROR("In the hash table, the number of chains should be equal to the number of symbols in the corresponding symbol table");
    }

    for (uint64_t i = 1; i < numberOfChains; i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            PRINT_ERROR("Hash Table search function is erroneous");
        }
    }

    PRINT_INFOR("finished verifying hash table at section %hd", sectionIndex);
    return true;
}

void HashTable::dump32(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    uint32_t tmpEntry;

    tmpEntry = (uint32_t)numberOfBuckets;
    ASSERT(tmpEntry == numberOfBuckets && "Hash table entry does not fit in 32 bits");
    binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Hash_Entry,offset+currByte);
    currByte += Size__32_bit_Hash_Entry;

    tmpEntry = (uint32_t)numberOfChains;
    ASSERT(tmpEntry == numberOfChains && "Hash table entry does not fit in 32 bits");
    binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Hash_Entry,offset+currByte);
    currByte += Size__32_bit_Hash_Entry;

    for (uint64_t i = 0; i < numberOfBuckets; i++){
        tmpEntry = (uint32_t)bucket[i];
        ASSERT(tmpEntry == bucket[i] && "Hash table entry does not fit in 32 bits");
        binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Hash_Entry,offset+currByte);
        currByte += Size__32_bit_Hash_Entry;
    }

    for (uint64_t i = 0; i < numberOfChains; i++){
        tmpEntry = (uint32_t)chain[i];
        ASSERT(tmpEntry == chain[i] && "Hash table entry does not fit in 32 bits");
        binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Hash_Entry,offset+currByte);
        currByte += Size__32_bit_Hash_Entry;
    }
}

void HashTable::dump64(BinaryOutputFile* binaryOutputFile, uint32_t offset){
}

void HashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (elfFile->is64Bit()){
        dump64(binaryOutputFile, offset);
    } else {
        dump32(binaryOutputFile, offset);
    }
}

uint32_t HashTable::read32(BinaryInputFile* binaryInputFile){

    uint32_t tmpEntry;

    if (!binaryInputFile->copyBytesIterate((char*)&tmpEntry, Size__32_bit_Hash_Entry)){
        PRINT_ERROR("Cannot read nbucket from Hash Table");
    }
    numberOfBuckets = (uint64_t)tmpEntry;
    bucket = new uint64_t[numberOfBuckets];

    if (!binaryInputFile->copyBytesIterate((char*)&tmpEntry, Size__32_bit_Hash_Entry)){
        PRINT_ERROR("Cannot read nchain from Hash Table");
    }
    numberOfChains = (uint64_t)tmpEntry;
    chain = new uint64_t[numberOfChains];

    //PRINT_INFOR("%d %lld", sizeInBytes, hashEntrySize*(numberOfBuckets + numberOfChains + 2));
    ASSERT(sizeInBytes == hashEntrySize*(numberOfBuckets + numberOfChains + 2) && "Hash Table size is inconsistent with its internal information");

    for (uint64_t i = 0; i < numberOfBuckets; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&tmpEntry, Size__32_bit_Hash_Entry)){
            PRINT_ERROR("Cannot read bucket[%d] from Hash Table (32)", i);
        }
        bucket[i] = (uint64_t)tmpEntry;
    }

    for (uint64_t i = 0; i < numberOfChains; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&tmpEntry, Size__32_bit_Hash_Entry)){
            PRINT_ERROR("Cannot read chain[%d] from Hash Table (32)", i);
        }
        chain[i] = (uint64_t)tmpEntry;
    }

    return sizeInBytes;
}

uint32_t HashTable::read64(BinaryInputFile* binaryInputFile){
    return 0;
}

uint32_t HashTable::read(BinaryInputFile* binaryInputFile){

    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes >= hashEntrySize * 2 && "Hash Table must contain at least 2 entries");

    if (elfFile->is64Bit()){
        read64(binaryInputFile);
    } else {
        read32(binaryInputFile);
    }

    return sizeInBytes;

}


void HashTable::print(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    PRINT_INFOR("Hash Table: section %hd, %lld buckets, %lld chains", sectionIndex, numberOfBuckets, numberOfChains);

    printBytes(0,0);

    for (uint64_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("BKT[%lld]\t%lld", i, bucket[i]);
    }
    for (uint64_t i = 0; i < numberOfChains; i++){
        PRINT_INFOR("CHN[%lld]\t%lld\t%s", i, chain[i], symTab->getSymbolName(i));
    }
}

uint64_t HashTable::getBucket(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfBuckets && "index into Hash Table bucket array is out of bounds");
    ASSERT(bucket && "bucket array should be initialized");

    return bucket[idx];
}

uint64_t HashTable::getChain(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfChains && "index into Hash Table chain array is out of bounds");
    ASSERT(chain && "chain array should be initialized");

    return chain[idx];
}


HashTable::HashTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(ElfClassTypes_hash_table,rawPtr,size,scnIdx,elf)
{
    // locate the symbol table for this hash table (should be the dynamic symbol table
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        if (elfFile->getSymbolTable(i)->getSectionIndex() == elfFile->getSectionHeader(sectionIndex)->GET(sh_link)){
            ASSERT(elfFile->getSymbolTable(i)->isDynamic() && "Hash table should be link with a symbol table that is dynamic");
            symTabIdx = i;
        }

    }

    if (elfFile->is64Bit()){
        hashEntrySize = Size__64_bit_Hash_Entry;
    } else {
        hashEntrySize = Size__32_bit_Hash_Entry;
    }

    numberOfBuckets = 0;
    numberOfChains = 0;

    bucket = NULL;
    chain = NULL;
}

HashTable::~HashTable(){
    if (bucket){
        delete bucket;
    }
    if (chain){
        delete chain;
    }
}

