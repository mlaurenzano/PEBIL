#include <TextSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>


void TextSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        ASSERT(instructions[i]->charStream() && "The instructions in this text section should be initialized");
        binaryOutputFile->copyBytes(instructions[i]->charStream(),instructions[i]->getLength(),offset+currByte);
        currByte += instructions[i]->getLength();
    }
}

uint32_t TextSection::addInstruction(char* bytes, uint32_t length, uint64_t addr){
    uint32_t totalSize = 0;

    //    printBytes(0,0);

    Instruction** newInstructions = new Instruction*[numberOfInstructions+1];

    for (uint32_t i = 0; i < numberOfInstructions; i++){
        totalSize += instructions[i]->getLength();
        newInstructions[i] = instructions[i];
    }

    //    PRINT_INFOR("This section has %d instructions already", numberOfInstructions);
    //PRINT_INFOR("Trying to use %d bytes (%d new) of %d", totalSize + length, length, sizeInBytes);
    ASSERT(totalSize + length <= sizeInBytes && "Cannot add an instruction without extending the size of this text section");
 
    newInstructions[numberOfInstructions] = new Instruction();
    newInstructions[numberOfInstructions]->setLength(length);
    newInstructions[numberOfInstructions]->setAddress(addr);
    newInstructions[numberOfInstructions]->setBytes(bytes);
    elfFile->getDisassembler()->print_insn((uint64_t)(newInstructions[numberOfInstructions]->charStream()), newInstructions[numberOfInstructions]);

    delete[] instructions;
    instructions = newInstructions;

    numberOfInstructions++;
}


Instruction* TextSection::getInstruction(uint32_t idx){
    ASSERT(idx <= 0 && idx < numberOfInstructions && "Array index out of bounds");

    return instructions[idx];
}

TextSection::~TextSection(){
    if (instructions){
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            if (instructions[i]){
                delete instructions[i];
            }
        }
        delete[] instructions;
    }
}

uint32_t TextSection::read(BinaryInputFile* binaryInputFile){
    PRINT_INFOR("GARBLE reading text section");


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
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        instructionLength = disassembler->print_insn(instructionAddress, dummyInstruction);
    }
    instructions = new Instruction*[numberOfInstructions];
    currByte = numberOfInstructions = 0;

    //disassembler->setPrintFunction((fprintf_ftype)sprintf,&disasmBuffer);
    disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
    //disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

    for (currByte = 0; currByte < sHdr->GET(sh_size); currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setAddress(sHdr->GET(sh_addr) + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);

        instructionLength = disassembler->print_insn(instructionAddress, instructions[numberOfInstructions]);        
        if (!instructionLength){
            instructionLength = 1;
        }
        instructions[numberOfInstructions]->setLength(instructionLength);

        instructions[numberOfInstructions]->setNextAddress();
    }
    PRINT_INFOR("Found %d instructions (%d bytes) in section %d", numberOfInstructions, currByte, sectionIndex);

    delete dummyInstruction;

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
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        fprintf(stdout, "(0x%lx) 0x%lx:\t", (uint64_t)(charStream() + currByte), sHdr->GET(sh_addr) + currByte);

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
