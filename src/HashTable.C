#include <HashTable.h>

uint32_t HashTable::findSymbol(const char* symbolName){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);
    uint32_t x = bucket[elf_hash(symbolName)%numberOfBuckets];
    uint32_t chainVal;

    while (strcmp(symbolName,symTab->getSymbolName(x))){
        if (x == chain[x]){
            PRINT_ERROR("The symbol being searched (%s) is non-existent", symbolName);
            return -1;
        }
        x = chain[x];
    }
    return x;
}


bool HashTable::verify(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    if (numberOfChains != symTab->getNumberOfSymbols()){
        PRINT_ERROR("In the hash table, the number of chains should be equal to the number of symbols in the corresponding symbol table");
    }

    for (uint32_t i = 1; i < numberOfChains; i++){
        if (findSymbol(symTab->getSymbolName(i)) != i){
            PRINT_ERROR("Hash Table search function is erroneous");
        }
    }
    return true;
}

void HashTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    uint32_t tmpEntry;

    binaryOutputFile->copyBytes((char*)&numberOfBuckets,hashEntrySize,offset+currByte);
    currByte += hashEntrySize;

    binaryOutputFile->copyBytes((char*)&numberOfChains,hashEntrySize,offset+currByte);
    currByte += hashEntrySize;

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        binaryOutputFile->copyBytes((char*)&bucket[i],hashEntrySize,offset+currByte);
        currByte += hashEntrySize;
    }

    for (uint32_t i = 0; i < numberOfChains; i++){
        binaryOutputFile->copyBytes((char*)&chain[i],hashEntrySize,offset+currByte);
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
    bucket = new uint32_t[numberOfBuckets];

    if (!binaryInputFile->copyBytesIterate((char*)&numberOfChains, hashEntrySize)){
        PRINT_ERROR("Cannot read nchain from Hash Table");
    }
    chain = new uint32_t[numberOfChains];

    ASSERT(sizeInBytes == hashEntrySize*(numberOfBuckets + numberOfChains + 2) && "Hash Table size is inconsistent with its internal information");

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&bucket[i], hashEntrySize)){
            PRINT_ERROR("Cannot read bucket[%d] from Hash Table", i);
        }
    }

    for (uint32_t i = 0; i < numberOfChains; i++){
        if (!binaryInputFile->copyBytesIterate((char*)&chain[i], hashEntrySize)){
            PRINT_ERROR("Cannot read chain[%d] from Hash Table)", i);
        }
    }

    return sizeInBytes;

}


void HashTable::print(){
    SymbolTable* symTab = elfFile->getSymbolTable(symTabIdx);

    PRINT_INFOR("Hash Table: section %hd, %d buckets, %d chains", sectionIndex, numberOfBuckets, numberOfChains);

    printBytes(0,0);

    for (uint32_t i = 0; i < numberOfBuckets; i++){
        PRINT_INFOR("BKT[%d]\t%d", i, bucket[i]);
    }
    for (uint32_t i = 0; i < numberOfChains; i++){
        PRINT_INFOR("CHN[%d]\t%d\t%s", i, chain[i], symTab->getSymbolName(i));
    }
}

uint32_t HashTable::getBucket(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfBuckets && "index into Hash Table bucket array is out of bounds");
    ASSERT(bucket && "bucket array should be initialized");

    return bucket[idx];
}

uint32_t HashTable::getChain(uint32_t idx){
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

