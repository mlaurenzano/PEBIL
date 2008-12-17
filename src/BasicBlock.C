#include <BasicBlock.h>
#include <ElfFileInst.h>
#include <Instruction.h>
#include <Function.h>
#include <FlowGraph.h>

#define MAX_SIZE_LINEAR_SEARCH 4096

void BasicBlock::findMemoryFloatOps(){
}

void BasicBlock::setIndex(uint32_t idx){
    index = idx;
    hashCode = HashCode(flowGraph->getTextSection()->getSectionIndex(),
                        flowGraph->getFunction()->getIndex(),
                        index);

    verify();
}

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

BasicBlock::BasicBlock(uint32_t idx, Function* func){
    type = ElfClassTypes_BasicBlock;
    index = idx;
    function = func;

    flags = 0;

    hashCode = HashCode(func->getTextSection()->getSectionIndex(),func->getIndex(),index);
    PRINT_DEBUG_HASHCODE("Block %d in function %d in section %d has HashCode 0x%012llx", index, func->getIndex(), func->getTextSection()->getSectionIndex(), hashCode.getValue());

    verify();
}

BasicBlock::~BasicBlock(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        delete instructions[i];
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
}

bool BasicBlock::verify(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->getIndex() != i){
            PRINT_ERROR("Instruction index %d does not match expected index %d", instructions[i]->getIndex(), i);
            return false;
        }
    }
    if (!hashCode.isBlock()){
        PRINT_ERROR("BasicBlock %d HashCode is malformed", index);
        return false;
    }
    return true;
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
