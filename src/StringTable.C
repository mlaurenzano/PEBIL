#include <StringTable.h>
#include <BinaryFile.h>

char* StringTable::getString(uint32_t offset){
    ASSERT(offset >= 0 && offset < sizeInBytes && "Cannot look up a string outside the string table"); 
    return strings + offset; 
}

uint32_t StringTable::addString(const char* name){

    uint32_t stringSize = strlen(name);
    uint32_t currentSize = sizeInBytes;

    PRINT_INFOR("Stringtable::addstring -- adding string %s with length %d", name, strlen(name));

    ASSERT(strings && "strings array should be initialized");

    char* newstrings = new char[sizeInBytes + strlen(name) + 1];
    memcpy(newstrings, strings, sizeInBytes);
    memcpy(newstrings + sizeInBytes, name, strlen(name));
    newstrings[sizeInBytes + strlen(name)] = '\0';
    PRINT_INFOR("new string: %s", newstrings + sizeInBytes);

    delete[] strings;
    strings = newstrings;
    PRINT_INFOR("size=%d", sizeInBytes);
    sizeInBytes += strlen(name) + 1;
    PRINT_INFOR("size=%d", sizeInBytes);

    return currentSize;
}

void StringTable::print() { 

    PRINT_INFOR("tidx : %d aka sect %d %uB",index,getSectionIndex(),sizeInBytes);
    for (uint32_t currByte = 0; currByte < sizeInBytes; currByte++){
        char* ptr = getString(currByte);
        PRINT_INFOR("\t%9d : %s",currByte,ptr);
        currByte += strlen(ptr);
    }

}

void StringTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(strings, sizeInBytes, offset);
}

uint32_t StringTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(getFilePointer());
    setFileOffset(binaryInputFile->currentOffset());

    strings = new char[sizeInBytes];
    binaryInputFile->copyBytesIterate(strings, sizeInBytes);

    return sizeInBytes;
}

StringTable::~StringTable(){
    if (strings){
        delete[] strings;
    }
}
