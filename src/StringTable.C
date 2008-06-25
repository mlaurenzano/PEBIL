#include <StringTable.h>
#include <BinaryFile.h>

char* StringTable::getString(uint32_t offset){
    ASSERT(offset >= 0 && offset < sizeInBytes && "Cannot look up a string outside the string table"); 
    return strings + offset; 
}

void StringTable::addString(const char* name){
    PRINT_INFOR("Stringtable::addstring -- adding string %s with length %d", name, strlen(name));

    ASSERT(strings && "strings array should be initialized");

    char* newstrings = new char[sizeInBytes + strlen(name) + 1];
    memcpy(newstrings, strings, sizeInBytes);
    memcpy(newstrings + sizeInBytes, name, strlen(name));
    PRINT_INFOR("new string: %s", newstrings + sizeInBytes);
    newstrings[sizeInBytes + strlen(name)] = '\0';

    delete[] strings;
    strings = newstrings;
    PRINT_INFOR("size=%d", sizeInBytes);
    sizeInBytes += strlen(name) + 1;
    PRINT_INFOR("size=%d", sizeInBytes);

    // now we must also update this sections header
    SectionHeader* scnHdr = elfFile->getSectionHeader(sectionIndex);
    if (elfFile->is64Bit()){
    } else {
        Elf32_Shdr scnHdrEntry;
        memcpy(&scnHdrEntry, ((SectionHeader32*)scnHdr)->charStream(), sizeof(Elf32_Shdr));
        scnHdrEntry.sh_size = sizeInBytes;
        memcpy(((SectionHeader32*)scnHdr)->charStream(), &scnHdrEntry, sizeof(Elf32_Shdr));
    }

    print();

    ASSERT(scnHdr->GET(sh_size) == sizeInBytes && "Section header size attribute should be equal to the actual size of the section");
}

void StringTable::print() { 

    PRINT_INFOR("StringTable(%d): Section %d, %d bytes, at address %#x", index, getSectionIndex(), sizeInBytes, getFilePointer());

    for (uint32_t currByte = 0; currByte < sizeInBytes; currByte++){
        char* ptr = getString(currByte);
        PRINT_INFOR("%9d: %s",currByte,ptr);
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
