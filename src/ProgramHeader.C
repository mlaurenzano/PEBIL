#include <ProgramHeader.h>

#include <Base.h>
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

const char* PTypeNames[] = { "NULL","LOAD","DYNAMIC","INTERP","NOTE","SHLIB","PHDR" };

void ProgramHeader::print() { 

    PRINT_INFOR("PHeader : %d",index);
    if(GET(p_type) <= PT_PHDR){
        PRINT_INFOR("\tPtyp : %s",PTypeNames[GET(p_type)]);
    } else {
        char* ptr = "UNK";
        switch(GET(p_type)){
            case PT_LOOS         : ptr = "LOOS"; break;
            case PT_GNU_EH_FRAME : ptr = "GNU_EH_FRAME"; break;
            case PT_GNU_STACK    : ptr = "GNU_STACK"; break;
            case PT_LOSUNW       : ptr = "LOSUNW"; break;
            case PT_SUNWSTACK    : ptr = "SUNWSTACK"; break;
            case PT_HIOS         : ptr = "HIOS"; break;
            case PT_LOPROC       : ptr = "LOPROC"; break;
            case PT_HIPROC       : ptr = "HIPROC"; break;
            default              : ptr = "UNK"; break;
        }
        PRINT_INFOR("\tPtyp : %s (%#x)",ptr,GET(p_type));
    }

    PRINT_INFOR("\tPoff : @%llx for %lluB",GET(p_offset),GET(p_filesz));
    PRINT_INFOR("\tPvad : %#llx for %lluB",GET(p_vaddr),GET(p_memsz));
    PRINT_INFOR("\tPpad : %#llx",GET(p_paddr));
    uint32_t alignment = GET(p_align);
    for(uint32_t i=0;i<32;i++){
        if((alignment >> i) & 0x1){
            alignment = i;
        }
    }
    PRINT_INFOR("\talig : 2**%u",alignment);

    uint32_t flags  = GET(p_flags);
    PRINT_INFOR("\tflgs : %c%c%c%c%c",ISPF_R(flags) ? 'r' : '-',
                                      ISPF_W(flags) ? 'w' : '-',
                                      ISPF_X(flags) ? 'x' : '-',
                                      flags == PF_MASKOS ? 'o' : '-',
                                      flags == PF_MASKPROC ? 'p' : '-');
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
            PRINT_ERROR("File size (%d) may not be greater than memory size (%d) for a loadable segment", GET(p_filesz), GET(p_memsz));
            return false;
        }
    }

    if (!isPowerOfTwo(GET(p_align)) && GET(p_align) != 0){
        PRINT_ERROR("Segment alignment must be 0 or a positive power of 2");
        return false;
    }

    if (GET(p_align) > 1){
        if (GET(p_vaddr) % GET(p_align) != GET(p_offset) % GET(p_align)){
            PRINT_ERROR("Segment(%d) virtual address(%016llx) and offset(%016llx) do not conform to alignment(%016llx)", index, GET(p_vaddr), GET(p_offset), GET(p_align));
            return false;
        }
    }

    if (GET(p_vaddr) % DEFAULT_PAGE_ALIGNMENT != GET(p_paddr) % DEFAULT_PAGE_ALIGNMENT){
        PRINT_ERROR("Physical and virtual address must be congruent mod pagesize for program header %d", index);
        return false;
    }

    return true;
}

void ProgramHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Program_Header,offset);
}

void ProgramHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Program_Header,offset);
}


