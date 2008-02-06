#include <StringTable.h>
#include <BinaryFile.h>
#include <SectionHeader.h>

char* StringTable::getString(uint32_t offset){
    PRINT_DEBUG("Offset is %d in %d",offset,sizeInBytes);
    ASSERT(offset < sizeInBytes); 
    if(!offset)
        return "";
    return stringTablePtr + offset; 
}

void StringTable::print() { 
    PRINT_INFOR("STRINGTABLE(%d): Section %d", index, sectionHeader->getIndex());
    if(stringTablePtr && sizeInBytes){
        PRINT_INFOR("\tSize  : %d, ptr(%#x)", sizeInBytes, stringTablePtr);
    } else {
        PRINT_INFOR("\tSize  : 0 EMPTY");
    }

    PRINT_INFOR("\tStrs  :");
    for (uint32_t currByte = 0; currByte < sizeInBytes; currByte++){
        char* ptr = (char*)(stringTablePtr + currByte);
        char* demangled = ptr;
        ASSERT(demangled && "FATAL : demangling should always return non-null pointer");

        PRINT_INFOR("%9d %s --- %s",currByte,ptr,demangled);

        currByte += strlen(ptr);
    }
}

uint32_t StringTable::read(BinaryInputFile* binaryInputFile){
    
    binaryInputFile->setInPointer(stringTablePtr);
    setFileOffset(binaryInputFile->currentOffset());

    DEBUG(
    uint32_t left = binaryInputFile->bytesLeftInBuffer();
    PRINT_DEBUG("%d bytesleft to the end and total string table size is %d",left,sizeInBytes);
    ASSERT((left == sizeInBytes) && "FATAL : Number of bytes in string table does not match");
    );
    return sizeInBytes;
}
