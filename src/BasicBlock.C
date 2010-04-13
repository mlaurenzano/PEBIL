#include <BasicBlock.h>

#include <ElfFileInst.h>
#include <Function.h>
#include <FlowGraph.h>
#include <InstrucX86.h>
#include <InstrucX86Generator.h>
#include <Instrumentation.h>

static const char* bytes_not_instructions = "<_pebil_unreachable_text>";

uint32_t BasicBlock::searchForArgsPrep(bool is64Bit){
    ASSERT(containsCallToRange(0,-1));
    uint32_t argsToSearch = Num__64_bit_StackArgs;

    bool foundArgs[argsToSearch];
    bzero(foundArgs, sizeof(bool) * argsToSearch);

    uint32_t numArgs = 0;
    if (is64Bit){
        for (uint32_t i = 0; i < instructions.size(); i++){
            //            instructions[i]->print();
            if (instructions[i]->getInstructionType() == InstrucX86Type_int){
                OperandX86* destOp = instructions[i]->getOperand(COMP_DEST_OPERAND);

                if (!destOp->getValue()){
                    for (uint32_t j = 0; j < Num__64_bit_StackArgs; j++){
                        if (destOp->getBaseRegister() == map64BitArgToReg(j)){
                            foundArgs[j] = true;
                        }
                    }
                }
                //                instructions[i]->getOperand(COMP_DEST_OPERAND)->print();
            }
        }
    } else {
        __FUNCTION_NOT_IMPLEMENTED;
    }

    for (uint32_t i = 0; i < argsToSearch; i++){
        if (foundArgs[i]){
            numArgs++;
        } else {
            break;
        }
    }


    PRINT_INFOR("found %d args ------------------------------------------------------", numArgs);

    return numArgs;
}

uint64_t CodeBlock::getProgramAddress(){
    ASSERT(instructions.size());
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (IS_BYTE_SOURCE_APPLICATION(instructions[i]->getByteSource())){
            return instructions[i]->getProgramAddress();
        }
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}

void BasicBlock::findCompareAndCBranch(){
    if (instructions.size() < 2){
        ASSERT(instructions.size());
        return;
    }
    if (instructions.back()->isConditionalBranch()){
        if (!instructions[instructions.size()-2]->isConditionCompare()){
            //PRINT_INFOR("found block with cond/br split");
            //            printDisassembly(false);
            //            setCmpCtrlSplit();
        }
    }
    return;
}

uint32_t CodeBlock::addTailJump(InstrucX86* tgtInstruction){
    instructions.append(tgtInstruction);
    byteCountUpdate = true;
    return getNumberOfBytes();
}

uint32_t BasicBlock::bloat(Vector<InstrumentationPoint*>* instPoints){
    PRINT_DEBUG_FUNC_RELOC("fluffing block at %llx for function %s", baseAddress, getContainer()->getName());

    PRINT_DEBUG_BLOAT_FILTER("block range for bloat [%#llx,%#llx)", getBaseAddress(), getBaseAddress() + getNumberOfBytes());
    for (uint32_t i = 0; i < (*instPoints).size(); i++){
        DEBUG_BLOAT_FILTER((*instPoints)[i]->getSourceObject()->print();)
        ASSERT(inRange((*instPoints)[i]->getInstBaseAddress()));
    }
    (*instPoints).sort(compareInstBaseAddress);

    Vector<InstrumentationPoint*> expansions;
    Vector<uint32_t> expansionIndices;
    for (uint32_t i = 0; i < (*instPoints).size();){
        expansions.append((*instPoints)[i]);
        if ((*instPoints)[i]->getInstLocation() == InstLocation_prior){
            expansionIndices.append((*instPoints)[i]->getSourceObject()->getIndex());
        } else if ((*instPoints)[i]->getInstLocation() == InstLocation_after){
            expansionIndices.append((*instPoints)[i]->getSourceObject()->getIndex() + 1);
        } else {
            __SHOULD_NOT_ARRIVE;
        }
        uint32_t j = i+1;
        while (j < (*instPoints).size() && (*instPoints)[i]->getInstBaseAddress() == (*instPoints)[j]->getInstBaseAddress() &&
               (*instPoints)[i]->getInstLocation() == (*instPoints)[j]->getInstLocation()){
            j++;
        }
        i = j;
    }

    PRINT_DEBUG_BLOAT_FILTER("Printing expansions");
    for (uint32_t i = 0; i < expansions.size(); i++){
        DEBUG_BLOAT_FILTER(expansions[i]->getSourceObject()->print();)
    }
    for (int32_t i = expansions.size()-1; i >= 0; i--){
        bool isRep = false;
        for (int32_t j = i-1; j >= 0; j--){
            if ((*instPoints)[j]->getInstBaseAddress() == expansions[i]->getInstBaseAddress()){
                if ((*instPoints)[j]->getInstrumentationMode() != InstrumentationMode_inline ||
                    (*instPoints)[i]->getInstrumentationMode() != InstrumentationMode_inline){
                    isRep = true;
                }
            }
        }
        uint32_t bloatAmount;
        if (isRep){
            bloatAmount = Size__uncond_jump;
        } else {
            bloatAmount = expansions[i]->getNumberOfBytes();
        }

        uint32_t instructionIdx = expansionIndices[i];
        PRINT_DEBUG_BLOAT_FILTER("bloating point at instruction %#llx by %d bytes", instructions[instructionIdx]->getProgramAddress(), bloatAmount);

        for (uint32_t j = 0; j < bloatAmount; j++){
            instructions.insert(InstrucX86Generator::generateNoop(), instructionIdx);
            byteCountUpdate = true;
        }
    }

    for (uint32_t i = 0; i < instructions.size(); i++){
        // convert all branches to use 4byte operands (giving them much larger immediate range)
        if (instructions[i]->isControl() && !instructions[i]->isReturn()){
            if (instructions[i]->bytesUsedForTarget() < sizeof(uint32_t)){
                PRINT_DEBUG_FUNC_RELOC("This instruction uses %d bytes for target calculation", instructions[i]->bytesUsedForTarget());
                instructions[i]->convertTo4ByteTargetOperand();
                byteCountUpdate = true;
            }
        }
    }
    setBaseAddress(baseAddress);

    return getNumberOfBytes();
}

uint32_t BasicBlock::getNumberOfIntegerOps(){
    uint32_t intCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isIntegerOperation()){
            intCount++;
        }
    }
    return intCount;
}

uint32_t BasicBlock::getNumberOfStringOps(){
    uint32_t strCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isStringOperation()){
            strCount++;
        }
    }
    return strCount;
}

uint32_t BasicBlock::getNumberOfMemoryOps(){
    uint32_t memCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isMemoryOperation()){
            memCount++;
        }
    }
    return memCount;
}

uint32_t BasicBlock::getNumberOfFloatOps(){
    uint32_t fpCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isFloatPOperation()){
            fpCount++;
        }
    }
    return fpCount;
}

// obviously these are not implemented correctly yet
uint32_t BasicBlock::getNumberOfLoads(){
    return getNumberOfMemoryOps();
}
uint32_t BasicBlock::getNumberOfStores(){
    return 0;
}

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

Block::Block(PebilClassTypes typ, uint32_t idx, FlowGraph* cfg)
    : Base(typ)
{
    index = idx;
    flowGraph = cfg;
}

RawBlock::RawBlock(uint32_t idx, FlowGraph* cfg, char* byt, uint32_t sz, uint64_t addr)
    : Block(PebilClassType_RawBlock,idx,cfg)
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

uint32_t CodeBlock::getAllInstructions(InstrucX86** allinsts, uint32_t nexti){
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
        instructions[i]->setIndex(i);
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
    if (instructions.back()->isReturn()){
        return true;
    }
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBranch() &&
            !getFlowGraph()->getFunction()->inRange(instructions[i]->getTargetAddress())){
            return true;
        }
    }
    return false;
}


bool BasicBlock::controlFallsThrough(){
    InstrucX86* last = instructions.back();
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

uint32_t CodeBlock::addInstruction(InstrucX86* inst){
    if (!instructions.size()){
        baseAddress = inst->getBaseAddress();
    }
    inst->setIndex(instructions.size());
    instructions.append(inst);
    sizeInBytes += instructions.size();
    byteCountUpdate = true;
    return instructions.size();
}

InstrucX86* CodeBlock::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->getBaseAddress() == addr){
            return instructions[i];
        }
    }
    return NULL;
}

uint32_t CodeBlock::getNumberOfBytes(){
    if (byteCountUpdate){
        numberOfBytes = 0;
        for (uint32_t i = 0; i < instructions.size(); i++){
            numberOfBytes += instructions[i]->getSizeInBytes();
        }
        byteCountUpdate = false;
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
    : Block(PebilClassType_CodeBlock,idx,cfg)
{
    byteCountUpdate = true;
    numberOfBytes = 0;
}


BasicBlock::BasicBlock(uint32_t idx, FlowGraph* cfg)
    : CodeBlock(idx,cfg)
{
    type = PebilClassType_BasicBlock;

    flags = 0;
    immDominatedBy = NULL;

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
    char ccs = '-';

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
    if (isCmpCtrlSplit()){
        ccs = 'S';
    }

    PRINT_INFOR("BASICBLOCK(%d) range=[0x%llx,0x%llx), %d instructions, flags [%c%c%c%c%c%c]", index, getBaseAddress(), getBaseAddress()+getNumberOfBytes(), getNumberOfInstructions(), pad, ent, ext, ctr, rch, ccs);
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

uint64_t BasicBlock::findInstrumentationPoint(uint64_t addr, uint32_t size, InstLocations loc){
    if (loc == InstLocation_prior){
        addr = addr - size;
    } else if (loc == InstLocation_after){
        InstrucX86* instruction = getInstructionAtAddress(addr);
        ASSERT(instruction);
        addr = instruction->getBaseAddress() + instruction->getSizeInBytes();
    } else {
        __SHOULD_NOT_ARRIVE;
    }

    ASSERT(inRange(addr) && "Instrumentation address should fall within BasicBlock bounds");

    InstrucX86* instruction = getInstructionAtAddress(addr);
    if (!instruction){
        print();
    }
    ASSERT(instruction);
    uint32_t instBytes = 0;
    uint32_t instIdx = instruction->getIndex();
    while (instBytes < size){
        if (!instructions[instIdx]->isRelocatable()){
            break;
        }
        instBytes += instruction->getSizeInBytes();
        instIdx++;
    }
    if (instBytes >= size){
        return addr;
    }
    /*
      } else { // loc == InstLocation_dont_care
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
        }
    */
    return 0;
}

Vector<InstrucX86*>* CodeBlock::swapInstructions(uint64_t addr, Vector<InstrucX86*>* replacements){
    InstrucX86* tgtInstruction = getInstructionAtAddress(addr);
    if (!tgtInstruction){
        PRINT_INFOR("looking for addr %#llx", addr);
        printInstructions();
    }
    ASSERT(tgtInstruction && "This basic block should have an instruction at the given address");

    Vector<InstrucX86*>* replaced = new Vector<InstrucX86*>();
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
        ASSERT(instructions.size() >= idx && "You ran out of instructions in this block");
        (*replaced).append(instructions.remove(idx));
        replacedBytes += (*replaced).back()->getSizeInBytes();
    }

    while (bytesToReplace < replacedBytes){
        (*replacements).append(InstrucX86Generator::generateNoop());
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
    byteCountUpdate = true;
    
    return replaced;
}
