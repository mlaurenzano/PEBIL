#include <BasicBlock.h>
#include <ElfFileInst.h>

#define MAX_SIZE_LINEAR_SEARCH 4096

void BasicBlock::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->dump(binaryOutputFile,offset+currByte);
        currByte += instructions[i]->getLength();
    }
    ASSERT(currByte == getBlockSize());
}

uint32_t BasicBlock::setInstructions(uint32_t num, Instruction** insts){
    ASSERT(!numberOfInstructions && !instructions);
    numberOfInstructions = num;
    
    instructions = new Instruction*[numberOfInstructions];
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i] = insts[i];
    }

    verify();
    //    PRINT_INFOR("Basic Block %d in function %s has %d instructions", index, function->getFunctionName(), numberOfInstructions);

    return numberOfInstructions;
}


Instruction* BasicBlock::getInstructionAtAddress(uint64_t addr){
    if (numberOfInstructions < MAX_SIZE_LINEAR_SEARCH){
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            if (instructions[i]->getAddress() == addr){
                return instructions[i];
            }
        }
    } else {
        PRINT_ERROR("You should implement a way to do non-linear instruction search");
        __SHOULD_NOT_ARRIVE;
        return NULL;
    }
    return NULL;
}

uint32_t BasicBlock::getBlockSize(){
    uint32_t size = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        size += instructions[i]->getLength();
    }
    return size;
}

bool BasicBlock::inRange(uint64_t addr){
    if (addr >= getAddress() &&
        addr < getAddress() + getBlockSize()){
        return true;
    }
    return false;
}

BasicBlock::BasicBlock(uint32_t idx, Function* func){
    index = idx;
    function = func;

    instructions = NULL;
    numberOfInstructions = 0;

    sourceBlocks = NULL;
    numberOfSourceBlocks = 0;
    targetBlocks = NULL;
    numberOfTargetBlocks = 0;

    flags = 0;
}

BasicBlock::~BasicBlock(){
    if (instructions){
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            delete instructions[i];
        }
        delete[] instructions;
    }
    if (sourceBlocks){
        delete[] sourceBlocks;
    }
    if (targetBlocks){
        delete[] targetBlocks;
    }
}

void BasicBlock::printInstructions(){
    PRINT_INFOR("Instructions for Basic Block %d", index);
    PRINT_INFOR("================");
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->print();
    }
}

void BasicBlock::print(){
    PRINT_INFOR("Basic Block %d at address 0x%llx", index, getAddress());
    PRINT_INFOR("\tSource Blocks:");
    for (uint32_t i = 0; i < numberOfSourceBlocks; i++){
        PRINT_INFOR("\t\tsource block(%d) with index %d at address %llx", i, sourceBlocks[i]->getIndex(), sourceBlocks[i]->getAddress());
    }
    PRINT_INFOR("\tTarget Blocks:");
    for (uint32_t i = 0; i < numberOfTargetBlocks; i++){
        PRINT_INFOR("\t\ttarget block(%d) with index %d at address %llx", i, targetBlocks[i]->getIndex(), targetBlocks[i]->getAddress());
    }
}

bool BasicBlock::verify(){
    for (uint32_t i = 0; i < numberOfSourceBlocks; i++){
        if (sourceBlocks[i]->isFunctionPadding()){
            PRINT_ERROR("Function padding blocks should not connect to other blocks");
            return false;
        }
        if (sourceBlocks[i]->getFunction() != getFunction()){
            PRINT_ERROR("Only blocks from the same function can connect to each other");
            return false;
        }
    }
    for (uint32_t i = 0; i < numberOfTargetBlocks; i++){
        if (targetBlocks[i]->isFunctionPadding()){
            PRINT_ERROR("Function padding blocks should not connect to other blocks");
            return false;
        }
        if (targetBlocks[i]->getFunction() != getFunction()){
            PRINT_ERROR("Only blocks from the same function can connect to each other");
            return false;
        }
    }
}

uint64_t BasicBlock::findInstrumentationPoint(){
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        uint32_t j = i;
        uint32_t instBytes = 0;
        while (j < numberOfInstructions && instructions[j]->isRelocatable()){
            instBytes += instructions[j]->getLength();
            j++;
        }
        if (instBytes >= SIZE_NEEDED_AT_INST_POINT){
            return instructions[i]->getAddress();
        }
    }
    return 0;
}

uint32_t BasicBlock::replaceInstructions(uint64_t addr, Instruction** replacements, 
                                         uint32_t numberOfReplacements, Instruction*** replacedInstructions){

    ASSERT(!*(replacedInstructions) && "This array should be empty since it will be filled by this function");

    uint32_t replacementBytes = 0;
    uint32_t bytesToReplace = 0;

    for (uint32_t i = 0; i < numberOfReplacements; i++){
        replacementBytes += replacements[i]->getLength();
    }

    uint32_t instructionsToReplace = 0;
    Instruction* inst = getInstructionAtAddress(addr);

    ASSERT(inst && "Instruction should exist at the requested address");
    uint64_t a;
    for (a = addr; a < addr+replacementBytes && inst; ){
        a += inst->getLength();
        bytesToReplace += inst->getLength();
        inst = getInstructionAtAddress(a);
        instructionsToReplace++;
    }
    ASSERT(a >= addr+replacementBytes && "Should be enough space to insert the requested instructions");
    ASSERT(instructionsToReplace && "At least one instruction must be replaced");

    Instruction** toReplace = new Instruction*[instructionsToReplace];
    instructionsToReplace = 0;
    inst = getInstructionAtAddress(addr);
    ASSERT(inst && "Instruction should exist at the requested address");
    for (a = addr; a < addr+replacementBytes && inst; ){
        a += inst->getLength();
        toReplace[instructionsToReplace] = inst;
        inst = getInstructionAtAddress(a);
        instructionsToReplace++;
    }
    ASSERT(a >= addr+replacementBytes && "There should be instructions in the range requested by the insert");

    ASSERT(replacementBytes <= bytesToReplace && "Should be enough room to insert the instructions");
    uint32_t extraNoops = bytesToReplace - replacementBytes;

    uint32_t newNumberOfInstructions = numberOfInstructions - instructionsToReplace + numberOfReplacements + extraNoops;
    Instruction** newinstructions = new Instruction*[newNumberOfInstructions];
    uint32_t currInstruction = 0;

    // copy instructions that occur before this replacement
    while (currInstruction < toReplace[0]->getIndex()){
        newinstructions[currInstruction] = instructions[currInstruction];
        currInstruction++;
    }

    // copy the replacement instructions
    while (currInstruction < toReplace[0]->getIndex() + numberOfReplacements){
        newinstructions[currInstruction] = replacements[currInstruction-toReplace[0]->getIndex()];
        currInstruction++;
    }

    // copy noops that have to be used as padding
    while (currInstruction < toReplace[0]->getIndex() + numberOfReplacements + extraNoops){
        newinstructions[currInstruction] = Instruction::generateNoop();
        currInstruction++;
    }

    // copy instructions that occur after the replacement
    while (currInstruction < newNumberOfInstructions){
        newinstructions[currInstruction] = instructions[currInstruction - newNumberOfInstructions + numberOfInstructions];
        currInstruction++;
    }

    delete[] instructions;
    instructions = newinstructions;
    numberOfInstructions = newNumberOfInstructions;

    addr = getAddress();
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->setIndex(i);
        instructions[i]->setAddress(addr);
        addr += instructions[i]->getLength();
    }
    *(replacedInstructions) = toReplace;

    verify();

    return instructionsToReplace;


}
