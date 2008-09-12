#include <Function.h>
#include <TextSection.h>
#include <Instruction.h>
#include <ElfFileInst.h>
#include <SymbolTable.h>

char* Function::getFunctionName(){
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
    ASSERT(currByte == functionSize);
}

bool Function::inRange(uint64_t addr){
    if (addr >= getFunctionAddress() &&
        addr < getFunctionAddress() + functionSize){
        return true;
    }
    return false;
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


char* Function::charStream(){
    ASSERT(rawSection);
    uint64_t functionOffset = getFunctionAddress() -
        rawSection->getElfFile()->getSectionHeader(rawSection->getSectionIndex())->GET(sh_addr);
    return (char*)(rawSection->charStream() + functionOffset);
}

Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        if (basicBlocks[i]->inRange(addr)){
            return basicBlocks[i]->getInstructionAtAddress(addr); 
        }
    }
    return NULL;
}

uint32_t Function::read(BinaryInputFile* binaryInputFile){
    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;
    uint32_t numberOfInstructions = 0;

    Instruction* dummyInstruction = new Instruction();
    for (currByte = 0; currByte < functionSize; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        instructionLength = rawSection->getDisassembler()->print_insn(instructionAddress, dummyInstruction);
    }

    delete dummyInstruction;

    ASSERT(currByte == functionSize && "Number of bytes read for function does not match function size");

    Instruction** instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;

    
    for (currByte = 0; currByte < functionSize; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setAddress(getFunctionAddress() + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);
        instructions[numberOfInstructions]->setIndex(numberOfInstructions);
        
        instructionLength = rawSection->getDisassembler()->print_insn(instructionAddress, instructions[numberOfInstructions]);
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


Function::Function(TextSection* rawsect, uint64_t addr, uint64_t exitAddr, uint32_t idx) :
    Base(ElfClassTypes_Function)
{
    rawSection = rawsect;
    functionSymbol = NULL;
    functionAddress = addr;
    index = idx;

    //    functionSize = functionSymbol->GET(st_size);
    functionSize = exitAddr - getFunctionAddress();

    numberOfBasicBlocks = 0;
    basicBlocks = NULL;

    verify();
}

Function::Function(TextSection* rawsect, Symbol* sym, uint64_t exitAddr, uint32_t idx) :
    Base(ElfClassTypes_Function)
{
    rawSection = rawsect;
    functionSymbol = sym;
    functionAddress = functionSymbol->GET(st_value);
    index = idx;

    //    functionSize = functionSymbol->GET(st_size);
    functionSize = exitAddr - getFunctionAddress();

    numberOfBasicBlocks = 0;
    basicBlocks = NULL;

    verify();
}

void Function::setFunctionSize(uint64_t size){
    functionSize = size;
}

bool Function::verify(){
    if (functionSymbol){
        if (functionSymbol->getSymbolType() != STT_FUNC){
            PRINT_ERROR("Function symbol should have type STT_FUNC");
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
    PRINT_INFOR("Function size is %lld bytes", functionSize);
    functionSymbol->print();
}
