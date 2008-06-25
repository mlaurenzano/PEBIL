#include <Base.h>
#include <ProgramHeader.h>
#include <BinaryFile.h>
#include <CStructuresElf.h>

bool ProgramHeader::inRange(uint64_t addr){
    uint64_t minAddr = GET(p_vaddr);
    uint64_t maxAddr = GET(p_vaddr) + GET(p_memsz);

    if (addr >= minAddr && addr < maxAddr){
        return true;
    }
    return false;
}

void ProgramHeader32::setOffset(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new Program Header offset: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.p_offset = (uint32_t)newVal;
}

void ProgramHeader32::setVirtualAddress(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new Program Header vaddr: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.p_vaddr = (uint32_t)newVal;
}

void ProgramHeader32::setPhysicalAddress(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new Program Header paddr: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.p_paddr = (uint32_t)newVal;
}

void ProgramHeader32::setMemorySize(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new Program Header memsz: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.p_memsz = (uint32_t)newVal;
}

void ProgramHeader32::setFileSize(uint64_t newVal){
    if ((uint32_t)newVal != newVal){
        PRINT_WARN("Losing bits when casting new Program Header filesz: %d != %lld", (uint32_t)newVal, newVal);
    }
    entry.p_filesz = (uint32_t)newVal;
}

void ProgramHeader64::setVirtualAddress(uint64_t newVal){
    entry.p_vaddr = newVal;
}

void ProgramHeader64::setOffset(uint64_t newVal){
    entry.p_offset = newVal;
}

void ProgramHeader64::setPhysicalAddress(uint64_t newVal){
    entry.p_paddr = newVal;
}

void ProgramHeader64::setMemorySize(uint64_t newVal){
    entry.p_memsz = newVal;
}

void ProgramHeader64::setFileSize(uint64_t newVal){
    entry.p_filesz = newVal;
}

void ProgramHeader::print() { 
    char sizeStr[3];
    if (getSizeInBytes() == Size__32_bit_Program_Header){
        sprintf(sizeStr,"32");
    } else {
        sprintf(sizeStr,"64");
    }

    PRINT_INFOR("ProgHdr%2s(%d):\t0x%08x\t0x%016llx\t0x%016llx\t0x%016llx", 
                sizeStr, index, GET(p_type), GET(p_filesz), GET(p_vaddr), GET(p_paddr));
    PRINT_INFOR("\t\t\t0x%08x\t0x%016llx\t0x%016llx\t0x%016llx",
                GET(p_offset), GET(p_memsz), GET(p_flags), GET(p_align));
}


uint32_t ProgramHeader32::read(BinaryInputFile* binaryInputFile){
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Program_Header)){
        PRINT_ERROR("Program header (32) can not be read");
    }
    return Size__32_bit_Program_Header;
}

uint32_t ProgramHeader64::read(BinaryInputFile* binaryInputFile){
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Program_Header)){
        PRINT_ERROR("Program header (64) can not be read");
    }
    return Size__64_bit_Program_Header;
}

bool ProgramHeader::verify(){
    if (GET(p_type) == PT_LOAD){
        if (GET(p_filesz) > GET(p_memsz)){
            PRINT_ERROR("File size may not be greater than memory size for a loadable segment");
        }
    }

    if (!isPowerOfTwo(GET(p_align)) && GET(p_align) != 0){
        PRINT_ERROR("Segment alignment must be 0 or a positive power of 2");
        return false;
    }

    if (GET(p_align) > 1){
        if (GET(p_vaddr) % GET(p_align) != GET(p_offset) % GET(p_align)){
            PRINT_ERROR("Segment(%d) virtual address(%016llx)/offset(%08x) does not conform to alignment(%016llx)", index, GET(p_vaddr), GET(p_offset), GET(p_align));
            return false;
        }
    }

    if (GET(p_vaddr) % DEFAULT_PAGE_ALIGNMENT != GET(p_paddr) % DEFAULT_PAGE_ALIGNMENT){
        PRINT_ERROR("Physical and virtual address must be congruent mod pagesize");
    }

}

void ProgramHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Program_Header,offset);
}

void ProgramHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Program_Header,offset);
}


