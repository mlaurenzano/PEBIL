#include <DwarfSection.h>
#include <BinaryFile.h>
#include <SectionHeader.h>
#include <LineInformation.h>


uint32_t DwarfLineInfoSection::read(BinaryInputFile* binaryInputFile){
    PRINT_INFOR("Reading DWARF line information at section %d", getSectionIndex());

    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    numberOfLineInfoTables = sizeInBytes / Size__Dwarf_LineInfo_Header;
    lineInfoTables = new LineInfoTable*[numberOfLineInfoTables];
    
    //    PRINT_INFOR("Using %d lineinfo tables to start, size is %d", numberOfLineInfoTables, sizeInBytes);

    numberOfLineInfoTables = 0;
    uint32_t currByte = 0;
    while (currByte < sizeInBytes){
        lineInfoTables[numberOfLineInfoTables] = new LineInfoTable(numberOfLineInfoTables,rawDataPtr+currByte,this);
        lineInfoTables[numberOfLineInfoTables]->read(binaryInputFile);
        currByte += lineInfoTables[numberOfLineInfoTables]->getSizeInBytes();
        numberOfLineInfoTables++;
    }
    LineInfoTable** dummy = new LineInfoTable*[numberOfLineInfoTables];
    for (uint32_t i = 0; i < numberOfLineInfoTables; i++){
        dummy[i] = lineInfoTables[i];
    }
    delete[] lineInfoTables;
    lineInfoTables = dummy;

    //    PRINT_INFOR("Using %d lineinfo tables -- total size is %d", numberOfLineInfoTables, currByte);

    ASSERT(currByte == sizeInBytes && "Number of bytes read from DwarfLineInfoSection does not match its size");
    verify();
    return sizeInBytes;
}

DwarfLineInfoSection::~DwarfLineInfoSection(){
    if (lineInfoTables){
        for (uint32_t i = 0; i < numberOfLineInfoTables; i++){
            delete lineInfoTables[i];
        }
        delete[] lineInfoTables;
    }
}

DwarfLineInfoSection::DwarfLineInfoSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : DwarfSection(filePtr, size, scnIdx, idx, elf) 
{ 
    type = ElfClassTypes_DwarfLineInfoSection; 

    lineInfoTables = NULL;
    numberOfLineInfoTables = 0;
}


void DwarfLineInfoSection::dump(BinaryOutputFile* b, uint32_t offset){
    RawSection::dump(b,offset);
}

void DwarfLineInfoSection::print(){

    PRINT_INFOR("Dwarf Line Info Section (%d)", getSectionIndex());

    printBytes(0,0);

    for (uint32_t i = 0; i < numberOfLineInfoTables; i++){
        lineInfoTables[i]->print();
    }
}

bool DwarfLineInfoSection::verify(){ 
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfLineInfoTables; i++){
        if (!lineInfoTables[i]->verify()){
            return false;
        }
        totalSize += lineInfoTables[i]->getSizeInBytes();
    }
    if (totalSize != sizeInBytes){
        PRINT_ERROR("Total size of DWARF Line Info section does not match the size of all of the line info tables it contains");
        return false;
    }

    return true;
}
