#include <TextSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>
#include <SymbolTable.h>


Instruction* TextSection::getInstructionAtAddress(uint64_t addr){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());
    if (!sectionHeader->inRange(addr)){
        PRINT_WARN("Instruction lookup failing because section %d does not contain address %llx", getSectionIndex(), addr);
        return NULL;
    }

    // linear search is bad -- too slow
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (addr == instructions[i]->getAddress()){
            return instructions[i];
        }
    }

    return NULL;

}

uint32_t TextSection::findFunctions(){

    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    numberOfFunctions = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->getSymbolType() == STT_FUNC && symbol->GET(st_shndx) == getSectionIndex()){
                    numberOfFunctions++;
                }
            }
        }
    }

    sortedFunctions = new Function*[numberOfFunctions];
    Symbol** functionSymbols = new Symbol*[numberOfFunctions];

    PRINT_INFOR("Found %d functions in section %d", numberOfFunctions, getSectionIndex());

    numberOfFunctions = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->getSymbolType() == STT_FUNC && symbol->GET(st_shndx) == getSectionIndex()){
                    PRINT_INFOR("Assigning function %d", numberOfFunctions);
                    functionSymbols[numberOfFunctions++] = symbol;
                }
            }
        }
    }

    qsort(functionSymbols,numberOfFunctions,sizeof(Symbol*),compareSymbolValue);

    if (numberOfFunctions){
        for (uint32_t i = 0; i < numberOfFunctions-1; i++){
            sortedFunctions[i] = new Function(this, functionSymbols[i], functionSymbols[i+1]->GET(st_value), i);
            functionSymbols[i]->print(NULL);
        }
        // the last function does till the end of the section
        sortedFunctions[numberOfFunctions-1] = new Function(this, functionSymbols[numberOfFunctions-1], 
                                                            sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size), numberOfFunctions-1);
    }
    delete[] functionSymbols;


    sectionHeader->print();
    PRINT_INFOR("Found %d functions for section %d", numberOfFunctions, getSectionIndex());

    verify();

    return numberOfFunctions;
}

bool TextSection::verify(){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    if (!numberOfFunctions){
        return true;
    }

    for (uint32_t i = 0; i < numberOfFunctions; i++){

        uint64_t entrAddr = sortedFunctions[i]->getFunctionAddress();
        uint64_t exitAddr = entrAddr + sortedFunctions[i]->getFunctionSize();

        // make sure each function entry resides within the bounds of this section
        if (!sectionHeader->inRange(entrAddr)){
            sectionHeader->print();
            PRINT_ERROR("The function entry address 0x%016llx is not in the range of section %d", entrAddr, sectionHeader->getIndex());
            return false;
        }

        // make sure each function exit resides within the bounds of this section
        if (!sectionHeader->inRange(exitAddr) && exitAddr != sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
            sortedFunctions[i]->print();
            sectionHeader->print();
            PRINT_INFOR("Section range [0x%016llx,0x%016llx]", sectionHeader->GET(sh_addr), sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size));
            PRINT_ERROR("The function exit address 0x%016llx is not in the range of section %d", exitAddr, sectionHeader->getIndex());
            return false;
        }
    }

    for (uint32_t i = 0; i < numberOfFunctions - 1; i++){

        // make sure sortedFunctions is actually sorted
        if (sortedFunctions[i]->getFunctionAddress() > sortedFunctions[i+1]->getFunctionAddress()){
            sortedFunctions[i]->print();
            sortedFunctions[i+1]->print();
            PRINT_ERROR("Function addresses 0x%016llx 0x%016llx are not sorted", sortedFunctions[i]->getFunctionAddress(), sortedFunctions[i+1]->getFunctionAddress());
            return false;
        }
    }

    return true;
}


void TextSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        ASSERT(instructions[i]->charStream() && "The instructions in this text section should be initialized");
        binaryOutputFile->copyBytes(instructions[i]->charStream(),instructions[i]->getLength(),offset+currByte);
        currByte += instructions[i]->getLength();
    }
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

    if (sortedFunctions){
        for (uint32_t i = 0; i < numberOfFunctions; i++){
            delete sortedFunctions[i];
        }
        delete[] sortedFunctions;
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
