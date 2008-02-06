#include <SymbolTable.h>
#include <RelocationTable.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <SectionHeader.h>

RelocationTable::RelocationTable(char* ptr, uint32_t sz, uint32_t nr, ElfRelType typ, ElfFile* elf, uint32_t idx, SectionHeader* sh)
        : Base(ElfClassTypes_relocation),
          relocationTablePtr(ptr),relocationSize(sz),type(typ),
          symbolTable(NULL),elfFile(elf),index(idx),numberOfRelocations(nr),
          sectionHeader(sh)
{
    sizeInBytes = numberOfRelocations * relocationSize;
    relocations = new Relocation*[numberOfRelocations];
    symbolTable = elfFile->findSymbolTableWithSectionIdx(sectionHeader->GET(sh_link));
    sectionToRelocate = elfFile->getSectionHeader(sectionHeader->GET(sh_info));

    ASSERT(symbolTable && "Need a symbol table for every relocation table");
    ASSERT(sectionToRelocate && "Need a relocatable section for every relocation table");
}

void RelocationTable::print(){
    PRINT_INFOR("RELOCATIONTABLE(%d): type(%d), entries(%d), section(%d)", index, type, numberOfRelocations, getSectionIndex());
    PRINT_INFOR("\tSymbol table section   : %d", sectionHeader->GET(sh_link));
    PRINT_INFOR("\tSection for relocation : %d", sectionHeader->GET(sh_info));
    for (uint32_t i = 0; i < numberOfRelocations; i++){
        relocations[i]->print();
    }
}

void Relocation32::print(){
    PRINT_INFOR("REL32(%d):\t%#12x\t%#12x\t%d\t%#x", index, entry.r_offset, entry.r_info, ELF32_R_SYM(entry.r_info), ELF32_R_TYPE(entry.r_info));
}
void Relocation64::print(){
}
void RelocationAddend32::print(){
    PRINT_INFOR("RELA32(%d):\t ", index);
}
void RelocationAddend64::print(){
}

uint32_t Relocation32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation)){
        PRINT_ERROR("Relocation (32) can not be read");
    }

    return Size__32_bit_Relocation;
}

uint32_t Relocation64::read(BinaryInputFile* binaryInputFile){
    return Size__64_bit_Relocation;
}

uint32_t RelocationAddend32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation_Addend)){
        PRINT_ERROR("Relocation (32) can not be read");
    }

    return Size__32_bit_Relocation_Addend;
}

uint32_t RelocationAddend64::read(BinaryInputFile* binaryInputFile){
    return Size__64_bit_Relocation_Addend;
}

uint32_t RelocationTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationTablePtr);

    PRINT_INFOR("Reading %d relocations for reltable %d", numberOfRelocations, index);

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        if (elfFile->is64Bit() && type == ElfRelType_rel){
            relocations[i] = new Relocation64(relocationTablePtr + (i * Size__64_bit_Relocation), i);
        } else if (elfFile->is64Bit() && type == ElfRelType_rela){
            relocations[i] = new RelocationAddend64(relocationTablePtr + (i * Size__64_bit_Relocation_Addend), i);
        } else if (!elfFile->is64Bit() && type == ElfRelType_rel){
            relocations[i] = new Relocation32(relocationTablePtr + (i * Size__32_bit_Relocation), i);
        } else if (!elfFile->is64Bit() && type == ElfRelType_rela){
            relocations[i] = new RelocationAddend32(relocationTablePtr + (i * Size__32_bit_Relocation_Addend), i);
        } else {
            PRINT_ERROR("Relocation type is invalid");
        }

        relocations[i]->read(binaryInputFile);
    }

    return sizeInBytes;
}

/*
void Relocation::print(SymbolTable* symbolTable,uint32_t index){
    PRINT_INFOR("\tRLC [%3d](adr %#llx)(sym %9d)(sze %3d)(typ %3d)",
                index,GET(r_vaddr),GET(r_symndx),GET(r_rsize),GET(r_rtype));
    if(symbolTable){
        symbolTable->printSymbol(GET(r_symndx));
    }
}

void RelocationTable::print(){
    PRINT_INFOR("RELOCATIONTABLE");
    PRINT_INFOR("\tCount : %d",numOfRelocations);

    PRINT_INFOR("\tReloc :");
    for(uint32_t i = 0;i<numOfRelocations;i++){
        relocations[i]->print(symbolTable,i);
    }
}

uint32_t RelocationTable::read(BinaryInputFile* binaryInputFile){

    PRINT_DEBUG("Reading the Relocation table");

    binaryInputFile->setInPointer(relocationPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currSize = Size__32_bit_RelocationTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_RelocationTable_Entry;
    }

    uint32_t ret = 0;
    for(uint32_t i = 0;i<numOfRelocations;i++){
        if(getXCoffFile()->is64Bit()){
            relocations[i] = new Relocation64();
        } else {
            relocations[i] = new Relocation32();
        }
        binaryInputFile->copyBytesIterate(relocations[i]->charStream(),currSize);
        ret += currSize;
    }

    ASSERT((sizeInBytes == ret) && "FATAL : Somehow the number of read does not match");

    return sizeInBytes;
}
*/
