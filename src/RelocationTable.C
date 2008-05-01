#include <SymbolTable.h>
#include <RelocationTable.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <SectionHeader.h>


uint16_t RelocationTable::setSymbolTable(){
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(getSectionIndex()));
    SectionHeader* sh = elfFile->getSectionHeader(getSectionIndex());
    ASSERT(elfFile->getRawSection(sh->GET(sh_link)));
    
    RawSection* sy = elfFile->getRawSection(sh->GET(sh_link));
    //    sh->print();
    //print();
    //sy->print();
    ASSERT(sy->getType() == ElfClassTypes_symbol_table);
    symbolTable = (SymbolTable*)sy;
    return symbolTable->getSectionIndex();
}

uint16_t RelocationTable::setRelocationSection(){
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(getSectionIndex()));
    SectionHeader* sh = elfFile->getSectionHeader(getSectionIndex());
    ASSERT(elfFile->getRawSection(sh->GET(sh_info)));
    
    RawSection* rs = elfFile->getRawSection(sh->GET(sh_info));
    relocationSection = rs;
    return relocationSection->getSectionIndex();
}


void RelocationTable::print(){
    PRINT_INFOR("RELOCATIONTABLE(%d): type(%d), entries(%d), section(%d)", index, type, numberOfRelocations, getSectionIndex());
    PRINT_INFOR("\tSymbol table section   : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_link));
    PRINT_INFOR("\tSection for relocation : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_info));
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
    binaryInputFile->setInPointer(getFilePointer());

    //    PRINT_INFOR("Reading %d relocations for reltable %d", numberOfRelocations, index);

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        if (elfFile->is64Bit() && type == ElfRelType_rel){
            relocations[i] = new Relocation64(getFilePointer() + (i * Size__64_bit_Relocation), i);
        } else if (elfFile->is64Bit() && type == ElfRelType_rela){
            relocations[i] = new RelocationAddend64(getFilePointer() + (i * Size__64_bit_Relocation_Addend), i);
        } else if (!elfFile->is64Bit() && type == ElfRelType_rel){
            relocations[i] = new Relocation32(getFilePointer() + (i * Size__32_bit_Relocation), i);
        } else if (!elfFile->is64Bit() && type == ElfRelType_rela){
            relocations[i] = new RelocationAddend32(getFilePointer() + (i * Size__32_bit_Relocation_Addend), i);
        } else {
            PRINT_ERROR("Relocation type %d is invalid", type);
        }

        relocations[i]->read(binaryInputFile);
    }

    return sizeInBytes;
}
