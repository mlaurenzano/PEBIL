#include <BasicBlock.h>

#include <ElfFileInst.h>
#include <Function.h>
#include <FlowGraph.h>
#include <Instruction.h>

#define MAX_SIZE_LINEAR_SEARCH 4096

uint32_t BasicBlock::addSourceBlock(BasicBlock* bb){
    sourceBlocks.append(bb);
    return sourceBlocks.size();
}

uint32_t BasicBlock::addTargetBlock(BasicBlock* bb){
    targetBlocks.append(bb);
    return targetBlocks.size();
}

bool BasicBlock::findExitInstruction(){
    return instructions.back()->isReturn();
}


bool BasicBlock::passesControlToNext(){
    Instruction* last = instructions.back();
    return (!last->isReturn() && !last->isBranch());
}

bool BasicBlock::containsOnlyControl(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (!instructions[i]->isControl() && !instructions[i]->isNoop()){
            return false;
        }
    }
    return true;
}

bool BasicBlock::isDominatedBy(BasicBlock* bb){
    BasicBlock* dom = immDominatedBy;
    while (dom){
        if (dom == bb){
            return true;
        }
        dom = dom->getImmDominator();
    }
    return false;
}

void BasicBlock::findMemoryFloatOps(){
    __FUNCTION_NOT_IMPLEMENTED;
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
    return baseAddress;
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
    if (!instructions.size()){
        baseAddress = inst->getAddress();
    }
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

BasicBlock::BasicBlock(uint32_t idx, FlowGraph* cfg){
    type = ElfClassTypes_BasicBlock;
    index = idx;
    flowGraph = cfg;

    flags = 0;
    immDominatedBy = NULL;

    numberOfMemoryOps = 0;
    numberOfFloatOps = 0;

    ASSERT(flowGraph);

    Function* func = flowGraph->getFunction();
    ASSERT(func);
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

void BasicBlock::printSourceBlocks(){
    PRINT_INFOR("SOURCE BLOCKS FOR BASICBLOCK(%d):", index);
    PRINT_INFOR("===========================================");
    for (uint32_t i = 0; i < sourceBlocks.size(); i++){
        sourceBlocks[i]->print();
    }
    PRINT_INFOR("===========================================");
}

void BasicBlock::print(){

    char pad = '-';
    char ent = '-';
    char ext = '-';
    char ctr = '-';
    char rch = '-';

    if (isPadding()){
        pad = 'P';
    }
    if (isEntry()){
        ent = 'E';
    }
    if (isExit()){
        ext = 'X';
    }
    if (isOnlyCtrl()){
        ctr = 'C';
    }
    if (isReachable()){
        rch = 'R';
    }

    PRINT_INFOR("BASICBLOCK(%d) range=[0x%llx,0x%llx), %d instructions, flags %c%c%c%c%c%c", index, getAddress(), getAddress()+getBlockSize(), getNumberOfInstructions(), pad, ent, ext, ctr, rch);
    if (immDominatedBy){
        PRINT_INFOR("\tdom: %d", immDominatedBy->getIndex());
    }
    if (sourceBlocks.size()){
        PRINT_INFO();
        PRINT_OUT("\tsources:");
        for (uint32_t i = 0; i < sourceBlocks.size(); i++){
            PRINT_OUT("%d ", sourceBlocks[i]->getIndex());
        }
        PRINT_OUT("\n");
    }
    if (targetBlocks.size()){
        PRINT_INFO();
        PRINT_OUT("\ttargets:");
        for (uint32_t i = 0; i < targetBlocks.size(); i++){
            PRINT_OUT("%d ", targetBlocks[i]->getIndex());
        }
        PRINT_OUT("\n");
    }
    //    printInstructions();
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

uint64_t BasicBlock::findInstrumentationPoint(uint32_t size, InstLocations loc){

    for (uint32_t i = 0; i < instructions.size(); i++){
        uint32_t j = i;
        uint32_t instBytes = 0;
        while (j < instructions.size() && instructions[j]->isRelocatable()){
            instBytes += instructions[j]->getLength();
            j++;
        }
        if (instBytes >= size){
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

    replacedBytes = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->setIndex(i);
        instructions[i]->setAddress(replacedBytes+baseAddress);
        replacedBytes += instructions[i]->getLength();
    }

    verify();

    return replaced;
}
