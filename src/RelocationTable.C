#include <SymbolTable.h>
#include <RelocationTable.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <SectionHeader.h>
#include <RawSection.h>

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
    if (elfFile->is64Bit() && type == ElfRelType_rel){
        PRINT_INFOR("RelocationTable64(%d): REL_, %d entries, section %d", index, numberOfRelocations, getSectionIndex());
    } else if (elfFile->is64Bit() && type == ElfRelType_rela){
        PRINT_INFOR("RelocationTable64(%d): RELA, %d entries, section %d", index, numberOfRelocations, getSectionIndex());
    } else if (!elfFile->is64Bit() && type == ElfRelType_rel){
        PRINT_INFOR("RelocationTable32(%d): REL_, %d entries, section %d", index, numberOfRelocations, getSectionIndex());
    } else {
        PRINT_INFOR("RelocationTable32(%d): RELA, %d entries, section %d", index, numberOfRelocations, getSectionIndex());
    }

    PRINT_INFOR("\tSymbol table section   : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_link));
    PRINT_INFOR("\tSection for relocation : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_info));

    if (type == ElfRelType_rel){
        PRINT_INFOR("Type(index):\tOffset\t\t\tInfo\t\t\tSymbol\tType");
    } else {
        PRINT_INFOR("Type(index):\tOffset\t\t\tInfo\t\t\tSymbol\tType\tAddend");
    }

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        relocations[i]->print();
    }
}

void Relocation32::print(){
    PRINT_INFOR("Relocation32(%d):\t0x%016llx\t0x%016llx\t%d\t0x%04x", index, GET(r_offset), GET(r_info), ELF32_R_SYM(GET(r_info)), ELF32_R_TYPE(GET(r_info)));
}
void Relocation64::print(){
    PRINT_INFOR("Relocation64(%d):\t0x%016llx\t0x%016llx\t%d\t0x%04x", index, GET(r_offset), GET(r_info), ELF64_R_SYM(GET(r_info)), ELF64_R_TYPE(GET(r_info)));
}
void RelocationAddend32::print(){
    PRINT_INFOR("RelAddend32(%d):\t0x%016llx\t0x%016llx\t%d\t0x%04x\t0x%016llx", index, GET(r_offset), GET(r_info), ELF32_R_SYM(GET(r_info)), ELF32_R_TYPE(GET(r_info)), GET(r_addend));
}
void RelocationAddend64::print(){
    PRINT_INFOR("RelAddend64(%d):\t0x%016llx\t0x%016x\t%d\t0x%04x\t0x%016llx", index, GET(r_offset), GET(r_info), ELF64_R_SYM(GET(r_info)), ELF64_R_TYPE(GET(r_info)), GET(r_addend));
}

uint32_t Relocation32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation)){
        PRINT_ERROR("Relocation (32) can not be read");
    }

    return Size__32_bit_Relocation;
}

uint32_t Relocation64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Relocation)){
        PRINT_ERROR("Relocation (64) can not be read");
    }

    return Size__64_bit_Relocation;
}

uint32_t RelocationAddend32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation_Addend)){
        PRINT_ERROR("Relocation Addend (32) can not be read");
    }

    return Size__32_bit_Relocation_Addend;
}

uint32_t RelocationAddend64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Relocation_Addend)){
        PRINT_ERROR("Relocation Addend (64) can not be read");
    }

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
