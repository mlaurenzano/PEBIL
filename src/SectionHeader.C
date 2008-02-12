#include <SectionHeader.h>
#include <BinaryFile.h>

#define DWARF_SCN_NAME_PREFIX ".debug_\0"

ElfClassTypes SectionHeader::setSectionType(){
//    PRINT_INFOR("Setting type for section %d", index);    
//    print();
    if (GET(sh_type) == SHT_STRTAB){
        sectionType = ElfClassTypes_string_table;
    } else if (GET(sh_type) == SHT_SYMTAB || GET(sh_type) == SHT_DYNSYM){
        sectionType = ElfClassTypes_symbol_table;
    } else if (GET(sh_type) == SHT_REL || GET(sh_type) == SHT_RELA){
        sectionType = ElfClassTypes_relocation_table;
    } else if (GET(sh_type) == SHT_PROGBITS){
        if (!hasWriteBit() && !hasAllocBit() && !hasExecInstrBit()){
            sectionType = ElfClassTypes_dwarf_section;
        }

    }

//    PRINT_INFOR("Found type %d", sectionType);
    return sectionType;
}

bool SectionHeader::hasWriteBit(){
    return (GET(sh_flags) & SHF_WRITE);
}
bool SectionHeader::hasAllocBit(){
    return (GET(sh_flags) & SHF_ALLOC);
}
bool SectionHeader::hasExecInstrBit(){
    return (GET(sh_flags) & SHF_EXECINSTR);
}

void SectionHeader::initFilePointers(BinaryInputFile* binaryInputFile){
}

bool SectionHeader::inRange(uint64_t address){ 
    return true;
}

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

void SectionHeader32::print() { 
    PRINT_INFOR("SECTIONHEADER32(%d)", index);

    PRINT_INFOR("\tName(%d -- %s)\tType(%#x)\tFlags(%#x)\tMemVaddr(%#x)\tFilAddr(%#x)", entry.sh_name, sectionNamePtr, 
        entry.sh_type, entry.sh_flags, entry.sh_addr, entry.sh_offset);
    PRINT_INFOR("\tSize(%d)\tLink(%d)\tInfo(%#x)\tAlign(%d)\tEntrySz(%d)", entry.sh_size, entry.sh_link, entry.sh_info, entry.sh_addralign, entry.sh_entsize); 
}
void SectionHeader64::print() { 
    PRINT_INFOR("SECTIONHEADER64(%d)", index);

    PRINT_INFOR("\tName(%d -- %s)\tType(%#x)\tFlags(%#llx)\tMemVaddr(%#llx)\tFilAddr(%#llx)", entry.sh_name, sectionNamePtr, 
        entry.sh_type, entry.sh_flags, entry.sh_addr, entry.sh_offset);
    PRINT_INFOR("\tSize(%d)\tLink(%d)\tInfo(%#x)\tAlign(%d)\tEntrySz(%d)", entry.sh_size, entry.sh_link, entry.sh_info, entry.sh_addralign, entry.sh_entsize); 
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

void SectionHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Section_Header,offset);
}

void SectionHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Section_Header,offset);
}

