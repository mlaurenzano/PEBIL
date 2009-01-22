#include <GnuVersymTable.h>

#include <ElfFile.h>

uint32_t GnuVersymTable::addSymbol(uint16_t val){
    ASSERT(versyms && "symbols array should be initialized");
    uint16_t* newsyms = new uint16_t[numberOfVersyms+1];

    for (uint32_t i = 0; i < numberOfVersyms; i++){
        newsyms[i] = versyms[i];
    }
    newsyms[numberOfVersyms] = val;

    delete[] versyms;
    versyms = newsyms;
    numberOfVersyms++;

    sizeInBytes += entrySize;

    ASSERT(sizeInBytes == entrySize*numberOfVersyms && "Section size does not match data size");
}


GnuVersymTable::GnuVersymTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(ElfClassTypes_GnuVersymTable,rawPtr,size,scnIdx,elf)
{
    if (elfFile->is64Bit()){
        entrySize = Size__64_bit_Gnu_Versym;
    } else {
        entrySize = Size__32_bit_Gnu_Versym;
    }

    ASSERT(entrySize == sizeof(uint16_t) && "The size of the entries is different than expected");
    ASSERT(size % entrySize == 0 && "This size of the section should be divisible by entry size");
    numberOfVersyms = size / entrySize;
    ASSERT(size == numberOfVersyms*entrySize && "Section size does not match data size");

    versyms = new uint16_t[numberOfVersyms];
}

GnuVersymTable::~GnuVersymTable(){
    if (versyms){
        delete[] versyms;
    }
}

void GnuVersymTable::print(){
    PRINT_INFOR("Gnu Version Symbol Table: section %d with %d entries", sectionIndex, numberOfVersyms);
    for (uint32_t i = 0; i < numberOfVersyms; i++){
        PRINT_INFOR("\tVersym (%d)\t: %hd", i, versyms[i]);
    }
}

void GnuVersymTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;

    for (uint32_t i = 0; i < numberOfVersyms; i++){
        binaryOutputFile->copyBytes(charStream()+entrySize*i,entrySize,offset+currByte);
        currByte += entrySize;
    }
    ASSERT(currByte == sizeInBytes && "Size written to file does not match theoretical size");
}

uint32_t GnuVersymTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(getFilePointer());
    uint32_t totalBytesRead = 0;

    for (uint32_t i = 0; i < numberOfVersyms; i++){
        binaryInputFile->copyBytesIterate(charStream()+entrySize*i,entrySize);
        totalBytesRead += entrySize;
    }
    ASSERT(totalBytesRead == sizeInBytes && "Size read from file does not match theoretical size");
    return totalBytesRead;
}
