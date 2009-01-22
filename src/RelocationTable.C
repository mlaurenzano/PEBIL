#include <RelocationTable.h>

#include <BinaryFile.h>
#include <ElfFile.h>
#include <RawSection.h>
#include <SectionHeader.h>
#include <SymbolTable.h>

void RelocationTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentByte = 0;
    for (uint32_t i = 0; i < numberOfRelocations; i++){
        binaryOutputFile->copyBytes(relocations[i]->charStream(),relocationSize,offset+currentByte);
        currentByte += relocationSize;
    }
}


uint32_t RelocationTable::addRelocation(uint64_t offset, uint64_t info){

    Relocation** newRelocations = new Relocation*[numberOfRelocations+1];

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        newRelocations[i] = relocations[i];
    }

    if (elfFile->is64Bit()){
        if (type == ElfRelType_rela){
            RelocationAddend64* rel = new RelocationAddend64(NULL,numberOfRelocations);
            rel->SET(r_addend,0);
            newRelocations[numberOfRelocations] = rel;
        } else {
            newRelocations[numberOfRelocations] = new Relocation64(NULL,numberOfRelocations);
        }
    } else {
        if (type == ElfRelType_rela){
            RelocationAddend32* rel = new RelocationAddend32(NULL,numberOfRelocations);
            rel->SET(r_addend,0);
            newRelocations[numberOfRelocations] = rel;
        } else {
            newRelocations[numberOfRelocations] = new Relocation32(NULL,numberOfRelocations);
        }
    }

    newRelocations[numberOfRelocations]->SET(r_offset,offset);
    newRelocations[numberOfRelocations]->SET(r_info,info);

    delete[] relocations;

    relocations = newRelocations;
    numberOfRelocations++;
    sizeInBytes += relocationSize;

    // returns the offset of the new entry
    return numberOfRelocations-1;
}


RelocationTable::RelocationTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(ElfClassTypes_RelocationTable,rawPtr,size,scnIdx,elf),index(idx),symbolTable(NULL),relocationSection(NULL)
{
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(sectionIndex));

    sizeInBytes = size;
    uint32_t typ = elfFile->getSectionHeader(sectionIndex)->GET(sh_type);

    ASSERT((typ == SHT_REL || typ == SHT_RELA) && "Section header type field must be relocation");


    if (elfFile->is64Bit()){
        if (typ == SHT_RELA){
            relocationSize = Size__64_bit_Relocation_Addend;
            type = ElfRelType_rela;
        } else {
            relocationSize = Size__64_bit_Relocation;
            type = ElfRelType_rel;
        }
    } else {
        if (typ == SHT_RELA){
            relocationSize = Size__32_bit_Relocation_Addend;
            type = ElfRelType_rela;
        } else {
            relocationSize = Size__32_bit_Relocation;
            type = ElfRelType_rel;
        }
    }
    ASSERT(relocationSize && "Size of a relocation entry must be > 0");
    ASSERT(sizeInBytes % relocationSize == 0 && "Section size is bad");
    numberOfRelocations = sizeInBytes / relocationSize;

    relocations = new Relocation*[numberOfRelocations];
}


RelocationTable::~RelocationTable(){
    if (relocations){
        for (uint32_t i = 0; i < numberOfRelocations; i++){
            if (relocations[i]){
                delete relocations[i];
            }
        }
        delete[] relocations;
    }
}



uint16_t RelocationTable::setSymbolTable(){
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(getSectionIndex()));
    SectionHeader* sh = elfFile->getSectionHeader(getSectionIndex());
    ASSERT(elfFile->getRawSection(sh->GET(sh_link)));
    
    RawSection* sy = elfFile->getRawSection(sh->GET(sh_link));

    ASSERT(sy->getType() == ElfClassTypes_SymbolTable);
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
    PRINT_INFOR("RelocTable : %d aka sect %d with %d relocations",index,getSectionIndex(),numberOfRelocations);
    PRINT_INFOR("\tadd? : %s", type == ElfRelType_rela ? "yes" : "no");
    PRINT_INFOR("\tsect : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_info));
    PRINT_INFOR("\tstbs : %d", elfFile->getSectionHeader(getSectionIndex())->GET(sh_link));

    ASSERT(elfFile->getSectionHeader(getSectionIndex()) && "Section header doesn't exist");

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        char* namestr = NULL;
        Symbol* foundsymbols[3];
        symbolTable->findSymbol4Addr(relocations[i]->GET(r_offset),foundsymbols,3,&namestr);
        relocations[i]->print(namestr);
        delete[] namestr;
    }
}

const char* RTypeNames[] = { "noreloc","Direct","PCRelative","OffsetTable","LinkageTable",
                             "CopyAtRuntime","CreateGOTEntry","CreatePLTEntry","AdjustByBase",
                             "Offset2GOT","PCRelative2GOT","R_386_32PLT/R_X86_64_32S",
                             "Direct16Extended","Direct16ExtendedPCRelative",
                             "OffsetStaticTLS","GOTAddress4TLS","GOTEntry4TLS","OffsetRelativeToTLS",
                             "GNUVersionDirectGeneral","GNUVersionDirectLocal",
                             "_16","PC16","_8","PC8","GD_32","GD_PUSH","GD_CALL","GD_POP",
                             "LDM_32","LDM_PUSH","LDM_CALL","LDM_POP","LDO_32","IE_32","LE_32",
                             "DTPMOD","DTPOFF","TPOFF"};

void Relocation32::print(char* str){
    if(!str) str = "";
    PRINT_INFOR("\trel%5d -- off:%#12llx stx:%5lld %25s -- %s",index,GET(r_offset),ELF32_R_SYM(GET(r_info)),
                ELF32_R_TYPE(GET(r_info)) < R_386_NUM ? RTypeNames[ELF32_R_TYPE(GET(r_info))] : "UNK",str);
}
void Relocation64::print(char* str){
    if(!str) str = "";
    PRINT_INFOR("\trel%5d -- off:%#12llx stx:%5lld %25s -- %s",index,GET(r_offset),ELF64_R_SYM(GET(r_info)),
                ELF64_R_TYPE(GET(r_info)) <= R_386_NUM ? RTypeNames[ELF64_R_TYPE(GET(r_info))] : "UNK",str);
}
void RelocationAddend32::print(char* str){
    if(!str) str = "";
    PRINT_INFOR("\trel%5d -- off:%#12llx stx:%5lld ad:%12lld %25s -- %s",index,GET(r_offset),ELF32_R_SYM(GET(r_info)),
                GET(r_addend),
                ELF32_R_TYPE(GET(r_info)) <= R_386_NUM ? RTypeNames[ELF32_R_TYPE(GET(r_info))] : "UNK",str);
}
void RelocationAddend64::print(char* str){
    if(!str) str = "";
    PRINT_INFOR("\trel%5d -- off:%#12llx stx:%5lld ad:%12lld %25s -- %s",index,GET(r_offset),ELF64_R_SYM(GET(r_info)),
                GET(r_addend),
                ELF64_R_TYPE(GET(r_info)) <= R_386_NUM ? RTypeNames[ELF64_R_TYPE(GET(r_info))] : "UNK",str);
}

uint32_t Relocation32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation)){
        PRINT_ERROR("Relocation (32) can not be read");
    }

    verify();

    return Size__32_bit_Relocation;
}

uint32_t Relocation64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Relocation)){
        PRINT_ERROR("Relocation (64) can not be read");
    }

    verify();

    return Size__64_bit_Relocation;
}

uint32_t RelocationAddend32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Relocation_Addend)){
        PRINT_ERROR("Relocation Addend (32) can not be read");
    }

    verify();
    
    return Size__32_bit_Relocation_Addend;
}

uint32_t RelocationAddend64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(relocationPtr);

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Relocation_Addend)){
        PRINT_ERROR("Relocation Addend (64) can not be read");
    }

    verify();

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

    verify();

    return sizeInBytes;
}
