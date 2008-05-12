#include <StringTable.h>
#include <BinaryFile.h>

char* StringTable::getString(uint32_t offset){
    ASSERT(offset < sizeInBytes); 
    if(!offset)
        return stringtable_entry_without_name;
    return getFilePointer() + offset; 
}

void StringTable::print() { 

    PRINT_INFOR("StringTable(%d): Section %d, %d bytes, at address %#x", index, getSectionIndex(), sizeInBytes, getFilePointer());

    for (uint32_t currByte = 0; currByte < sizeInBytes; currByte++){
        char* ptr = getString(currByte);
        // at some point we need to implement name demangling for C++
        PRINT_INFOR("%9d: %s",currByte,ptr);
        currByte += strlen(ptr);
    }

}

uint32_t StringTable::read(BinaryInputFile* binaryInputFile){
    
    binaryInputFile->setInPointer(getFilePointer());
    setFileOffset(binaryInputFile->currentOffset());

    DEBUG(
          uint32_t left = binaryInputFile->bytesLeftInBuffer();
          PRINT_DEBUG("%d bytesleft to the end and total string table size is %d",left,sizeInBytes);
          ASSERT((left == sizeInBytes) && "FATAL : Number of bytes in string table does not match");
          );
    return sizeInBytes;
}
