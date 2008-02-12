#include <Base.h>
#include <ProgramHeader.h>
#include <BinaryFile.h>

void ProgramHeader32::print() { 
    PRINT_INFOR("PROGRAMHEADER32(%d)", index);
    PRINT_INFOR("\tType(%#x)\tOffset(%#x)\tVaddr(%#x)\tPaddr(%#x)", entry.p_type, entry.p_offset, entry.p_vaddr, entry.p_paddr);
    PRINT_INFOR("\tFileSz(%#lx -- %d)\tMemSz(%#lx -- %d)\tFlags(%#x)\tAlign(%d)", entry.p_filesz, entry.p_filesz, entry.p_memsz, 
entry.p_memsz, entry.p_flags, entry.p_align);
}
void ProgramHeader64::print() { 
    PRINT_INFOR("PROGRAMHEADER64(%d)", index);
    PRINT_INFOR("\tType(%#x)\tOffset(%#llx)\tVaddr(%#llx)\tPaddr(%#llx)", entry.p_type, entry.p_offset, entry.p_vaddr, entry.p_paddr);
    PRINT_INFOR("\tFileSz(%#llx -- %d)\tMemSz(%#llx -- %d)\tFlags(%#x)\tAlign(%d)", entry.p_filesz, entry.p_filesz, entry.p_memsz, 
entry.p_memsz, entry.p_flags, entry.p_align);
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
        if (GET(p_vaddr) != GET(p_offset) % GET(p_align)){
            PRINT_ERROR("Segment virtual address does not conform to alignment");
            return false;
        }
    }
}

void ProgramHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Program_Header,offset);
}

void ProgramHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Program_Header,offset);
}


