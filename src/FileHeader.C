#include <BinaryFile.h>
#include <FileHeader.h>

void FileHeader32::setSectionHeaderOffset(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new File Header shoff: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.e_shoff = (uint32_t)newVal;
}

void FileHeader64::setSectionHeaderOffset(uint64_t newVal){
    entry.e_shoff = newVal;
}

bool FileHeader::verify(uint16_t targetSize){
    if (!ISELFMAGIC(GET(e_ident)[EI_MAG0],GET(e_ident)[EI_MAG1],GET(e_ident)[EI_MAG2],GET(e_ident)[EI_MAG3])){
        PRINT_ERROR("Magic number incorrect");
        return false;
    }
    if (GET(e_ident)[EI_DATA] == ELFDATANONE){
        PRINT_ERROR("Elf data invalid");
        return false;
    }
    if (GET(e_ident)[EI_VERSION] != EV_CURRENT){
        PRINT_ERROR("Elf version must be current");
        return false;
    }
    if (GET(e_ident)[EI_ABIVERSION] != 0){
        PRINT_ERROR("ABI version should be 0");
        return false;
    }
    if (GET(e_type) == ET_NONE){
        PRINT_ERROR("File type unknown");
        return false;
    }
    uint32_t machine_typ = GET(e_machine);
    if (machine_typ != EM_386 && 
        machine_typ != EM_860 && 
        machine_typ != EM_X86_64){
        PRINT_ERROR("Machine type must be intel or amd x86");
        return false;
    }
    if (GET(e_version) == EV_NONE){
        PRINT_ERROR("Elf header version unknown");
        return false;
    }
    if (!GET(e_phoff)){
        if (GET(e_phnum) != 0){
            PRINT_ERROR("program header entries must be zero if no program header is present");
            return false;
        }
    }
    if (!GET(e_shoff)){
        if (GET(e_shnum) != 0){
            PRINT_ERROR("section header entries must be zero if no section header is present");
            return false;
        }
        if (GET(e_shstrndx) != SHN_UNDEF){
            PRINT_ERROR("section header entries must be zero if no section header is present");
            return false;
        }
    }
    if (GET(e_ident)[EI_CLASS] == ELFCLASS64){
        if (GET(e_ehsize) != Size__64_bit_File_Header){
            PRINT_ERROR("File header size is wrong: %d != %d", GET(e_ehsize), Size__64_bit_File_Header);
            return false;
        }

        if (GET(e_phentsize) != Size__64_bit_Program_Header){
            PRINT_ERROR("Program header size is wrong: %d != %d", GET(e_phentsize), Size__64_bit_Program_Header);
            return false;           
        }
        if (GET(e_shentsize) != Size__64_bit_Section_Header){
            PRINT_ERROR("Section header size is wrong", GET(e_shentsize), Size__64_bit_Section_Header);
            return false;
        }
        
    } else if (GET(e_ident)[EI_CLASS] == ELFCLASS32){
        if (GET(e_ehsize) != Size__32_bit_File_Header){
            PRINT_ERROR("File header size is wrong");
            return false;
        }
        if (GET(e_phentsize) != Size__32_bit_Program_Header){
            PRINT_ERROR("Program header size is wrong; %d != %d", GET(e_phentsize), Size__32_bit_Program_Header);
            return false;           
        }
        if (GET(e_shentsize) != Size__32_bit_Section_Header){
            PRINT_ERROR("Section header size is wrong");
            return false;
        }
    } else if (GET(e_ident)[EI_CLASS] == ELFCLASSNONE){
        PRINT_ERROR("Elf file class defined as none");
        return false;
    } else {
        PRINT_ERROR("Elf file class invalid");
        return false;
    }
    return true;
}


void FileHeader::print() { 

    if (getSizeInBytes() == Size__32_bit_File_Header){
        PRINT_INFOR("FileHeader32");
    } else {
        PRINT_INFOR("FileHeader64");
    }

    unsigned char* ident = GET(e_ident);

    PRINT_INFOR("Magic\t\tMagicStr\tClass\tData\tID-Ver\tOS-ABI\tABIVer\tType\tArch\tVersion\t\tEntry\t\t\tFlags");
    PRINT_INFOR("0x%08x\t0x%02x+%c%c%c\t0x%02x\t0x%02x\t0x%02x\t0x%02x\t0x%04hx\t0x%04hx\t0x%04hx\t0x%08x\t0x%016llx\t0x%08x", 
                ELFHDR_GETMAGIC, ident[EI_MAG0], ident[EI_MAG1], ident[EI_MAG2], ident[EI_MAG3], ident[EI_CLASS], 
                ident[EI_DATA], ident[EI_VERSION], ident[EI_OSABI], ident[EI_ABIVERSION], GET(e_type), GET(e_machine),
                GET(e_version), GET(e_entry), GET(e_flags));

    if (GET(e_phoff)){
        PRINT_INFOR("ProgHdr Table:\tFile Offset\t\tNum Entries\tEntry Size");
        PRINT_INFOR("\t\t\t0x%016llx\t%hd\t\t%hd", GET(e_phoff), GET(e_phnum), GET(e_phentsize));
    }
    if (GET(e_shoff)){
        PRINT_INFOR("SectHdr Table:\tFile Offset\t\tNum Entries\tEntry Size");
        PRINT_INFOR("\t\t\t0x%016llx\t%hd\t\t%hd", GET(e_shoff), GET(e_shnum), GET(e_shentsize));
    }

}


void FileHeader::initFilePointers(BinaryInputFile* binaryInputFile){

    if(GET(e_phoff)){
        programHeaderTablePtr = binaryInputFile->fileOffsetToPointer(GET(e_phoff));
    }
    if(GET(e_shoff)){
        sectionHeaderTablePtr = binaryInputFile->fileOffsetToPointer(GET(e_shoff));
    }

}

uint32_t FileHeader32::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_File_Header)){
        PRINT_ERROR("File header (32) can not be read");
    }

    verify(Size__32_bit_Program_Header);
    initFilePointers(binaryInputFile);

    return Size__32_bit_File_Header;
}

uint32_t FileHeader64::read(BinaryInputFile* binaryInputFile){
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_File_Header)){
        PRINT_ERROR("File header (64) can not be read");
    }

    verify(Size__64_bit_Program_Header);
    initFilePointers(binaryInputFile);

    return Size__64_bit_File_Header;
}

void FileHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_File_Header,offset);
}

void FileHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_File_Header,offset);
}

