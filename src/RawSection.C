#include <RawSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>
#include <BinaryFile.h>
#include <SectionHeader.h>

RawSection::RawSection(ElfClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : Base(classType),rawDataPtr(rawPtr),sectionIndex(scnIdx),elfFile(elf) 
{ 
    sizeInBytes = size; 

    hashCode = HashCode((uint32_t)sectionIndex);
    PRINT_DEBUG_HASHCODE("Section %d Hashcode: 0x%04llx", (uint32_t)sectionIndex, hashCode.getValue());

    verify();
}

bool RawSection::verify(){
    if (!hashCode.isSection()){
        PRINT_ERROR("RawSection %d HashCode is malformed", (uint32_t)sectionIndex);
        return false;
    }
    return true;
}

SectionHeader* RawSection::getSectionHeader(){
    return elfFile->getSectionHeader(getSectionIndex());
}


void RawSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset)
{ 
    if (getType() != ElfClassTypes_RawSection && getType() != ElfClassTypes_no_type &&
        getType() != ElfClassTypes_DwarfSection && getType() != ElfClassTypes_DwarfLineInfoSection){
        PRINT_ERROR("You should implement the dump function for class type %d", getType());
    }
    binaryOutputFile->copyBytes(charStream(),getSizeInBytes(),offset); 
}

void RawSection::printBytes(uint32_t bytesPerWord, uint32_t bytesPerLine){
    if (bytesPerWord <= 0){
        bytesPerWord = 8;
    }
    if (bytesPerLine <= 0){
        bytesPerLine = 64;
    }
    
    uint32_t currByte = 0;

    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for section %d:", sectionIndex);
    for (currByte = 0; currByte < sizeInBytes; currByte++){     
        if (currByte % bytesPerLine == 0){
            if (currByte){
                fprintf(stdout, "\n");
            }
            fprintf(stdout, "(%16llx) %8x: ", getSectionHeader()->GET(sh_offset)+currByte, currByte);
        } else if (currByte && currByte % bytesPerWord == 0){
            fprintf(stdout, " ");
        }
        fprintf(stdout, "%02hhx", *(char*)(rawDataPtr + currByte));
    }
    fprintf(stdout, "\n");
}

