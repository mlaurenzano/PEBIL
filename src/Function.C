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

#include <Function.h>

#include <BasicBlock.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <ElfFileInst.h>
#include <FlowGraph.h>
#include <X86Instruction.h>
#include <Instrumentation.h>
#include <LengauerTarjan.h>
#include <SectionHeader.h>
#include <Stack.h>
#include <SymbolTable.h>
#include <TextSection.h>

uint32_t Function::findStackSize(){
    ASSERT(flowGraph);
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = flowGraph->getBasicBlock(i);
        if (bb->isEntry()){
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* ins = bb->getInstruction(j);
                OperandX86* srcop = ins->getOperand(COMP_DEST_OPERAND);
                if (ins->GET(mnemonic) == UD_Isub && !srcop->getValue()){
                    if (srcop->getBaseRegister() == X86_REG_SP){
                        stackSize = ins->getOperand(COMP_SRC_OPERAND)->getValue();
                        break;
                    }
                }
            }
            break;
        }
    }
    if (!stackSize){
        stackSize = Size__trampoline_autoinc;
    }
}

bool Function::hasLeafOptimization(){
    uint32_t numberOfInstructions = getNumberOfInstructions();
    X86Instruction** allInstructions = new X86Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);

    bool callFound = false;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->isFunctionCall()){
            callFound = true;
        }
    }
    delete[] allInstructions;
    return !callFound;
}

void Function::printDisassembly(bool instructionDetail){
    fprintf(stdout, "%llx <func -- %s>:", getBaseAddress(), getName());
    if (getBadInstruction()){
        fprintf(stdout, "badi@ %llx", getBadInstruction());
    }
    fprintf(stdout, "\n");
    if (flowGraph){
        for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
            flowGraph->getBlock(i)->printDisassembly(instructionDetail);
        }
    }
}

uint32_t Function::bloatBasicBlocks(Vector<Vector<InstrumentationPoint*>*>* instPoints){
    uint32_t currByte = 0;
    if ((*instPoints).size() != flowGraph->getNumberOfBasicBlocks()){
        print();
        flowGraph->print();
        PRINT_INFOR("%d inst point lists, %d blocks", (*instPoints).size(), flowGraph->getNumberOfBasicBlocks());
    }
    ASSERT((*instPoints).size() == flowGraph->getNumberOfBasicBlocks());

    uint32_t bbidx = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        Block* block = flowGraph->getBlock(i);
        if (block->getType() == PebilClassType_BasicBlock){
            ((BasicBlock*)block)->bloat((*instPoints)[bbidx]);
            bbidx++;
        }
        block->setBaseAddress(baseAddress + currByte);
        currByte += block->getNumberOfBytes();
    }
    ASSERT(bbidx == flowGraph->getNumberOfBasicBlocks());
    sizeInBytes = currByte;

    return sizeInBytes;
}

uint32_t Function::addSafetyJump(X86Instruction* tgtInstruction){
    Block* block = flowGraph->getBasicBlock(flowGraph->getNumberOfBasicBlocks() - 1);
    if (block->getType() == PebilClassType_BasicBlock){
        CodeBlock* cb = ((CodeBlock*)block);
        sizeInBytes -= cb->getNumberOfBytes();
        sizeInBytes += cb->addTailJump(tgtInstruction);
    } else {
        PRINT_ERROR("End of function %s isn't basic block", getName());
    }
}

bool Function::hasCompleteDisassembly(){
    // if something happened during disassembly that we dont understand
    if (getBadInstruction()){
        return false;
    }    

    if (isDisasmFail()){
        return false;
    }

    // if this function references data that is inside the function body
    if (hasSelfDataReference()){
        return false;
    }

    /*
    if (refersToInstruction()){
        return false;
    }
    */
    
    // if this function calls __i686.get_pc_thunk.bx
    for (uint32_t i = 0; i < textSection->getNumberOfTextObjects(); i++){
        TextObject* tobj = textSection->getTextObject(i);
        if (tobj->getType() == PebilClassType_Function){
            Function* func = (Function*)tobj;
            if (!strcmp(func->getName(),"__i686.get_pc_thunk.bx")){
                if (containsCallToRange(func->getBaseAddress(),func->getBaseAddress()+func->getSizeInBytes())){
                    return false;
                }
            }
        }
    }
    return true;
}

bool Function::callsSelf(){
    return containsCallToRange(baseAddress + 1, baseAddress + getNumberOfBytes());
}

bool Function::refersToInstruction(){
    uint32_t numberOfInstructions = getNumberOfInstructions();
    X86Instruction** allInstructions = new X86Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->getAddressAnchor() && !allInstructions[i]->isControl()){
            if (allInstructions[i]->getAddressAnchor()->getLink()->getType() == PebilClassType_X86Instruction){
                if (allInstructions[i]->isMemoryOperation()){
                    allInstructions[i]->print();
                    delete[] allInstructions;
                    return true;
                }
            }
        }
    }
    delete[] allInstructions;
    return false;    
}

bool Function::hasSelfDataReference(){
    uint32_t numberOfInstructions = getNumberOfInstructions();
    X86Instruction** allInstructions = new X86Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->getAddressAnchor()){
            if (allInstructions[i]->usesRelativeAddress() &&
                !allInstructions[i]->isControl() &&
                inRange(allInstructions[i]->getBaseAddress() + allInstructions[i]->getAddressAnchor()->getLinkOffset() + allInstructions[i]->getSizeInBytes())){
                PRINT_DEBUG_FUNC_RELOC("Instruction self-data-ref inside function %s @ %#llx", getName(), allInstructions[i]->getBaseAddress());
                DEBUG_FUNC_RELOC(allInstructions[i]->print();)
                delete[] allInstructions;
                return true;
            }
        }
    }
    delete[] allInstructions;
    return false;
}

bool Function::containsReturn(){
    uint32_t numberOfInstructions = getNumberOfInstructions();
    X86Instruction** allInstructions = new X86Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);
    bool hasReturn = false;

    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->isReturn() || allInstructions[i]->isHalt()){
            hasReturn = true;
            i = numberOfInstructions;
        }
    }
    delete[] allInstructions;

    return hasReturn;
}

bool Function::containsCallToRange(uint64_t lowAddr, uint64_t highAddr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->containsCallToRange(lowAddr,highAddr)){
            return true;
        }
    }
    return false;
}

uint32_t Function::getAllInstructions(X86Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        instructionCount += flowGraph->getBasicBlock(i)->getAllInstructions(allinsts, instructionCount + nexti);
    }
    ASSERT(instructionCount == getNumberOfInstructions());
    return instructionCount;
}

Vector<X86Instruction*>* Function::swapInstructions(uint64_t addr, Vector<X86Instruction*>* replacements){
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        if (getBasicBlock(i)->inRange(addr)){
            return getBasicBlock(i)->swapInstructions(addr, replacements);
        }
    }
    print();
    printInstructions();
    PRINT_ERROR("Cannot find instructions at address 0x%llx to replace (function %s)", addr, getName());
    return 0;
}

void Function::setBaseAddress(uint64_t newBaseAddr){
    baseAddress = newBaseAddr;
    flowGraph->setBaseAddress(newBaseAddr);
}

uint32_t Function::getNumberOfBytes(){
    ASSERT(flowGraph);
    return flowGraph->getNumberOfBytes();
}

uint32_t Function::getNumberOfInstructions() { 
    ASSERT(flowGraph);
    return flowGraph->getNumberOfInstructions(); 
}


uint32_t Function::getNumberOfBasicBlocks() { 
    ASSERT(flowGraph);
    return flowGraph->getNumberOfBasicBlocks(); 
}


BasicBlock* Function::getBasicBlock(uint32_t idx){
    ASSERT(flowGraph);
    return flowGraph->getBasicBlock(idx);
}

void Function::printInstructions(){
    X86Instruction** allInstructions = new X86Instruction*[getNumberOfInstructions()];
    getAllInstructions(allInstructions,0);
    for (uint32_t i = 0; i < getNumberOfInstructions(); i++){
        ASSERT(allInstructions[i]);
        allInstructions[i]->print();
    }
    delete[] allInstructions;
}

void Function::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        flowGraph->getBlock(i)->dump(binaryOutputFile,offset+currByte);
        PRINT_DEBUG_CFG("\tDumping block for function %s at %#llx for %d bytes", getName(), flowGraph->getBlock(i)->getBaseAddress(), flowGraph->getBlock(i)->getNumberOfBytes());
        currByte += flowGraph->getBlock(i)->getNumberOfBytes();
    }
    if (currByte != sizeInBytes){
        PRINT_ERROR("Function %s dumped %d bytes and has %d bytes", getName(), currByte, sizeInBytes);
    }
    ASSERT(currByte == sizeInBytes);
}

uint32_t Function::digest(Vector<AddressAnchor*>* addressAnchors){
    Vector<X86Instruction*>* allInstructions = NULL;

    // try to use a recursive algorithm
    allInstructions = digestRecursive();

    // use a linear algorithm if recursive failed
    if (!allInstructions){
        ASSERT(getBadInstruction());
        allInstructions = digestLinear();
    } else {
        setRecursiveDisasm();
    }

    ASSERT(allInstructions);

    if (!isDisasmFail()){
        generateCFG(allInstructions, addressAnchors);        
        findStackSize();
#ifndef NO_REG_ANALYSIS
        flowGraph->flowAnalysis();
#endif
    }

    delete allInstructions;

    return sizeInBytes;
}

Vector<X86Instruction*>* Function::digestRecursive(){
    Vector<X86Instruction*>* allInstructions = new Vector<X86Instruction*>();
    X86Instruction* currentInstruction;
    uint64_t currentAddress = 0;

    PRINT_DEBUG_CFG("Recursively digesting function %s at [%#llx,%#llx)", getName(), getBaseAddress(), getBaseAddress() + getSizeInBytes());

    Stack<uint64_t> unprocessed = Stack<uint64_t>(sizeInBytes);

    // put the entry point onto stack
    unprocessed.push(baseAddress);

    // build an array of all reachable instructions
    while (!unprocessed.empty() && !getBadInstruction()){
        currentAddress = unprocessed.pop();

        void* inst = bsearch(&currentAddress, &(*allInstructions), (*allInstructions).size(), sizeof(X86Instruction*), searchBaseAddress);
        if (inst){
            X86Instruction* tgtInstruction = *(X86Instruction**)inst;
            ASSERT(tgtInstruction->getBaseAddress() == currentAddress && "Problem in disassembly -- found instruction that enters the middle of another instruction");
            continue;
        }
        if (!inRange(currentAddress)){
            PRINT_WARN(4,"In function %s, control flows out of function bound (%#llx)", getName(), currentAddress);
            continue;
        }

        currentInstruction = new X86Instruction(this, currentAddress, textSection->getStreamAtAddress(currentAddress), ByteSource_Application_Function, 0);

        PRINT_DEBUG_CFG("recursive cfg: address %#llx with %d bytes", currentAddress, currentInstruction->getSizeInBytes());
        uint64_t checkAddr = currentInstruction->getBaseAddress();

        if (currentInstruction->getInstructionType() == UD_Iinvalid){
            setBadInstruction(currentInstruction->getBaseAddress());
        }

        (*allInstructions).insertSorted(currentInstruction,compareBaseAddress);

        // make sure the targets of this branch have not been processed yet
        uint64_t fallThroughAddr = currentInstruction->getBaseAddress() + currentInstruction->getSizeInBytes();
        PRINT_DEBUG_CFG("\tChecking FTaddr %#llx", fallThroughAddr);
        void* fallThrough = bsearch(&fallThroughAddr,&(*allInstructions),(*allInstructions).size(),sizeof(X86Instruction*),searchBaseAddress);
        if (!fallThrough){
            if (currentInstruction->controlFallsThrough()){
                PRINT_DEBUG_CFG("\t\tpushing %#llx", fallThroughAddr);
                unprocessed.push(fallThroughAddr);
            } else { 
                PRINT_DEBUG_CFG("\t\tsetting FTAddr = 0");
                fallThroughAddr = 0;
            }
        } else {
            X86Instruction* tgtInstruction = *(X86Instruction**)fallThrough;
            if (tgtInstruction->getBaseAddress() != fallThroughAddr){
                setBadInstruction(fallThroughAddr);
            }
        }
        
        Vector<uint64_t>* controlTargetAddrs = new Vector<uint64_t>();
        Vector<uint64_t>* controlStorageAddrs = new Vector<uint64_t>();
        if (currentInstruction->isJumpTableBase()){
            PRINT_DEBUG_CFG("\t\t%#llx is jump table base", currentInstruction->getBaseAddress());
            uint64_t jumpTableBase = currentInstruction->findJumpTableBaseAddress(allInstructions);
            if (inRange(jumpTableBase) || !jumpTableBase){
                PRINT_DEBUG_CFG("cannot locate jump table info");
                setBadInstruction(currentInstruction->getBaseAddress());
            } else {
                ASSERT(!(*controlTargetAddrs).size());
                currentInstruction->computeJumpTableTargets(jumpTableBase, this, controlTargetAddrs, controlStorageAddrs);
                PRINT_DEBUG_CFG("Jump table targets (%d):", (*controlTargetAddrs).size());
            }
        } else if (currentInstruction->usesControlTarget()){
            (*controlTargetAddrs).append(currentInstruction->getTargetAddress());
        }

        for (uint32_t i = 0; i < (*controlTargetAddrs).size(); i++){
            uint64_t controlTargetAddr = (*controlTargetAddrs)[i];
            PRINT_DEBUG_CFG("\tChecking CTaddr %#llx", controlTargetAddr);
            void* controlTarget = bsearch(&controlTargetAddr,&(*allInstructions),(*allInstructions).size(),sizeof(X86Instruction*),searchBaseAddress);
            if (!controlTarget){
                if (inRange(controlTargetAddr)                 // target address is in this functions also
                    && controlTargetAddr != fallThroughAddr){  // target and fall through are the same, meaning the address is already pushed above
                    PRINT_DEBUG_CFG("\t\tpushing %#llx", controlTargetAddr);
                    unprocessed.push(controlTargetAddr);
                }
            } else {
                X86Instruction* tgtInstruction = *(X86Instruction**)controlTarget;
                if (tgtInstruction->getBaseAddress() != controlTargetAddr){
                    setBadInstruction(controlTargetAddr);
                }              
            }
        }
        delete controlTargetAddrs;
        delete controlStorageAddrs;
    }

    qsort(&(*allInstructions), (*allInstructions).size(), sizeof(X86Instruction*), compareBaseAddress);
    //    ASSERT((*allInstructions).isSorted(compareBaseAddress));

    for (uint32_t i = 0; i < (*allInstructions).size() - 1; i++){
        if ((*allInstructions)[i]->getBaseAddress() + (*allInstructions)[i]->getSizeInBytes() >
            (*allInstructions)[i+1]->getBaseAddress()){
            setBadInstruction((*allInstructions)[i+1]->getBaseAddress());
        }
    }

    if (getBadInstruction()){
        for (uint32_t i = 0; i < (*allInstructions).size(); i++){
            delete (*allInstructions)[i];
        }
        delete allInstructions;
        return NULL;
    } else {

        // in case the disassembler found an instruction that exceeds the function boundary, we will
        // reduce the size of the last instruction accordingly so that the extra bytes will not be
        // used. This can happen when data is stored at the end of function code
        X86Instruction* tail = (*allInstructions).back();
        uint32_t currByte = tail->getBaseAddress() + tail->getSizeInBytes() - getBaseAddress();
        if ( currByte > sizeInBytes){
            uint32_t extraBytes = currByte - sizeInBytes;
            tail->setSizeInBytes(tail->getSizeInBytes() - extraBytes);

            char oType[9];
            if (getType() == PebilClassType_FreeText){
                sprintf(oType, "%s", "FreeText\0");
            } else if (getType() == PebilClassType_Function){
                sprintf(oType, "%s", "Function\0");
            }

            PRINT_WARN(3,"Found instructions that rexceed the %s boundary in %.24s by %d bytes", oType, getName(), extraBytes);
        }

    }

    return allInstructions;
}

uint32_t Function::generateCFG(Vector<X86Instruction*>* instructions, Vector<AddressAnchor*>* addressAnchors){
    BasicBlock* currentBlock = NULL;
    BasicBlock* entryBlock = NULL;
    uint32_t numberOfBasicBlocks = 0;

    ASSERT((*instructions).size());

    flowGraph = new FlowGraph(this);

    PRINT_DEBUG_CFG("Building CFG for function %s -- have %d instructions", getName(), (*instructions).size());

    // find the block leaders
    (*instructions)[0]->setLeader(true);
    Vector<uint64_t> leaderAddrs;
    Vector<uint64_t> anchorAddrs;
    Vector<uint64_t> anchorInstrs;
    
    for (uint32_t i = 0; i < (*instructions).size(); i++){
        Vector<uint64_t>* controlTargetAddrs = new Vector<uint64_t>();
        Vector<uint64_t>* controlStorageAddrs = new Vector<uint64_t>();
        if ((*instructions)[i]->isJumpTableBase()){
            setJumpTable();

            uint64_t jumpTableBase = (*instructions)[i]->findJumpTableBaseAddress(instructions);
            if (!jumpTableBase){
                PRINT_WARN(6,"Cannot determine indirect jump target for instruction at %#llx", (*instructions)[i]->getBaseAddress());
                ASSERT(getBadInstruction());
                setJumpTable();
            } else if (inRange(jumpTableBase)){
                ASSERT(getBadInstruction());
                setJumpTable();
            } else {
                ASSERT(!(*controlTargetAddrs).size() && !(*controlStorageAddrs).size());
                TableModes tableMode = (*instructions)[i]->computeJumpTableTargets(jumpTableBase, this, controlTargetAddrs, controlStorageAddrs);
                ASSERT((*controlTargetAddrs).size() == (*controlStorageAddrs).size());
                if (tableMode == TableMode_direct){
                    if ((*controlStorageAddrs).size()){
                        //PRINT_INFOR("Jump table in memory at %#llx for %d entries", (*controlStorageAddrs)[0], (*controlStorageAddrs).size());
                    }
                    for (uint32_t j = 0; j < (*controlTargetAddrs).size(); j++){
                        anchorInstrs.append((*controlTargetAddrs)[j]);
                        anchorAddrs.append((*controlStorageAddrs)[j]);
                    }
                } else {
                    //PRINT_INFOR("Untouched jump table at %#llx", (*instructions)[i]->getBaseAddress());
                    setJumpTable();
                }
                PRINT_DEBUG_CFG("Jump table targets (%d):", (*controlTargetAddrs).size());
            }
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        } else if ((*instructions)[i]->usesControlTarget()){
            (*controlTargetAddrs).append((*instructions)[i]->getTargetAddress());
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        } else if ((*instructions)[i]->isReturn()){
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        }
        for (uint32_t j = 0; j < (*controlTargetAddrs).size(); j++){
            leaderAddrs.append((*controlTargetAddrs)[j]);
        }
        delete controlTargetAddrs;
        delete controlStorageAddrs;
    }

    for (uint32_t i = 0; i < leaderAddrs.size(); i++){
        uint64_t tgtAddr = leaderAddrs[i];
        void* inst = bsearch(&tgtAddr,&(*instructions),(*instructions).size(),sizeof(X86Instruction*),searchBaseAddress);
        PRINT_DEBUG_CFG("Looking for leader addr %#llx", tgtAddr);
        if (inst){
            PRINT_DEBUG_CFG("\tFound it");
            X86Instruction* tgtInstruction = *(X86Instruction**)inst;
            if (!getBadInstruction()){
                if (tgtInstruction->getBaseAddress() != tgtAddr){
                    PRINT_ERROR("found instruction that enters the middle of another instruction -- function %s instruction %#llx", getName(), tgtAddr);
                }
            }
            tgtInstruction->setLeader(true);
        }
    }

    for (uint32_t i = 0; i < (*instructions).size(); i++){
        if ((*instructions)[i]->isLeader()){
            currentBlock = new BasicBlock(numberOfBasicBlocks++,flowGraph);
            currentBlock->setBaseAddress((*instructions)[i]->getBaseAddress());
            if (!flowGraph->getNumberOfBlocks()){
                entryBlock = currentBlock;
            }
            flowGraph->addBlock(currentBlock);
            PRINT_DEBUG_CFG("Starting new Block");
        }
        PRINT_DEBUG_CFG("\tAdding instruction %#llx with %d bytes", (*instructions)[i]->getBaseAddress(), (*instructions)[i]->getSizeInBytes());
        currentBlock->addInstruction((*instructions)[i]);
    }

    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        PRINT_DEBUG_CFG("Have a block at adr %#llx", flowGraph->getBlock(i)->getBaseAddress());
    }

    flowGraph->connectGraph(entryBlock);
    flowGraph->setImmDominatorBlocks();

    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        if (flowGraph->getBlock(i)->getType() == PebilClassType_BasicBlock){
            BasicBlock* bb = (BasicBlock*)flowGraph->getBlock(i);
            bb->findCompareAndCBranch();
        }
    }


    DEBUG_CFG(print();)

    ASSERT(flowGraph->getNumberOfBlocks() == flowGraph->getNumberOfBasicBlocks());
    ASSERT(flowGraph->getNumberOfBlocks());

    // now find any areas not covered by basic blocks and put them into RawBlocks
    Vector<RawBlock*> unknownBlocks;
    if (flowGraph->getBlock(0)->getBaseAddress() > getBaseAddress()){
        PRINT_DEBUG_CFG("\tFound RawBlock (s) at %#llx + %d bytes", getBaseAddress(), flowGraph->getBlock(0)->getBaseAddress()-getBaseAddress());
        unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(getBaseAddress()),
                                              flowGraph->getBlock(0)->getBaseAddress()-getBaseAddress(), getBaseAddress()));
    }
    for (int32_t i = 0; i < flowGraph->getNumberOfBlocks()-1; i++){
        uint64_t blockEnds = flowGraph->getBlock(i)->getBaseAddress() + flowGraph->getBlock(i)->getNumberOfBytes();        
        uint64_t blockBegins = flowGraph->getBlock(i+1)->getBaseAddress();        
        if (blockEnds < blockBegins){
            PRINT_DEBUG_CFG("\tFound RawBlock (m) at %#llx + %d bytes", blockEnds, blockBegins-blockEnds);
            unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(blockEnds),
                                                  blockBegins-blockEnds, blockEnds));            
        }
    }
    BasicBlock* lastBlock = (BasicBlock*)flowGraph->getBlock(flowGraph->getNumberOfBlocks()-1);
    uint64_t blockEnds = lastBlock->getBaseAddress() + lastBlock->getNumberOfBytes();
    uint64_t funcEnds = getBaseAddress() + getSizeInBytes();
    if (blockEnds < funcEnds){
        PRINT_DEBUG_CFG("\tFound RawBlock (e) at %#llx + %d bytes", blockEnds, funcEnds-blockEnds);
        unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(blockEnds),
                                              funcEnds-blockEnds, blockEnds));
    }
    for (uint32_t i = 0; i < unknownBlocks.size(); i++){
        DEBUG_CFG(unknownBlocks[i]->print();)
        flowGraph->addBlock(unknownBlocks[i]);
    }

    // anchor any jump table entries to instructions in this function
    uint32_t dataLen;
    if (getTextSection()->getElfFile()->is64Bit()){
        dataLen = sizeof(uint64_t);
    } else {
        dataLen = sizeof(uint32_t);
    }

    Vector<uint64_t> unqStorageAddrs = Vector<uint64_t>();
    Vector<uint64_t> unqTargetAddrs = Vector<uint64_t>();

    for (uint32_t i = 0; i < anchorAddrs.size(); i++){
        bool foundRepeat = false;
        for (uint32_t j = 0; j < unqStorageAddrs.size(); j++){
            if (unqStorageAddrs[j] == anchorAddrs[i]){
                foundRepeat = true;
            }
        }
        if (!foundRepeat){
            unqStorageAddrs.append(anchorAddrs[i]);
            unqTargetAddrs.append(anchorInstrs[i]);
        }
    }

    for (uint32_t i = 0; i < unqStorageAddrs.size(); i++){
        RawSection* dataSection = getTextSection()->getElfFile()->findDataSectionAtAddr(unqStorageAddrs[i]);
        ASSERT(dataSection);
        DataReference* dataRef = new DataReference(unqStorageAddrs[i], dataSection, dataLen, unqStorageAddrs[i] - dataSection->getSectionHeader()->GET(sh_addr));

        DEBUG_ANCHOR(dataRef->print();)

        X86Instruction* linkedInstruction = getInstructionAtAddress(unqTargetAddrs[i]);
        ASSERT(linkedInstruction);
        dataRef->initializeAnchor(linkedInstruction);
        dataSection->addDataReference(dataRef);

        (*addressAnchors).append(dataRef->getAddressAnchor());
        dataRef->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
    }

    verify();
    
}

X86Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->inRange(addr)){
            return flowGraph->getBasicBlock(i)->getInstructionAtAddress(addr); 
        }
    }

    return NULL;
}

BasicBlock* Function::getBasicBlockAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->inRange(addr)){
            return flowGraph->getBasicBlock(i); 
        }
    }

    return NULL;
}

uint64_t Function::findInstrumentationPoint(uint64_t addr, uint32_t size, InstLocations loc){
    ASSERT(inRange(addr) && "Instrumentation address should fall within Function bounds");
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->inRange(addr)){
            return flowGraph->getBasicBlock(i)->findInstrumentationPoint(addr, size, loc);
        }
    }
    return 0;
}

Function::~Function(){
    if (flowGraph){
        delete flowGraph;
    }
}


Function::Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz)
    : TextObject(PebilClassType_Function,text,idx,sym,sym->GET(st_value),sz)
{
    ASSERT(sym);

    flowGraph = NULL;
    hashCode = HashCode(text->getSectionIndex(),index);
    PRINT_DEBUG_HASHCODE("Function %d, section %d  Hashcode: 0x%08llx", index, text->getSectionIndex(), hashCode.getValue());

    badInstruction = 0;
    flags = 0;
    stackSize = 0;

    verify();
}


bool Function::verify(){
    if (symbol){
        if (!symbol->isFunctionSymbol(textSection)){
            symbol->print();
            PRINT_ERROR("The symbol given for this function does not appear to be a function symbol");
            return false;
        }
    }
    if (!hashCode.isFunction()){
        PRINT_ERROR("Function %d HashCode is malformed", index);
        return false;
    }
    if (flowGraph){
        flowGraph->verify();
        for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
            if (!flowGraph->getBasicBlock(i)->verify()){
                return false;
            }
        }
        if (getNumberOfBytes() > sizeInBytes){
            PRINT_ERROR("Function %s has more bytes in BBs (%d) than in its size (%d)", getName(), getNumberOfBytes(), sizeInBytes);
        }
        X86Instruction** allInstructions = new X86Instruction*[getNumberOfInstructions()];
        getAllInstructions(allInstructions,0);
        qsort(allInstructions,getNumberOfInstructions(),sizeof(X86Instruction*),compareBaseAddress);
        for (uint32_t i = 0; i < getNumberOfInstructions()-1; i++){
            if (allInstructions[i+1]->getBaseAddress() && allInstructions[i]->getBaseAddress() == allInstructions[i+1]->getBaseAddress()){
                allInstructions[i]->print();
                allInstructions[i+1]->print();
                PRINT_ERROR("In function %s found the same instruction twice at base address %#llx", getName(), allInstructions[i]->getBaseAddress());
                return false;
            }
        }
        delete[] allInstructions;

        uint32_t numberOfBytes = 0;
        for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
            numberOfBytes += flowGraph->getBlock(i)->getNumberOfBytes();
        }
        if (numberOfBytes != sizeInBytes){
            PRINT_ERROR("Bytes in FlowGraph doesn't match function size");
            return false;
        }
    }

    return true;
}

void Function::print(){
    PRINT_INFOR("Function %s has base address %#llx -- flags %#llx", getName(), baseAddress, flags);
    symbol->print();
}
