#include <BasicBlock.h>

#include <ElfFileInst.h>
#include <Function.h>
#include <FlowGraph.h>
#include <Instruction.h>
#include <InstructionGenerator.h>

#define MAX_SIZE_LINEAR_SEARCH 4096

static const char* bytes_not_instructions = "<x86_inst_unreachable_text>";

void RawBlock::printDisassembly(bool instructionDetail){
    uint32_t bytesPerWord = 1;
    uint32_t bytesPerLine = 8;
    
    uint32_t currByte = 0;
    
    for (currByte = 0; currByte < sizeInBytes; currByte++){
        if (currByte % bytesPerLine == 0){
            if (currByte){
                fprintf(stdout, "%s\n", bytes_not_instructions);
            }
            fprintf(stdout, "%llx: ", getBaseAddress()+currByte);
        }
        fprintf(stdout, "%02hhx ", rawBytes[currByte]);
    }
    for (uint32_t i = 0; i < 8-(sizeInBytes%8); i++){
        fprintf(stdout, "   ");        
    }

    fprintf(stdout, "%s\n", bytes_not_instructions);
}

void CodeBlock::printDisassembly(bool instructionDetail){

    PRINT_DEBUG_ANCHOR("Block begins");

    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->binutilsPrint(stdout);
        if (instructionDetail){
            instructions[i]->print();
        }
    }
}

Block::Block(ElfClassTypes typ, uint32_t idx, FlowGraph* cfg)
    : Base(typ)
{
    index = idx;
    flowGraph = cfg;
}

RawBlock::RawBlock(uint32_t idx, FlowGraph* cfg, char* byt, uint32_t sz, uint64_t addr)
    : Block(ElfClassTypes_RawBlock,idx,cfg)
{
    sizeInBytes = sz;
     
    rawBytes = NULL;
    rawBytes = new char[sizeInBytes];
    memcpy(rawBytes,byt,sizeInBytes);
    ASSERT(rawBytes);

    baseAddress = addr;
}

RawBlock::~RawBlock(){
    if (rawBytes){
        delete[] rawBytes;
    }
}

void RawBlock::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(rawBytes,sizeInBytes,offset);
}

void RawBlock::print(){
    PRINT_INFO();
    PRINT_OUT("RawBlock: %#llx -- ", baseAddress);
    for (uint32_t i = 0; i < sizeInBytes; i++){
        PRINT_OUT("%hhx ", rawBytes[i]);
    }
    PRINT_OUT("\n");
}

uint32_t BasicBlock::bloat(uint32_t minBlockSize){

    // convert all branches to use 4byte operands
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isControl() && !instructions[i]->isReturn()){
            if (instructions[i]->bytesUsedForTarget() < sizeof(uint32_t)){
                PRINT_DEBUG_FUNC_RELOC("This instruction uses %d bytes for target calculation", instructions[i]->bytesUsedForTarget());
                instructions[i]->convertTo4ByteTargetOperand();
                instructions[i]->setBaseAddress(baseAddress+currByte);
            }
        }
        currByte += instructions[i]->getSizeInBytes();
    }

    // pad with noops if necessary
    int32_t extraBytesNeeded = minBlockSize - currByte;
    while (extraBytesNeeded > 0){
        Instruction* extraNoop = InstructionGenerator::generateNoop();
        extraNoop->setBaseAddress(baseAddress+currByte);

        currByte += extraNoop->getSizeInBytes();
        extraBytesNeeded -= extraNoop->getSizeInBytes();
        extraNoop->setIndex(instructions.size());
#ifdef DEBUG_FUNC_RELOC
        //extraNoop->print();
#endif
        instructions.append(extraNoop);
    }
    return getNumberOfBytes();
}


bool BasicBlock::containsCallToRange(uint64_t lowAddr, uint64_t highAddr){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isFunctionCall()){
            uint64_t targetAddr = instructions[i]->getTargetAddress();
            if (targetAddr >= lowAddr && targetAddr < highAddr){
                return true;
            }
        }
    }
    return false;
}

uint32_t CodeBlock::getAllInstructions(Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        allinsts[i+nexti] = instructions[i];
        instructionCount++;
    }
    return instructionCount;
}


void CodeBlock::setBaseAddress(uint64_t newBaseAddr){
    baseAddress = newBaseAddr;
    uint32_t currentOffset = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->setBaseAddress(baseAddress + currentOffset);
        currentOffset += instructions[i]->getSizeInBytes();
    }
}

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


bool BasicBlock::controlFallsThrough(){
    Instruction* last = instructions.back();
    return last->controlFallsThrough();
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
    return instructions.back()->getTargetAddress();
}

void CodeBlock::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->dump(binaryOutputFile,offset+currByte);
        currByte += instructions[i]->getSizeInBytes();
    }
    ASSERT(currByte == getNumberOfBytes());
}

uint32_t CodeBlock::addInstruction(Instruction* inst){
    if (!instructions.size()){
        baseAddress = inst->getBaseAddress();
    }
    inst->setIndex(instructions.size());
    instructions.append(inst);
    return instructions.size();
}

Instruction* CodeBlock::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->getBaseAddress() == addr){
            return instructions[i];
        }
    }

    if (instructions.size() < MAX_SIZE_LINEAR_SEARCH){
        for (uint32_t i = 0; i < instructions.size(); i++){
            if (instructions[i]->getBaseAddress() == addr){
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

uint32_t CodeBlock::getNumberOfBytes(){
    uint32_t numberOfBytes = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        numberOfBytes += instructions[i]->getSizeInBytes();
    }
    return numberOfBytes;
}

bool BasicBlock::inRange(uint64_t addr){
    if (addr >= getBaseAddress() &&
        addr < getBaseAddress() + getNumberOfBytes()){
        return true;
    }
    return false;
}

CodeBlock::CodeBlock(uint32_t idx, FlowGraph* cfg)
    : Block(ElfClassTypes_CodeBlock,idx,cfg)
{
}


BasicBlock::BasicBlock(uint32_t idx, FlowGraph* cfg)
    : CodeBlock(idx,cfg)
{
    type = ElfClassTypes_BasicBlock;

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

CodeBlock::~CodeBlock(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        delete instructions[i];
    }
}

void CodeBlock::printInstructions(){
    PRINT_INFOR("Instructions for Code Block %d", index);
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

    PRINT_INFOR("BASICBLOCK(%d) range=[0x%llx,0x%llx), %d instructions, flags %c%c%c%c%c%c", index, getBaseAddress(), getBaseAddress()+getNumberOfBytes(), getNumberOfInstructions(), pad, ent, ext, ctr, rch);
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

    uint32_t numberOfBranches = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isControl() && IS_BYTE_SOURCE_APPLICATION(instructions[i]->getByteSource())){
            numberOfBranches++;
        }
    }
    if (numberOfBranches > 1){
        PRINT_ERROR("Block at %#llx should only have 1 branch (%d found)", baseAddress, numberOfBranches);
        return false;
    }

    return true;
}

uint64_t BasicBlock::findInstrumentationPoint(uint32_t size, InstLocations loc){

    for (uint32_t i = 0; i < instructions.size(); i++){
        uint32_t j = i;
        uint32_t instBytes = 0;
        while (j < instructions.size() && instructions[j]->isRelocatable()){
            instBytes += instructions[j]->getSizeInBytes();
            j++;
        }
        if (instBytes >= size){
            return instructions[i]->getBaseAddress();
        }
    }
    return 0;
}

Vector<Instruction*>* CodeBlock::swapInstructions(uint64_t addr, Vector<Instruction*>* replacements){
    Instruction* tgtInstruction = getInstructionAtAddress(addr);
    ASSERT(tgtInstruction && "This basic block should have an instruction at the given address");

    Vector<Instruction*>* replaced = new Vector<Instruction*>();
    if (!(*replacements).size()){
        return replaced;
    }

    // find out how many bytes we need to replace
    uint32_t bytesToReplace = 0;
    for (uint32_t i = 0; i < (*replacements).size(); i++){
        bytesToReplace += (*replacements)[i]->getSizeInBytes();
    }

    // remove the instructions from the basic block and add them to the return array
    uint32_t replacedBytes = 0;
    uint32_t idx = tgtInstruction->getIndex();

    while (replacedBytes < bytesToReplace){
        (*replaced).append(instructions.remove(idx));
        ASSERT(instructions.size() >= idx && "You ran out of instructions in this block");
        replacedBytes += (*replaced).back()->getSizeInBytes();
    }

    while (bytesToReplace < replacedBytes){
        (*replacements).append(InstructionGenerator::generateNoop());
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
        instructions[i]->setBaseAddress(replacedBytes+baseAddress);
        replacedBytes += instructions[i]->getSizeInBytes();
    }

    verify();

    return replaced;
}
