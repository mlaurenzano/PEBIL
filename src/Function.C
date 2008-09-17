#include <Function.h>
#include <ElfFile.h>
#include <SectionHeader.h>
#include <Disassembler.h>
#include <TextSection.h>
#include <Instruction.h>
#include <ElfFileInst.h>
#include <SymbolTable.h>
#include <BasicBlock.h>
#include <BinaryFile.h>

char* Function::getName(){
    if (functionSymbol){
        return functionSymbol->getSymbolName();
    }
    return symbol_without_name;
}

void Function::printInstructions(){
    __FUNCTION_NOT_IMPLEMENTED;
}

void Function::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        basicBlocks[i]->dump(binaryOutputFile,offset+currByte);
        currByte += basicBlocks[i]->getBlockSize();
    }
    ASSERT(currByte == sizeInBytes);
}

uint32_t Function::findBasicBlocks(uint32_t numberOfInstructions, Instruction** instructions){
    ASSERT(!numberOfBasicBlocks && !basicBlocks && "Should not try to find the basic blocks in a function more than once");

    numberOfBasicBlocks = 1;
    basicBlocks = new BasicBlock*[numberOfBasicBlocks];

    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        basicBlocks[i] = new BasicBlock(i,this);
        basicBlocks[i]->setInstructions(numberOfInstructions,instructions);
    }

    verify();

    return numberOfBasicBlocks;
}

Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        if (basicBlocks[i]->inRange(addr)){
            return basicBlocks[i]->getInstructionAtAddress(addr); 
        }
    }
    return NULL;
}

uint32_t Function::digest(){
    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;
    uint32_t numberOfInstructions = 0;

    Instruction* dummyInstruction = new Instruction();
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, dummyInstruction);
    }

    delete dummyInstruction;

    PRINT_INFOR("Function %s: read %d bytes from function, %d bytes in functions", getName(), currByte, sizeInBytes);
    if (functionSymbol)
        functionSymbol->print();
    ASSERT(currByte == sizeInBytes && "Number of bytes read for function does not match function size");

    Instruction** instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;

    
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setAddress(getAddress() + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);
        instructions[numberOfInstructions]->setIndex(numberOfInstructions);
        
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, instructions[numberOfInstructions]);
        if (!instructionLength){
            instructionLength = 1;
        }
        instructions[numberOfInstructions]->setLength(instructionLength);
        
        instructions[numberOfInstructions]->setNextAddress();
    }

    findBasicBlocks(numberOfInstructions, instructions);

    delete[] instructions;
    return currByte;
}

uint64_t Function::findInstrumentationPoint(){
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        uint64_t p = basicBlocks[i]->findInstrumentationPoint();
        if (p){
            return p;
        }
    }
    return 0;
}

Function::~Function(){
    if (basicBlocks){
        for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
            delete basicBlocks[i];
        }
        delete[] basicBlocks;
    }
}


Function::Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz) :
    TextObject(ElfClassTypes_Function,text,idx,sym->GET(st_value),sz)
{
    textSection = text;
    functionSymbol = sym;

    numberOfBasicBlocks = 0;
    basicBlocks = NULL;

    verify();
}


bool Function::verify(){
    if (functionSymbol){
        if (!functionSymbol->isFunctionSymbol(textSection)){
            PRINT_ERROR("The symbol given for this function does not appear to be a function symbol");
            return false;
        }
    }
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        if (!basicBlocks[i]->verify()){
            return false;
        }
    }
    return true;
}

void Function::print(){
    PRINT_INFOR("Function size is %lld bytes", sizeInBytes);
    functionSymbol->print();
}
