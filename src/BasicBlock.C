#include <BasicBlock.h>
#include <ElfFileInst.h>
#include <Instruction.h>
#include <Function.h>

#define MAX_SIZE_LINEAR_SEARCH 4096

uint64_t BasicBlock::getTargetAddress() {
    ASSERT(instructions.back());
    return instructions.back()->getNextAddress();
}

uint64_t BasicBlock::getAddress() { 
    ASSERT(instructions[0]); 
    return instructions[0]->getAddress(); 
}

void BasicBlock::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->dump(binaryOutputFile,offset+currByte);
        currByte += instructions[i]->getLength();
    }
    ASSERT(currByte == getBlockSize());
}

uint32_t BasicBlock::addInstruction(Instruction* inst){
    inst->setIndex(instructions.size());
    instructions.append(inst);
    return instructions.size();
}

Instruction* BasicBlock::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->getAddress() == addr){
            return instructions[i];
        }
    }

    if (instructions.size() < MAX_SIZE_LINEAR_SEARCH){
        for (uint32_t i = 0; i < instructions.size(); i++){
            if (instructions[i]->getAddress() == addr){
                return instructions[i];
            }
        }
    } else {
        PRINT_ERROR("The author should implement a non-linear instruction search");
        __SHOULD_NOT_ARRIVE;
        return NULL;
    }
    return NULL;
}

uint32_t BasicBlock::getBlockSize(){
    uint32_t size = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
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

void BasicBlock::giveSourceBlocks(BitSet<BasicBlock*>* srcBlocks){
    ASSERT(!sourceBlocks && "sourceBlocks should not be initialized");
    sourceBlocks = new BitSet<BasicBlock*>(*srcBlocks);
}

BitSet<BasicBlock*>* BasicBlock::getTargetBlocks(){
    return targetBlocks;
}

void BasicBlock::giveTargetBlocks(BitSet<BasicBlock*>* tgtBlocks){
    ASSERT(!targetBlocks && "targetBlocks should not be initialized");
    targetBlocks = new BitSet<BasicBlock*>(*tgtBlocks);
}

BitSet<BasicBlock*>* BasicBlock::getDominatorBlocks(){
    return dominatorBlocks;
}

void BasicBlock::giveDominatorBlocks(BitSet<BasicBlock*>* domBlocks){
    ASSERT(!dominatorBlocks && "dominatorBlocks should not be initialized");
    dominatorBlocks = new BitSet<BasicBlock*>(*domBlocks);
}

BitSet<BasicBlock*>* BasicBlock::getSourceBlocks(){
    return sourceBlocks;
}

BasicBlock::BasicBlock(uint32_t idx, Function* func){
    type = ElfClassTypes_BasicBlock;
    index = idx;
    function = func;

    sourceBlocks = NULL;
    targetBlocks = NULL;
    dominatorBlocks = NULL;

    flags = 0;
}

BasicBlock::~BasicBlock(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        delete instructions[i];
    }
    if (sourceBlocks){
        delete sourceBlocks;
    }
    if (targetBlocks){
        delete targetBlocks;
    }
    if (dominatorBlocks){
        delete dominatorBlocks;
    }
}

void BasicBlock::printInstructions(){
    PRINT_INFOR("Instructions for Basic Block %d", index);
    PRINT_INFOR("================");
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->print();
    }
}

void BasicBlock::print(){
    PRINT_INFOR("Basic Block %d at address range [0x%llx,0x%llx)", index, getAddress(), getAddress()+getBlockSize());
    if (sourceBlocks){
    PRINT_INFOR("\tSource Blocks:");
        BasicBlock** srcs = (*sourceBlocks).duplicateMembers();
        for (uint32_t i = 0; i < (*sourceBlocks).size(); i++){
            PRINT_INFOR("\t\tsource block(%d) with index %d at address %llx", i, srcs[i]->getIndex(), srcs[i]->getAddress());
        }
        delete[] srcs;
    }
    if (targetBlocks){
        PRINT_INFOR("\tTarget Blocks:");
        BasicBlock** tgts = (*targetBlocks).duplicateMembers();
        for (uint32_t i = 0; i < (*targetBlocks).size(); i++){
            PRINT_INFOR("\t\ttarget block(%d) with index %d at address %llx", i, tgts[i]->getIndex(), tgts[i]->getAddress());
        }
        delete[] tgts;
    }
}

bool BasicBlock::verify(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->getIndex() != i){
            PRINT_ERROR("Instruction index %d does not match expected index %d", instructions[i]->getIndex(), i);
            return false;
        }
    }

    if (sourceBlocks){
        BasicBlock** srcs = (*sourceBlocks).duplicateMembers();
        for (uint32_t i = 0; i < (*sourceBlocks).size(); i++){
            if (srcs[i]->isFunctionPadding()){
                PRINT_ERROR("Function padding blocks should not connect to other blocks");
                return false;
            }
            if (srcs[i]->getFunction() != getFunction()){
                PRINT_ERROR("Only blocks from the same function can connect to each other");
                return false;
            }
        }
        delete[] srcs;
    }

    if (targetBlocks){
        BasicBlock** tgts = (*targetBlocks).duplicateMembers();
        for (uint32_t i = 0; i < (*targetBlocks).size(); i++){
            if (tgts[i]->isFunctionPadding()){
                PRINT_ERROR("Function padding blocks should not connect to other blocks");
                return false;
            }
            if (tgts[i]->getFunction() != getFunction()){
                PRINT_ERROR("Only blocks from the same function can connect to each other");
                return false;
            }
        }
        delete[] tgts;
    }
}

uint64_t BasicBlock::findInstrumentationPoint(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        uint32_t j = i;
        uint32_t instBytes = 0;
        while (j < instructions.size() && instructions[j]->isRelocatable()){
            instBytes += instructions[j]->getLength();
            j++;
        }
        if (instBytes >= SIZE_NEEDED_AT_INST_POINT){
            return instructions[i]->getAddress();
        }
    }
    return 0;
}

Vector<Instruction*>* BasicBlock::swapInstructions(uint64_t addr, Vector<Instruction*>* replacements){
    Instruction* tgtInstruction = getInstructionAtAddress(addr);
    ASSERT(tgtInstruction && "This basic block should have an instruction at the given address");

    Vector<Instruction*>* replaced = new Vector<Instruction*>();
    if (!(*replacements).size()){
        return replaced;
    }

    // find out how many bytes we need to replace
    uint32_t bytesToReplace = 0;
    for (uint32_t i = 0; i < (*replacements).size(); i++){
        bytesToReplace += (*replacements)[i]->getLength();
    }

    // remove the instructions from the basic block and add them to the return array
    uint32_t replacedBytes = 0;
    uint32_t idx = tgtInstruction->getIndex();

    while (replacedBytes < bytesToReplace){
        (*replaced).append(instructions.remove(idx));
        ASSERT(instructions.size() >= idx && "You ran out of instructions in this block");
        replacedBytes += (*replaced).back()->getLength();
    }

    while (bytesToReplace < replacedBytes){
        instructions.insert(Instruction::generateNoop(),idx);
        bytesToReplace++;
    }

    (*replacements).reverse();
    for (uint32_t i = 0; i < (*replacements).size(); i++){
        instructions.insert((*replacements)[i],idx);
    }
    (*replacements).reverse();

    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->setIndex(i);
    }

    verify();

    return replaced;
}
