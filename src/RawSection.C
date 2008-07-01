#include <RawSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>

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
            fprintf(stdout, "%8x: ", currByte);
        } else if (currByte && currByte % bytesPerWord == 0){
            fprintf(stdout, " ");
        }
        fprintf(stdout, "%02hhx", *(char*)(rawDataPtr + currByte));
    }
    fprintf(stdout, "\n");
}

