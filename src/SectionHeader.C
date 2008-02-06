//#include <RelocationTable.h>
//#include <LineInfoTable.h>
#include <SectionHeader.h>
#include <BinaryFile.h>

#define LINE_RELOC_OVERFLOW 0xffff

bool SectionHeader::hasWriteBit(){
    return (GET(sh_flags) & SHF_WRITE);
}
bool SectionHeader::hasAllocBit(){
    return (GET(sh_flags) & SHF_ALLOC);
}
bool SectionHeader::hasExecInstrBit(){
    return (GET(sh_flags) & SHF_EXECINSTR);
}

ElfSectType SectionHeader::getSectionType(){
    return ElfSectType_undefined;
}

void SectionHeader::initFilePointers(BinaryInputFile* binaryInputFile){
/*
    if((GET(s_size) != 0) && !IS_SECT_TYPE(OVRFLO) && !IS_SECT_TYPE(BSS)){
        rawDataPtr = binaryInputFile->fileOffsetToPointer(GET(s_scnptr));
    }
    if((GET(s_nreloc) != 0) && !IS_SECT_TYPE(OVRFLO)){
        numOfRelocations = (uint32_t)GET(s_nreloc);
        if(numOfRelocations){
            relocationPtr = binaryInputFile->fileOffsetToPointer(GET(s_relptr));
            ASSERT((IS_SECT_TYPE(DATA) || IS_SECT_TYPE(TEXT)) && 
                "FATAL : Only text and Data can have reloc");
        }
    }
    if((GET(s_nlnno) != 0) && !IS_SECT_TYPE(OVRFLO)){
        numOfLineInfo = (uint32_t)GET(s_nlnno);
        if(numOfLineInfo){
            ASSERT(IS_SECT_TYPE(TEXT) && 
                "FATAL : Only text and Data can have reloc");
            lineInfoPointer = binaryInputFile->fileOffsetToPointer(GET(s_lnnoptr));
        }
    }
*/
}

bool SectionHeader::inRange(uint64_t address){ 
/*
    if(!GET(s_vaddr) || IS_SECT_TYPE(OVRFLO)) 
        return false; 
    return (((GET(s_vaddr)) <= address) && (address < (GET(s_vaddr) + GET(s_size)))); 
*/
return true;
}

/*
RelocationTable* SectionHeader::readRelocTable(BinaryInputFile* binaryInputFile,XCoffFile* xcoff){
    relocationTable = NULL;
    if(!numOfRelocations || !relocationPtr)
        return NULL;

    relocationTable = new RelocationTable(relocationPtr,numOfRelocations,xcoff);
    relocationTable->read(binaryInputFile);
    return relocationTable;
}
*/

/*
LineInfoTable* SectionHeader::readLineInfoTable(BinaryInputFile* binaryInputFile,XCoffFile* xcoff){
    lineInfoTable = NULL;
    if(!numOfLineInfo || !lineInfoPointer)
        return NULL;

    lineInfoTable = new LineInfoTable(lineInfoPointer,numOfLineInfo,xcoff);
    lineInfoTable->read(binaryInputFile);
    return lineInfoTable;
}
*/

bool SectionHeader::verify() {
    if (index == 0){
        if (GET(sh_link) != SHN_UNDEF ||
            GET(sh_info) != 0         ||
            GET(sh_addralign) != 0    ||
            GET(sh_entsize) != 0){

            PRINT_ERROR("First section header must be unused");
            return false;
        }
    }

    if (index >= SHN_LORESERVE && index <= SHN_HIRESERVE){
        PRINT_ERROR("Section header table cannot use reserved indices");
        return false;
    }

    if(hasAllocBit()){
        if (GET(sh_addr) == 0){
            PRINT_ERROR("This section will reside in memory, but has no address");
            return false;
        }
    }

    if (!isPowerOfTwo(GET(sh_addralign)) && GET(sh_addralign) != 0){
        PRINT_ERROR("Section's address alignment musrt be 0 or a power of 2");
        return false;
    }
    if (GET(sh_addralign) != 0){
        if (GET(sh_addr) % GET(sh_addralign) != 0){
            PRINT_ERROR("Section address does not conform to alignment");
            return false;
        }
    }
    return true;
}

void SectionHeader::setOverFlowSection(SectionHeader* sh) { 
/*
    ASSERT((IS_SECT_TYPE(DATA) || IS_SECT_TYPE(TEXT)) &&
        "FATAL : Only text and Data can have reloc");

    overFlowSection = sh; 
    numOfRelocations = (uint32_t)(sh->GET(s_paddr));
    numOfLineInfo = (uint32_t)(sh->GET(s_vaddr));
*/
}

void SectionHeader32::print() { 
    PRINT_INFOR("SECTIONHEADER (%d)", index);

    PRINT_INFOR("\tName(%d -- %s)\tType(%#x)\tFlags(%#x)\tMemVaddr(%#x)\tFilAddr(%#x)", entry.sh_name, sectionNamePtr, 
        entry.sh_type, entry.sh_flags, entry.sh_addr, entry.sh_offset);
    PRINT_INFOR("\tSize(%d)\tLink(%d)\tInfo(%#x)\tAlign(%d)\tEntrySz(%d)", entry.sh_size, entry.sh_link, entry.sh_info, entry.sh_addralign, entry.sh_entsize); 
}
void SectionHeader64::print() { 
    PRINT_INFOR("SECTIONHEADER (%d)", index);

    PRINT_INFOR("\tName                      : %d",entry.sh_name);
    PRINT_INFOR("\tType                      : %#x",entry.sh_type);
    PRINT_INFOR("\tFlags                     : %#x",entry.sh_flags);
    PRINT_INFOR("\tVaddr in Memory           : %#x",entry.sh_addr);
    PRINT_INFOR("\tOffset in File            : %#x",entry.sh_offset);
    PRINT_INFOR("\tSize                      : %d bytes",entry.sh_size);
    PRINT_INFOR("\tHeader Table Idx Link     : %d",entry.sh_link);
    PRINT_INFOR("\tExtra Info                : %d",entry.sh_info);
    PRINT_INFOR("\tAlignment                 : %d",entry.sh_addralign);
    PRINT_INFOR("\tEntry Size                : %d",entry.sh_entsize);
}

uint32_t SectionHeader32::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Section_Header)){
        PRINT_ERROR("Section header (32) can not be read");
    }
    verify();
//    initFilePointers(binaryInputFile);
    
    return Size__32_bit_Section_Header;
}

uint32_t SectionHeader64::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Section_Header)){
        PRINT_ERROR("Section header (64) can not be read");
    }
//    verify();
//    initFilePointers(binaryInputFile);

    return Size__64_bit_Section_Header;
}


