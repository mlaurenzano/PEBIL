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

Instruction* TextSection::getInstruction(uint32_t idx){
    ASSERT(idx <= 0 && idx < numberOfInstructions && "Array index out of bounds");

    return instructions[idx];
}

uint32_t TextSection::disassemble(){
    char disasmBuffer[MAX_DISASM_STR_LENGTH];

    ASSERT(elfFile && "Text section should be linked to its corresponding ElfFile object");

    Disassembler* disassembler = elfFile->getDisassembler();
    ASSERT(disassembler && "Disassembler should be initialized before calling disassemble");
    disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);

    SectionHeader* sHdr = elfFile->getSectionHeader(sectionIndex);
    ASSERT(sHdr && "Invalid sectionIndex set on text section");

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;
    Instruction* dummyInstruction = new Instruction();

    PRINT_INFOR("Disassembling Section %s(%d)", sHdr->getSectionNamePtr(), sectionIndex);

    for (currByte = 0; currByte < sHdr->GET(sh_size); currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint32_t)charStream() + currByte);
        instructionLength = disassembler->print_insn(instructionAddress, dummyInstruction);
    }
    instructions = new Instruction*[numberOfInstructions];
    currByte = numberOfInstructions = 0;

    //disassembler->setPrintFunction((fprintf_ftype)sprintf,&disasmBuffer);
    //disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
    disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

    for (currByte = 0; currByte < sHdr->GET(sh_size); currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint32_t)charStream() + currByte);

        fprintf(stdout, " 0x%lx:\t", sHdr->GET(sh_addr) + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setAddress(sHdr->GET(sh_addr) + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);

        instructionLength = disassembler->print_insn(instructionAddress, instructions[numberOfInstructions]);        
        fprintf(stdout, "\n");
        instructions[numberOfInstructions]->setNextAddress();
        //        instructions[numberOfInstructions]->print();
    }
    PRINT_INFOR("Found %d instructions (%d bytes) in section %d", numberOfInstructions, currByte, sectionIndex);

}

uint32_t TextSection::printDisassembledCode(){
    ASSERT(elfFile && "Text section should be linked to its corresponding ElfFile object");

    Disassembler* disassembler = elfFile->getDisassembler();
    ASSERT(disassembler && "Disassembler should be initialized before calling disassemble");

    disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

    SectionHeader* sHdr = elfFile->getSectionHeader(sectionIndex);
    ASSERT(sHdr && "Invalid sectionIndex set on text section");

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint32_t instructionCount = 0;
    uint64_t instructionAddress;
    Instruction* dummyInstruction = new Instruction();

    PRINT_INFOR("Disassembly output of Section %s(%d)", sHdr->getSectionNamePtr(), sectionIndex);

    for (currByte = 0; currByte < sHdr->GET(sh_size); currByte += instructionLength, instructionCount++){
        instructionAddress = (uint64_t)((uint32_t)charStream() + currByte);
        fprintf(stdout, "(0x%lx) 0x%lx:\t", (uint32_t)(charStream() + currByte), sHdr->GET(sh_addr) + currByte);

        instructionLength = disassembler->print_insn(instructionAddress, dummyInstruction);

        fprintf(stdout, "\t(bytes -- ");
        uint8_t* bytePtr;
        for (uint32_t j = 0; j < instructionLength; j++){
            bytePtr = (uint8_t*)charStream() + currByte + j;
            fprintf(stdout, "%2.2lx ", *bytePtr);
        }

        fprintf(stdout, ")\n");
    }
    PRINT_INFOR("Found %d instructions (%d bytes) in section %d", instructionCount, currByte, sectionIndex);
}
