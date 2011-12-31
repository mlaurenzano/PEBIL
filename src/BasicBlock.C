/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <BasicBlock.h>

#include <ElfFileInst.h>
#include <Function.h>
#include <FlowGraph.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <Instrumentation.h>

static const char* bytes_not_instructions = "<_pebil_unreachable_text>";

uint32_t BasicBlock::getDefXIter(){
    uint32_t defcnt = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->hasDefXIter()){
            defcnt++;
        }
    }
    return defcnt;
}

bool BasicBlock::endsWithCall(){
    return  instructions.back()->isCall();
}

uint32_t BasicBlock::getNumberOfMemoryBytes(){
    uint32_t byteCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        byteCount += instructions[i]->getNumberOfMemoryBytes();
    }
    return byteCount;
}

bool BasicBlock::isInLoop(){
    return flowGraph->isBlockInLoop(getIndex());
}

uint32_t BasicBlock::searchForArgsPrep(bool is64Bit){
    ASSERT(containsCallToRange(0,-1));
    uint32_t argsToSearch = Num__64_bit_StackArgs;

    bool foundArgs[argsToSearch];
    bzero(foundArgs, sizeof(bool) * argsToSearch);

    uint32_t numArgs = 0;
    if (is64Bit){
        for (uint32_t i = 0; i < instructions.size(); i++){
            //            instructions[i]->print();
            if (instructions[i]->getInstructionType() == X86InstructionType_int ||
                instructions[i]->getInstructionType() == X86InstructionType_move){
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

uint32_t CodeBlock::addTailJump(X86Instruction* tgtInstruction){
    instructions.append(tgtInstruction);
    byteCountUpdate = true;
    return getNumberOfBytes();
}

uint32_t BasicBlock::bloat(Vector<InstrumentationPoint*>* instPoints){
    PRINT_DEBUG_FUNC_RELOC("fluffing block at %llx for function %s", baseAddress, getLeader()->getContainer()->getName());

    PRINT_DEBUG_BLOAT_FILTER("block range for bloat [%#llx,%#llx)", getBaseAddress(), getBaseAddress() + getNumberOfBytes());
    for (uint32_t i = 0; i < (*instPoints).size(); i++){
        DEBUG_BLOAT_FILTER((*instPoints)[i]->getSourceObject()->print();)
        ASSERT((*instPoints)[i]->getSourceObject()->getContainer()->getType() == PebilClassType_Function);
        Function* pointsFunction = (Function*)(*instPoints)[i]->getSourceObject()->getContainer();
        ASSERT(pointsFunction->getHashCode().getValue() == flowGraph->getFunction()->getHashCode().getValue());
    }

    X86Instruction* firstInstruction = instructions[0];

    Vector<InstrumentationPoint*> expansions;
    Vector<uint32_t> expansionIndices;
    for (uint32_t i = 0; i < (*instPoints).size(); i++){
        expansions.append((*instPoints)[i]);
        if ((*instPoints)[i]->getInstLocation() == InstLocation_prior){
            expansionIndices.append((*instPoints)[i]->getSourceObject()->getIndex());
        } else if ((*instPoints)[i]->getInstLocation() == InstLocation_after){
            expansionIndices.append((*instPoints)[i]->getSourceObject()->getIndex() + 1);
        } else if ((*instPoints)[i]->getInstLocation() == InstLocation_replace){
            // send a dummy value so that no expansion happens
            expansionIndices.append(getNumberOfInstructions() + 2);
        } else {
            __SHOULD_NOT_ARRIVE;
        }
    }

    PRINT_DEBUG_BLOAT_FILTER("Printing expansions");
    for (uint32_t i = 0; i < expansions.size(); i++){
        DEBUG_BLOAT_FILTER(expansions[i]->getSourceObject()->print();)
    }
    for (int32_t i = expansions.size()-1; i >= 0; i--){
        uint32_t bloatAmount = expansions[i]->getNumberOfBytes();
        uint32_t instructionIdx = expansionIndices[i];
        PRINT_DEBUG_BLOAT_FILTER("bloating point at instruction %#llx by %d bytes", instructions[instructionIdx]->getProgramAddress(), bloatAmount);
        if (instructionIdx < instructions.size() + 1){
            for (uint32_t j = 0; j < bloatAmount; j++){
                instructions.insert(X86InstructionFactory::emitNop(), instructionIdx);
                byteCountUpdate = true;
            }
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

uint32_t BasicBlock::getNumberOfLoads(){
    uint32_t loadCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isLoad()){
            loadCount++;
        }
    }
    return loadCount;
}
uint32_t BasicBlock::getNumberOfStores(){
    uint32_t storeCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isStore()){
            storeCount++;
        }
    }
    return storeCount;
}

uint32_t BasicBlock::getNumberOfSpecialRegOps(){
    uint32_t specialRegCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isSpecialRegOp()){
            specialRegCount++;
        }
    }
    return specialRegCount;
}

uint32_t BasicBlock::getNumberOfShiftRotOps(){
    uint32_t shiftRotCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isSpecialRegOp()){
            shiftRotCount++;
        }
    }
    return shiftRotCount;
}

uint32_t BasicBlock::getNumberOfSyscalls(){
    uint32_t syscallCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isSystemCall()){
            syscallCount++;
        }
    }
    return syscallCount;
}

uint32_t BasicBlock::getNumberOfLogicOps(){
    uint32_t logicOpCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isLogicOp()){
            logicOpCount++;
        }
    }
    return logicOpCount;
}

uint32_t BasicBlock::getNumberOfBranches(){
    uint32_t branchCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBranch()){
            branchCount++;
        }
    }
    return branchCount;
}

uint32_t BasicBlock::getNumberOfBinMem(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinMem()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinSystem(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinSystem()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinMove(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinMove()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinSinglev(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinFloatv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinSingle(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinFloat()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinDoublev(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinDoublev()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinDouble(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinDouble()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinFloatv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinSinglev() || instructions[i]->isBinDoublev()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinFloat(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinSingle() || instructions[i]->isBinDouble()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinFloats(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinSingles() || instructions[i]->isBinDoubles()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinSingles(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinSingles()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinDoubles(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinDoubles()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinWordv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinWordv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinWord(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinWord()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinBytev(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinBytev()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinByte(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinByte()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinDwordv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinDwordv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinDword(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinDword()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinQwordv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinQwordv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinQword(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinQword()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinIntv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinIntv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinInt(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinInt()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinBin(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinBin()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinBinv(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinBinv()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinUncond(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinUncond()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinCond(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinCond()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinInvalid(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinInvalid()){
	    instructions[i]->print();
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinCache(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinCache()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinString(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinString()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinStack(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinStack()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinOther(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinOther()){
            binCount++;
        }
    }
    return binCount;
}

uint32_t BasicBlock::getNumberOfBinUnknown(){
    uint32_t binCount = 0;
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (instructions[i]->isBinUnknown()){
            binCount++;
        }
    }
    return binCount;
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

uint32_t CodeBlock::getAllInstructions(X86Instruction** allinsts, uint32_t nexti){
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

uint32_t BasicBlock::removeSourceBlock(BasicBlock* srcBlock){
    for (uint32_t i = 0; i < sourceBlocks.size(); i++){
        if (srcBlock->getIndex() == sourceBlocks[i]->getIndex()){
            sourceBlocks.remove(i);
            i--;
        }
    }
    return sourceBlocks.size();
}

uint32_t BasicBlock::removeTargetBlock(BasicBlock* tgtBlock){
    for (uint32_t i = 0; i < targetBlocks.size(); i++){
        if (tgtBlock->getIndex() == targetBlocks[i]->getIndex()){
            targetBlocks.remove(i);
            i--;
        }
    }
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
    X86Instruction* last = instructions.back();
    return last->controlFallsThrough();
}

bool BasicBlock::containsOnlyControl(){
    for (uint32_t i = 0; i < instructions.size(); i++){
        if (!instructions[i]->isControl() && !instructions[i]->isNop()){
            return false;
        }
    }
    return true;
}

bool BasicBlock::isDominatedBy(BasicBlock* bb){
    BasicBlock* dom = this;
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

uint32_t CodeBlock::addInstruction(X86Instruction* inst){
    if (!instructions.size()){
        baseAddress = inst->getBaseAddress();
    }
    inst->setIndex(instructions.size());
    instructions.append(inst);
    sizeInBytes += instructions.size();
    byteCountUpdate = true;
    return instructions.size();
}

X86Instruction* CodeBlock::getInstructionAtAddress(uint64_t addr){
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
    defXIterCount = 0;
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
            print();
            printInstructions();
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
    __SHOULD_NOT_ARRIVE;
    if (loc == InstLocation_prior){
        addr = addr - size;
    } else if (loc == InstLocation_after){
        X86Instruction* instruction = getInstructionAtAddress(addr);
        ASSERT(instruction);
        addr = instruction->getBaseAddress() + instruction->getSizeInBytes();
    } else {
        __SHOULD_NOT_ARRIVE;
    }

    ASSERT(inRange(addr) && "Instrumentation address should fall within BasicBlock bounds");

    X86Instruction* instruction = getInstructionAtAddress(addr);
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

Vector<X86Instruction*>* CodeBlock::swapInstructions(uint64_t addr, Vector<X86Instruction*>* replacements){
    X86Instruction* tgtInstruction = getInstructionAtAddress(addr);
    if (!tgtInstruction){
        PRINT_INFOR("looking for addr %#llx inside block with range [%#llx,%#llx)", addr, getBaseAddress(), getBaseAddress() + getNumberOfBytes());
        printInstructions();
        //flowGraph->getFunction()->printInstructions();
    }
    ASSERT(tgtInstruction && "This basic block should have an instruction at the given address");

    Vector<X86Instruction*>* replaced = new Vector<X86Instruction*>();
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
        ASSERT(instructions.size() > idx && "You ran out of instructions in this block");
        (*replaced).append(instructions.remove(idx));
        replacedBytes += (*replaced).back()->getSizeInBytes();
    }

    while (bytesToReplace < replacedBytes){
        (*replacements).append(X86InstructionFactory::emitNop());
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
