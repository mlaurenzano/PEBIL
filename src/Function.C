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
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        basicBlocks[i]->dump(binaryOutputFile,offset+currByte);
        currByte += basicBlocks[i]->getBlockSize();
    }
    ASSERT(currByte == sizeInBytes);
}

// we will assume that functions can only be entered at the beginning
uint32_t Function::findBasicBlocks(uint32_t numberOfInstructions, Instruction** instructions){
    ASSERT(!basicBlocks.size() && "Basic blocks vector should be empty");

    bool* isLeader = new bool[numberOfInstructions];
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        isLeader[i] = false;
    }

    uint32_t numberOfBasicBlocks = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (i == 0){
            isLeader[i] = true;
            numberOfBasicBlocks++;
        }
        if (instructions[i]->getAddress() + instructions[i]->getLength() != instructions[i]->getNextAddress()){
            if (inRange(instructions[i]->getNextAddress())){
                if (instructions[i]->isBranchInstruction()){
                    if (i+1 < numberOfInstructions){
                        if (!isLeader[i+1]){
                            isLeader[i+1] = true;
                            numberOfBasicBlocks++;
                        }
                    }
                }
                for (uint32_t j = 0; j < numberOfInstructions; j++){
                    if (instructions[j]->getAddress() == instructions[i]->getNextAddress()){
                        if (!isLeader[j]){
                            isLeader[j] = true;
                            numberOfBasicBlocks++;
                        }
                        if (i+1 < numberOfInstructions){
                            if (!isLeader[i+1]){
                                isLeader[i+1] = true;
                                numberOfBasicBlocks++;
                            }
                        }
                    }
                }
            }
        }
    }

#ifdef DEBUG_BASICBLOCK
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->print();
        PRINT_DEBUG_BASICBLOCK("Is leader? %d", isLeader[i]);
    }
    PRINT_DEBUG_BASICBLOCK("Found %d instructions in %d basic blocks in function %s", numberOfInstructions, numberOfBasicBlocks, getName());
#endif

    BasicBlock* currentBlock = NULL;
    numberOfBasicBlocks = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (isLeader[i]){
            if (currentBlock){
                basicBlocks.append(currentBlock);
                numberOfBasicBlocks++;
            }
            currentBlock = new BasicBlock(numberOfBasicBlocks,this);
        }
        currentBlock->addInstruction(instructions[i]);
    }
    basicBlocks.append(currentBlock);
    numberOfBasicBlocks++;

#ifdef DEBUG_BASICBLOCK
    PRINT_DEBUG_BASICBLOCK("****** Printing Blocks for function %s", getName());

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        basicBlocks[i]->print();
        basicBlocks[i]->printInstructions();
    }
#endif

    delete[] isLeader;
    ASSERT(numberOfBasicBlocks == basicBlocks.size());

    verify();

    return basicBlocks.size();
}

Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
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

    /*
    PRINT_INFOR("Function %s: read %d bytes from function, %d bytes in functions", getName(), currByte, sizeInBytes);
    if (functionSymbol)
        functionSymbol->print();
    */

    Instruction** instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;
    
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setAddress(getAddress() + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);
        //        instructions[numberOfInstructions]->setIndex(numberOfInstructions);
        
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, instructions[numberOfInstructions]);
        if (!instructionLength){
            instructionLength = 1;
        }
        instructions[numberOfInstructions]->setLength(instructionLength);
        instructions[numberOfInstructions]->setNextAddress();
    }
    
    // in case the disassembler found an instruction that exceeds the function boundary, we will
    // reduce the size of the last instruction accordingly so that the extra bytes will not be
    // used
    if (currByte > sizeInBytes){
        uint32_t extraBytes = currByte-sizeInBytes;
        instructions[numberOfInstructions-1]->setLength(instructions[numberOfInstructions-1]->getLength()-extraBytes);
        currByte -= extraBytes;
        PRINT_WARN("Disassembler found instructions that exceed the function boundary in %s by %d bytes", getName(), extraBytes);
    }

    ASSERT(currByte == sizeInBytes && "Number of bytes read for function does not match function size");
    findBasicBlocks(numberOfInstructions, instructions);

    delete[] instructions;
    return currByte;
}

uint64_t Function::findInstrumentationPoint(){
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        uint64_t p = basicBlocks[i]->findInstrumentationPoint();
        if (p){
            return p;
        }
    }
    return 0;
}

Function::~Function(){
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        delete basicBlocks[i];
    }
}


Function::Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz) :
    TextObject(ElfClassTypes_Function,text,idx,sym->GET(st_value),sz)
{
    textSection = text;
    functionSymbol = sym;

    verify();
}


bool Function::verify(){
    if (functionSymbol){
        if (!functionSymbol->isFunctionSymbol(textSection)){
            PRINT_ERROR("The symbol given for this function does not appear to be a function symbol");
            return false;
        }
    }
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
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
