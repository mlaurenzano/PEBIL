#include <SectionHeader.h>
#include <BinaryFile.h>

SectionHeader32::SectionHeader32(uint16_t idx)
    : SectionHeader()
{ 
    sizeInBytes = Size__32_bit_Section_Header; 
    index = idx; 
    bzero(&entry,sizeof(Elf32_Shdr));
}

SectionHeader64::SectionHeader64(uint16_t idx)
    : SectionHeader()
{ 
    sizeInBytes = Size__64_bit_Section_Header; 
    index = idx; 
    bzero(&entry,sizeof(Elf64_Shdr));
}

ElfClassTypes SectionHeader::setSectionType(){
    if (GET(sh_type) == SHT_STRTAB){
        sectionType = ElfClassTypes_string_table;
    } else if (GET(sh_type) == SHT_SYMTAB || GET(sh_type) == SHT_DYNSYM){
        sectionType = ElfClassTypes_symbol_table;
    } else if (GET(sh_type) == SHT_REL || GET(sh_type) == SHT_RELA){
        sectionType = ElfClassTypes_relocation_table;
    } else if (GET(sh_type) == SHT_PROGBITS){
        if (!hasWriteBit() && !hasAllocBit() && !hasExecInstrBit()){
            sectionType = ElfClassTypes_dwarf_section;
        } else if (hasExecInstrBit()){
            sectionType = ElfClassTypes_text_section;
        }
    } else if (GET(sh_type) == SHT_HASH || GET(sh_type) == SHT_GNU_HASH){
        sectionType = ElfClassTypes_hash_table;
    } else if (GET(sh_type) == SHT_NOTE){
        sectionType = ElfClassTypes_note_section;
    }
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
    uint64_t minAddr = GET(sh_addr);
    uint64_t maxAddr = GET(sh_addr) + GET(sh_size);
    if (address >= minAddr && address < maxAddr){
        return true;
    }
    return false;
}

bool SectionHeader::verify() {
    if (index == 0){
        PRINT_INFOR("Verifying section 0");
        if (GET(sh_link) != SHN_UNDEF ||
            GET(sh_info) != 0         ||
            GET(sh_addralign) != 0    ||
            GET(sh_entsize) != 0){

            PRINT_ERROR("First section header must be unused");
            return false;
        }
    }

    if (index >= SHN_LORESERVE){
        // currently this will always be true since SHN_HIRESERVE=0xffff
        if (index <= SHN_HIRESERVE){
            PRINT_ERROR("Section header table cannot use reserved indices");
            return false;
        }
    }

    if(hasAllocBit()){
        if (GET(sh_addr) == 0){
            PRINT_ERROR("This section will reside in memory, but has no address");
            return false;
        }
    }

    if (!isPowerOfTwo(GET(sh_addralign)) && GET(sh_addralign) != 0){
        PRINT_ERROR("Section's address alignment must be 0 or a power of 2");
        return false;
    }
    if (GET(sh_addralign) != 0){
        if (GET(sh_addr) % GET(sh_addralign) != 0){
            print();
            PRINT_ERROR("Section (%d) address does not conform to alignment", index);
            return false;
        }
    }
    return true;
}

void SectionHeader::print() { 
    char sizeStr[3];
    if (getSizeInBytes() == Size__32_bit_Section_Header){
        sprintf(sizeStr,"32");
    } else {
        sprintf(sizeStr,"64");
    }

    PRINT_INFOR("SecHdr%s(%hd):\t%16s\t0x%08x\t\t0x%016llx\t0x%016llx\t0x%016llx",
                sizeStr, index, getSectionNamePtr(), GET(sh_type), GET(sh_flags), GET(sh_addr), GET(sh_offset));
    PRINT_INFOR("\t\t\t%8d\t\t0x%08x\t\t0x%016llx\t0x%016llx\t0x%016llx",
                GET(sh_link), GET(sh_info), GET(sh_size), GET(sh_addralign), GET(sh_entsize));

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
  //    PRINT_INFOR("dumping section header %d", index);
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Section_Header,offset);
}

void SectionHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Section_Header,offset);
}

