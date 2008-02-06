#include <SectHeader.h>
#include <Instruction.h>
#include <RawSection.h>
#include <SymbolTable.h>
#include <DemangleWrapper.h>
#include <Instruction.h>
#include <Function.h>
#include <Iterator.h>
#include <XCoffFile.h>
#include <BitSet.h>
#include <BinaryFile.h>
#include <Loop.h>
#include <Stack.h>
#include <LinkedList.h>
#include <LengauerTarjan.h>

TIMER(
    double cfg_s1=0.0;
    double cfg_s2=0.0;
    double cfg_s3=0.0;
    double cfg_s4=0.0;
    double cfg_s5=0.0;
);

void FlowGraph::printInnerLoops(){
    for (uint32_t i = 0; i < numberOfLoops; i++){
        for (uint32_t j = 0; j < numberOfLoops; j++){
            if (loops[i]->isInnerLoop(loops[j])){
                PRINT_INFOR("Loop %d is inside loop %d", j, i);
            }
            if (i == j){
                ASSERT(loops[i]->isIdenticalLoop(loops[j]));
            } else {
                ASSERT(!loops[i]->isIdenticalLoop(loops[j]));
            }
        }
    }
}

int compareLoopHeaderVaddr(const void* arg1,const void* arg2){
    Loop* lp1 = *((Loop**)arg1);
    Loop* lp2 = *((Loop**)arg2);
    uint64_t vl1 = lp1->getHead()->getBaseAddress();
    uint64_t vl2 = lp2->getHead()->getBaseAddress();

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}

void FlowGraph::buildLoops(){
    if(loops){
        return;
    }

    PRINT_DEBUG("Considering flowgraph for function %d -- has %d blocks", function->getIndex(),  numOfBasicBlocks);

    numberOfLoops = 0;
    loops = NULL;

    BasicBlock** allBlocks = getAllBlocks();

    LinkedList<BasicBlock*> backEdges;
    BitSet <BasicBlock*>* visitedBitSet = newBitSet();
    BitSet <BasicBlock*>* completedBitSet = newBitSet();

    depthFirstSearch(allBlocks[0],visitedBitSet,true,completedBitSet,&backEdges);

    delete visitedBitSet;
    delete completedBitSet;

    if(backEdges.empty()){
        PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,numOfBasicBlocks);
        return;
    }

    ASSERT(!(backEdges.size() % 2) && "Fatal: Back edge list should be multiple of 2, (from->to)");
    BitSet<BasicBlock*>* inLoop = newBitSet();
    Stack<BasicBlock*> loopStack(numOfBasicBlocks);
    LinkedList<Loop*> loopList;

    while(!backEdges.empty()){

        BasicBlock* from = backEdges.shift();
        BasicBlock* to = backEdges.shift();
        ASSERT(from && to && "Fatal: Backedge end points are invalid");

        if(from->isDominatedBy(to)){
            /* for each back edge found, perform natural loop finding algorithm 
               from pg. 604 of the Aho/Sethi/Ullman (Dragon) compiler book */

            numberOfLoops++;

            loopStack.clear();
            inLoop->clear();

            inLoop->insert(to->getIndex());
            if (!inLoop->contains(from->getIndex())){
                inLoop->insert(from->getIndex());
                loopStack.push(from);
            }
            while(!loopStack.empty()){
                BasicBlock* top = loopStack.pop();
                uint32_t numOfSources = top->getNumOfSources();
                for (uint32_t m = 0; m < numOfSources; m++){
                    BasicBlock* pred = top->getSourceBlock(m);
                    if (!inLoop->contains(pred->getIndex())){
                        inLoop->insert(pred->getIndex());
                        loopStack.push(pred);
                    }
                }
            }

            Loop* newLoop = new Loop(to, from, this, inLoop);
            loopList.insert(newLoop);

            DEBUG_MORE(
                newLoop->print()
            );
        }
    }

    ASSERT((loopList.size() == numberOfLoops) && 
        "Fatal: Number of loops should match backedges defining them");

    delete inLoop;

    PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,numOfBasicBlocks);

    if(numberOfLoops){
        loops = new Loop*[numberOfLoops];
        uint32_t i = 0;
        while(!loopList.empty()){
            loops[i++] = loopList.shift();
        }
        ASSERT(numberOfLoops == i);

        qsort(loops,numberOfLoops,sizeof(Loop*),compareLoopHeaderVaddr);
        for(i=0;i<numberOfLoops;i++){
            loops[i]->setIndex(i);
        }
    } 
    ASSERT(!loopList.size());

    DEBUG_MORE(printInnerLoops());
}

BasicBlock* FlowGraph::getEntryBlock(){
    for (uint32_t i = 0; i < numOfBasicBlocks; i++){
        if (basicBlocks[i]->isEntry()){
            return basicBlocks[i];
        }
    }
    ASSERT(0 && "No entry block found");
    return NULL;
}

MemoryOperation::MemoryOperation(Instruction insn,uint64_t addr,BasicBlock* bb,uint32_t idx)
    : instruction(insn),basicBlock(bb),index(idx)
{
    ASSERT(insn.isMemoryOperation() && 
           "FATAL : Only the memory instructions can form memory operation");
    offset = (uint32_t)(addr-basicBlock->getBaseAddress());
    hashCode = HashCode(basicBlock->getFlowGraph()->getRawSection()->getIndex(),
                       basicBlock->getFlowGraph()->getFunction()->getIndex(),
                       basicBlock->getIndex(),
                       index);
}

Operand MemoryOperation::getAddressOperand1(){
    uint32_t src1 = 0;
    if(isDForm() || isDsForm())
        src1 = instruction.getDFormSrc1();
    else if(isXForm())
        src1 = instruction.getXFormSrc1();

    if(src1)
        return Operand::GPRegisterOperands[src1];

    return Operand::IntegerOperand0;
}

Operand MemoryOperation::getAddressOperand2(){
    if(isDForm() || isDsForm()) {
        int32_t offset = instruction.getDFormImmediate();
        if(isDsForm())
            offset = instruction.getDsFormImmediate();
        return IntegerOperand(offset);
    } else if(isXForm()){
        uint32_t src2 = instruction.getXFormSrc2();
        if(instruction.isMemoryXFormButNoSrc2())
            return Operand::IntegerOperand0;
        else
            return Operand::GPRegisterOperands[src2];
    }
    return Operand::IntegerOperand0;
}
bool MemoryOperation::isDForm(){
    return instruction.isMemoryDForm();
}
bool MemoryOperation::isDsForm(){
    return instruction.isMemoryDsForm();
}
bool MemoryOperation::isXForm(){
    return instruction.isMemoryXForm();
}
void MemoryOperation::print(){
    PRINT_INFOR("\t(mem %#12llx) (idx %4d) (unq %#12llx)",
        offset+basicBlock->getBaseAddress(),index,getHashCode().getValue());
    getAddressOperand1().print();
    getAddressOperand2().print();
}

uint64_t MemoryOperation::getInsnAddr(){
    return (offset + basicBlock->getBaseAddress());
}

void BasicBlock::addEdge(BasicBlock* to)
{ 
    ASSERT(to && "FATAL : Invalid block is being added as an edge");
    if(isJumpTable() || to->isJumpTable() || isTrace() || to->isTrace()){
        PRINT_DEBUG("Inserting edge between JumpTable/TraceBack");
        return;
    }
}

AddressIterator BasicBlock::getInstructionIterator(){
    return AddressIterator::newAddressIteratorWord(baseAddress,sizeInBytes);
}

RawSection* BasicBlock::getRawSection(){
    return flowGraph->getRawSection();
}

void BasicBlock::findMemoryFloatOps(){
    if(isTrace() || isJumpTable())
        return;

    uint32_t insnCount = 0;
    numOfMemoryOps = 0;

    MemoryOperation** insnArr = new MemoryOperation*[getInstructionCount()];

    AddressIterator ait = getInstructionIterator();
    while(ait.hasMore()){
        insnCount++;
        Instruction insn = getRawSection()->readInstruction(&ait);
        if(insn.isMemoryOperation() && !insn.isUnhandledMemoryOp()){
            insnArr[numOfMemoryOps] = new MemoryOperation(insn,*ait,this,numOfMemoryOps);
            numOfMemoryOps++;
        } else if(insn.isFloatPOperation()){
            numOfFloatPOps++;
        } else if(insn.isUnhandledMemoryOp()){
            PRINT_INFOR("Unhandled memop bits of block %#llx, %dth instruction,\taddr %#llx\tvalue %x", 
                        getBaseAddress(), insnCount, *ait, insn.bits());
//          ASSERT(0 && "FATAL : This is an unhandled memory operation");
        }
        ait++;
    }

    if(numOfMemoryOps){
        memoryOps = new MemoryOperation*[numOfMemoryOps];
        for(uint32_t i=0;i<numOfMemoryOps;i++){
            memoryOps[i] = insnArr[i];
        }
    }

    delete[] insnArr;
}

void BasicBlock::print(){
    PRINT_INFOR("[B(idx %5d) (adr %#12llx,%#12llx) (sz %4d) (unq %#12llx) (dom %d) (flgs %#8x)",
                index,baseAddress,(baseAddress+sizeInBytes),sizeInBytes,
                getHashCode().getValue(),immDominatedBy ? immDominatedBy->getIndex() : -1,
                flags);

    for(uint32_t i=0;i<numOfTargets;i++){
        BasicBlock* bb = targets[i];
        PRINT_INFOR("\t(tgt %5d)",bb->getIndex());
    }

    for(uint32_t i=0;i<numOfSources;i++){
        BasicBlock* bb = sources[i];
        PRINT_INFOR("\t(src %5d)",bb->getIndex());
    }


    PRINT_INFOR("\t(inst %5d) (mops %3d) (fops %3d)",getInstructionCount(),numOfMemoryOps,numOfFloatPOps);
    for(uint32_t i=0;i<numOfMemoryOps;i++){
        memoryOps[i]->print();
    }

    AddressIterator ait = getInstructionIterator();
    while(ait.hasMore()){
        Instruction insn = getRawSection()->readInstruction(&ait);
        insn.print(*ait,getXCoffFile()->is64Bit());
        ait++;
    }
    PRINT_INFOR("]");
}

void BasicBlock::setIndex(uint32_t idx){
    index = idx;
    hashCode = HashCode(flowGraph->getRawSection()->getIndex(),
                        flowGraph->getFunction()->getIndex(),
                        index);
    
    ASSERT(hashCode.isBlock() && "FATAL : the has code for the block is not right");
}

RawSection* FlowGraph::getRawSection(){
    return function->getRawSection();
}

uint32_t FlowGraph::getIndex() { 
    return function->getIndex(); 
}

uint32_t FlowGraph::getAllBlocks(BasicBlock** arr){
    for(uint32_t i=0;i<numOfBasicBlocks;i++)
        arr[i] = basicBlocks[i];
    return numOfBasicBlocks;
}

uint32_t FlowGraph::initializeAllBlocks(BasicBlock** blockArray,BasicBlock* traceBlock,uint32_t arrCount){

    ASSERT("blockArray has to be sorted interms of block base addresses");

    uint32_t totalCount = (traceBlock ? 1 : 0);

    for(uint32_t i=0;i<arrCount;i++){
        if(blockArray[i]){
            totalCount++;
        }
    }

    if(totalCount){

        numOfBasicBlocks = totalCount;
        basicBlocks = new BasicBlock*[totalCount];

        if(traceBlock){
            totalCount--;
            traceBlock->setIndex(totalCount);
            basicBlocks[totalCount] = traceBlock;
        }

        uint32_t idx = 0;
        for(uint32_t i = 0;idx<totalCount;i++){
            BasicBlock* bb = blockArray[i];
            if(!bb)
                continue;
            bb->setIndex(idx);
            basicBlocks[idx++] = bb;
        }
    }

    return numOfBasicBlocks;
}

void FlowGraph::findMemoryFloatOps(){
    if(!numOfBasicBlocks)
        return;

    for(uint32_t i=0;i<numOfBasicBlocks;i++){
        basicBlocks[i]->findMemoryFloatOps();
    }
}


void FlowGraph::print(){
    PRINT_INFOR("[G(idx %5d) (#bb %6d) (unq %#12llx)",
            getIndex(),numOfBasicBlocks,function->getHashCode().getValue());

    if(!numOfBasicBlocks){
        PRINT_INFOR("]");
        return;
    }

    for(uint32_t i=0;i<numOfBasicBlocks;i++){
        basicBlocks[i]->print();
    }

    PRINT_INFOR("]");

    for (uint32_t i = 0; i < numberOfLoops; i++){
        loops[i]->print();
    }
}

BitSet<BasicBlock*>* FlowGraph::newBitSet() { 
    if(numOfBasicBlocks)
        return new BitSet<BasicBlock*>(numOfBasicBlocks,basicBlocks); 
    return NULL;
}

Function::Function(uint32_t idx,uint32_t symCount,Symbol** symbols,RawSection* sect)
    : rawSection(sect),index(idx),numOfAllSymbols(symCount),insnSizeInBytes(0),
      flowGraph(NULL) {

    ASSERT(numOfAllSymbols && "FATAL : there should be at least one symbol for the function");

    allSymbols = new Symbol*[numOfAllSymbols];

    for(uint32_t i=0;i<numOfAllSymbols;i++){
        allSymbols[i] = symbols[i];
    }
    for(uint32_t i=0;i<numOfAllSymbols;i++){
        ASSERT(allSymbols[0]->GET(n_value) == allSymbols[i]->GET(n_value)); 
    }

    SymbolTable* symbolTable = rawSection->getSymbolTable();
    ASSERT(symbolTable && "FATAL : Every raw section should have the same symbol table not NULL");

    baseAddress = allSymbols[0]->GET(n_value);
    sizeInBytes = (uint32_t)(symbolTable->getSymbolLength(allSymbols[0]));

    hashCode = HashCode(rawSection->getIndex(),index);
    ASSERT(hashCode.isFunction() && "FATAL : the has code for the function is not right");

}

uint32_t Function::getAllSymbolNames(char* buffer,uint32_t len,bool onlyFirst){
    uint32_t ret = 0;
    uint32_t howMany = numOfAllSymbols;
    if(onlyFirst){
        howMany = 1;
    }
    SymbolTable* symbolTable = rawSection->getSymbolTable();
    if(buffer && len){
        char* ptr = buffer;
        for(uint32_t i=0;i<howMany;i++){
            char* name = symbolTable->getSymbolName(allSymbols[i]);
            uint32_t namelen = strlen(name);

            namelen = (len < namelen ? len : namelen);
            strncpy(ptr,name,namelen);
            ptr += namelen;
            len -= namelen;
            free(name);

            if(!len){
                ptr--;
                break;
            }
            if(i != (howMany-1)){
                *ptr = ' ';
                ptr++;
                len--;
            }
        }
        *ptr = '\0';
        ret = (uint32_t)(ptr-buffer);
    }
    return ret;
}

void Function::print(){
    SymbolTable* symbolTable = rawSection->getSymbolTable();

    PRINT_INFOR("");
    PRINT_INFOR("[F(idx %5d) (bsz %#12llx,%5d) (trb %5d,%5d) (unq %#12llx)",
        index,baseAddress,sizeInBytes,insnSizeInBytes,sizeInBytes-insnSizeInBytes,
        getHashCode().getValue());

    for(uint32_t i=0;i<numOfAllSymbols;i++){
        char* name = symbolTable->getSymbolName(allSymbols[i]);
        uint64_t symSize = symbolTable->getSymbolLength(allSymbols[i]);

        DemangleWrapper wrapper;
        char* demangled = wrapper.demangle_combined(name);

        PRINT_INFOR("\t(sym %3lld,%3d,%s)",symSize,symbolTable->getStorageMapping(allSymbols[i]),demangled);

        free(name);
    }

    if(flowGraph)
        flowGraph->print();

    PRINT_INFOR("]");
    PRINT_INFOR("");
}

void Function::updateInstructionSize(){

    insnSizeInBytes = sizeInBytes;

    AddressIterator ait = getAddressIterator();

    while(ait.hasMore()){
        Instruction insn = rawSection->readInstruction(&ait);
        if(insn.isZero()){
            break;
        }
        ++ait;
    }

    if(ait.hasMore()){
        insnSizeInBytes = (uint32_t)(ait.getOffset());
    }
}

bool Function::getJumpTableInformation(uint64_t* jumpTableAddr,
                                       uint32_t* jumpTableEntryCount,
                                       AddressIterator ait)
{
    *jumpTableAddr = 0;
    *jumpTableEntryCount = 0;

    ASSERT(ait.hasMore());

    if(!ait.hasPrev()){
        return false;
    }

    Instruction jumpInst = rawSection->readInstruction(&ait);

    ait--; ait--;

    int32_t additionalOffset = 0;
    bool jumpRequiresOneMoreIndir = false;
    uint32_t tocBaseRegister = INVALID_REGISTER;

    if(ait.hasMore()){
        Instruction prevInst = rawSection->readInstruction(&ait);
        if(jumpInst.isIndirectJumpCtr()){
            if(!prevInst.isAddBeforeJump()){
                return false;
            }
        } else if(jumpInst.isIndirectJumpLnk()){
            if(!prevInst.isLoadBeforeJump()){
                return false;
            } else {
                tocBaseRegister = prevInst.getLoadBeforeJumpSrc1();
                ait--;
                if(ait.hasMore()){
                    prevInst = rawSection->readInstruction(&ait);
                    if(prevInst.definesJTBaseAddrIndir()){
                        additionalOffset = prevInst.getJTBaseAddrIndirOffset();
                        tocBaseRegister = prevInst.getJTBaseAddrIndirSrc();
                        jumpRequiresOneMoreIndir = true;
                    }
                }
            }

        }
    }

    bool isCountSet = false;
    bool isAddrSet = false;

    while(ait.hasMore()){
        Instruction insn = rawSection->readInstruction(&ait);

        if(!*jumpTableEntryCount && insn.definesJTEntryCount()){
            *jumpTableEntryCount = insn.getJTEntryCount() + 1;
            isCountSet = true;
        }
        
        if(!*jumpTableAddr && insn.definesJTBaseAddress()){
            if((tocBaseRegister == INVALID_REGISTER) ||
               (insn.getJTBaseAddrTarget() == tocBaseRegister))
            {
                int32_t TOCoffset = insn.getJTBaseOffsetTOC(); 
                *jumpTableAddr = getXCoffFile()->readTOC(TOCoffset);
                isAddrSet = true;
                if(jumpRequiresOneMoreIndir){
                    PRINT_DEBUG("JMPINDIR one more indirection common to xlC");
                    *jumpTableAddr += additionalOffset;
                }
            }
        }

        if(isAddrSet && isCountSet){
            return true;
        }

        ait--;
    }

    return false;
}

void Function::parseJumpTable(AddressIterator dait,
                              BasicBlock** allBlocks,
                              BasicBlock* currBlock,
                              LinkedList<uint32_t>* outgoingEdgeBags,
                              LinkedList<uint32_t>* incomingEdgeBags)
{
    dait--;

    Instruction insn = rawSection->readInstruction(&dait);

    uint64_t jumpTableAddr = 0;
    uint32_t jumpTableEntryCount = 0;

    if(getJumpTableInformation(&jumpTableAddr,&jumpTableEntryCount,dait)){

        PRINT_DEBUG("JUMP TABLE : %#112llx %#12llx %d",*dait,jumpTableAddr,jumpTableEntryCount);

        RawSection* jumpTableSect = getXCoffFile()->findRawSection(jumpTableAddr);

        if(jumpTableSect && 
           (jumpTableSect->IS_SECT_TYPE(TEXT) || jumpTableSect->IS_SECT_TYPE(DATA))){
            AddressIterator jumpTableIt = jumpTableSect->getAddressIterator();
            jumpTableIt.skipTo(jumpTableAddr);
            for(uint32_t i=0;i<jumpTableEntryCount;i++){
                uint64_t leaderAddr = jumpTableSect->readBytes(&jumpTableIt);
                if(insn.isIndirectJumpCtr()){
                    leaderAddr += jumpTableAddr;
                } 
                if(instructionInRange(leaderAddr)){
                    uint32_t insnIndex = getInstructionIndex(leaderAddr);
                    if(!allBlocks[insnIndex]){
                        allBlocks[insnIndex] = new BasicBlock(flowGraph,leaderAddr);
                    }
                    if(currBlock){
                        BasicBlock* toBlock = allBlocks[insnIndex];
                        outgoingEdgeBags[currBlock->getIndex()].insert(toBlock->getIndex());
                        incomingEdgeBags[toBlock->getIndex()].insert(currBlock->getIndex());
                    }

                    PRINT_DEBUG("\tLeader : %#12llx JMP",leaderAddr);
                    jumpTableIt++;
                }
            }
        }

        if(currBlock)
            return;

        if(instructionInRange(jumpTableAddr)){
            uint32_t insnIndex = getInstructionIndex(jumpTableAddr);
            BasicBlock* jumpTableBlock = allBlocks[insnIndex];
            if(!jumpTableBlock){
                jumpTableBlock = new BasicBlock(flowGraph,jumpTableAddr);
                allBlocks[insnIndex] = jumpTableBlock;
            }
            jumpTableBlock->setJumpTable();
            jumpTableBlock->setNoPath();
            jumpTableBlock->setSizeInBytes(jumpTableEntryCount*dait.unit());
        }
    }
}

FlowGraph* Function::getFlowGraph(){
    return flowGraph;
}

void Function::generateCFG(){

    TIMER(double t1 = timer());

    DEBUG_MORE(
        PRINT_INFOR("********************************\n");
        print();
    );
    
    flowGraph = new FlowGraph(this);

    uint32_t instructionCount = getInstructionCount();
    BasicBlock** allBlocks = new BasicBlock*[instructionCount];
    for(uint32_t i=0;i<instructionCount;i++){
        allBlocks[i] = NULL;
    }

    AddressIterator ait = getInstructionIterator();

    if (!ait.hasMore()){
        PRINT_INFOR("Function %d has no instructions", index);
        return;
    }
    ASSERT(ait.hasMore() && "FATAL : There is a Function with no instructions");

    allBlocks[0] = new BasicBlock(flowGraph,getBaseAddress());
    PRINT_DEBUG("\tLeader : %#12llx",getBaseAddress());

    while(ait.hasMore()){

        uint64_t insnAddr = *ait;
        Instruction insn = rawSection->readInstruction(&ait);
        uint32_t insnIndex = getInstructionIndex(insnAddr);
        BasicBlock* fromBlock = allBlocks[insnIndex];

        ASSERT(!insn.isZero() && "FATAL : Instructions should not have 0 word in them");

        if(fromBlock && fromBlock->isJumpTable()){
            ait.skip(fromBlock->getSizeInBytes());
            continue;
        }

        ait++;    

        if(insn.definesLeaders()){
            uint64_t leaderAddr = *ait;
            if(instructionInRange(leaderAddr)){
                insnIndex = getInstructionIndex(leaderAddr);
                if(!allBlocks[insnIndex]){
                    allBlocks[insnIndex] = new BasicBlock(flowGraph,leaderAddr);
                }
                PRINT_DEBUG("\tLeader : %#12llx",leaderAddr);
            }
        }

        if(insn.hasTargetAddress()){
            uint64_t leaderAddr = insn.getTargetAddress(insnAddr);
            if(instructionInRange(leaderAddr)){
                insnIndex = getInstructionIndex(leaderAddr);
                if(!allBlocks[insnIndex]){
                    allBlocks[insnIndex] = new BasicBlock(flowGraph,leaderAddr);
                }
                PRINT_DEBUG("\tLeader : %#12llx",leaderAddr);
            }
        } else if(insn.isIndirectJump()){
            uint64_t leaderAddr = *ait;
            if(instructionInRange(leaderAddr)){
                parseJumpTable(ait,allBlocks);
            }
        } else if(insn.isOtherBranch()){
            PRINT_ERROR("\tConditional Return/Call");
            PRINT_ERROR("\tConditional Indirect Call");
            PRINT_ERROR("\tConditional Indirect Branch");
            ASSERT(0 && "FATAL : There is a type of branch does is not handled");
        }
    }

    BasicBlock* traceBackBlock = NULL;
    if(getSize() != getInstructionSize()){
        traceBackBlock = new BasicBlock(flowGraph,getBaseAddress()+getInstructionSize());
        traceBackBlock->setSizeInBytes(getSize()-getInstructionSize());
        traceBackBlock->setTrace();
        traceBackBlock->setNoPath();
    }

    TIMER(double t2 = timer();cfg_s1+=(t2-t1);t1=t2);

    uint32_t blockCount = flowGraph->initializeAllBlocks(allBlocks,traceBackBlock,instructionCount);
    if(!blockCount){
        delete[] allBlocks;
        return;
    }

    LinkedList<uint32_t>* outgoingEdgeBags = new LinkedList<uint32_t>[blockCount];
    LinkedList<uint32_t>* incomingEdgeBags = new LinkedList<uint32_t>[blockCount];

    BasicBlock* currBlock = NULL;
    bool prevInsnDefinesLeader = false;

    allBlocks[0]->setEntry();
    ait.reset();
    while(ait.hasMore()){

        uint64_t insnAddr = *ait;
        Instruction insn = rawSection->readInstruction(&ait);
        uint32_t insnIndex = getInstructionIndex(insnAddr);
        BasicBlock* nextBlock = allBlocks[insnIndex];

        if(nextBlock){
            if(currBlock && !currBlock->isJumpTable()){
                currBlock->setSizeInBytes((uint32_t)(insnAddr-currBlock->getBaseAddress()));
                if(!prevInsnDefinesLeader){
                    ASSERT(!nextBlock->isJumpTable() && "FATAL : Fallbacks to jump table???");
                    BasicBlock* toBlock = nextBlock;
                    outgoingEdgeBags[currBlock->getIndex()].insert(toBlock->getIndex());
                    incomingEdgeBags[toBlock->getIndex()].insert(currBlock->getIndex());
                }
            }

            currBlock = nextBlock;

            if(currBlock->isJumpTable()){
                ait.skip(currBlock->getSizeInBytes());
                prevInsnDefinesLeader = false;
                continue;
            }
        }

        ait++;    

        prevInsnDefinesLeader = false;
        if(insn.definesLeaders()){
            prevInsnDefinesLeader = true;
        }

        if(insn.isCondBranch() || insn.isCondReturn() || insn.isCall()){
            uint64_t leaderAddr = *ait;
            if(instructionInRange(leaderAddr)){
                insnIndex = getInstructionIndex(leaderAddr);
                BasicBlock* toBlock = allBlocks[insnIndex];
                outgoingEdgeBags[currBlock->getIndex()].insert(toBlock->getIndex());
                incomingEdgeBags[toBlock->getIndex()].insert(currBlock->getIndex());
            } else {
                currBlock->setExit();
            }
        }

        if(insn.hasTargetAddress()){
            uint64_t leaderAddr = insn.getTargetAddress(insnAddr);
            if(instructionInRange(leaderAddr)){
                insnIndex = getInstructionIndex(leaderAddr);
                BasicBlock* toBlock = allBlocks[insnIndex];
                outgoingEdgeBags[currBlock->getIndex()].insert(toBlock->getIndex());
                incomingEdgeBags[toBlock->getIndex()].insert(currBlock->getIndex());
            } else {
                currBlock->setExit();
            }
        } else if(insn.isIndirectJump()){
            uint64_t leaderAddr = *ait;
            if(instructionInRange(leaderAddr)){
                parseJumpTable(ait,allBlocks,currBlock,outgoingEdgeBags,incomingEdgeBags);
            } else {
                currBlock->setExit();
            }
        } 

        if(insn.isReturn()){
            currBlock->setExit();
        }
    }

    TIMER(t2 = timer();cfg_s2+=(t2-t1);t1=t2);


    if(currBlock){
        currBlock->setSizeInBytes((uint32_t)((*ait)-currBlock->getBaseAddress()));
    }

    PRINT_DEBUG("*** Found %d blocks for function %d\n",blockCount,getIndex());

    BitSet<BasicBlock*>* edgeSet = flowGraph->newBitSet();

    for(uint32_t i=0;i<blockCount;i++){
        BasicBlock* fromBlock = flowGraph->getBlock(i);
        LinkedList<uint32_t>* outgoingEdges = &(outgoingEdgeBags[fromBlock->getIndex()]);
        LinkedList<uint32_t>* incomingEdges = &(incomingEdgeBags[fromBlock->getIndex()]);

        ASSERT(fromBlock && "FATAL : Basic block and its outgoing edges can not be NULL");

        edgeSet->clear();
        while(!outgoingEdges->empty()){
            uint32_t bbidx = outgoingEdges->shift();
            edgeSet->insert(bbidx);
        }
        uint32_t numOfEdges = edgeSet->size();
        if(numOfEdges){
            fromBlock->setTargets(numOfEdges,edgeSet->duplicateMembers());
        }


        edgeSet->clear();
        while(!incomingEdges->empty()){
            uint32_t bbidx = incomingEdges->shift();
            edgeSet->insert(bbidx);
        }
        numOfEdges = edgeSet->size();
        if(numOfEdges){
            fromBlock->setSources(numOfEdges,edgeSet->duplicateMembers());
        }
    }
    delete[] outgoingEdgeBags;
    delete[] incomingEdgeBags;

    TIMER(t2 = timer();cfg_s3+=(t2-t1);t1=t2);


    PRINT_DEBUG("Finding unreachable blocks in %d block for %d\n",blockCount,getIndex());

    BasicBlock* root = allBlocks[0];
    edgeSet->setall(); /* 1 indicates not visited yet */

    flowGraph->depthFirstSearch(root,edgeSet,false);

    uint32_t unreachableCount = edgeSet->size(); /* all members with their bit set are unvisited ones **/
    if(unreachableCount){
        BasicBlock** unreachableBlocks = edgeSet->duplicateMembers();
        for(uint32_t i=0;i<unreachableCount;i++){
            unreachableBlocks[i]->setNoPath();
            PRINT_DEBUG("\tBlock %d is unreachable\n",unreachableBlocks[i]->getIndex());
        }
        delete[] unreachableBlocks;
    }
    PRINT_DEBUG("******** Found %d unreachable blocks for function %d with %d blocks\n",unreachableCount,getIndex(),blockCount);

    delete edgeSet;

    delete[] allBlocks;

    TIMER(t2 = timer();cfg_s4+=(t2-t1);t1=t2);


    flowGraph->setImmDominatorBlocks(root);

    DEBUG_MORE(
        print();
        PRINT_INFOR("********************************\n");
    );

    TIMER(t2 = timer();cfg_s5=(t2-t1);t1=t2);
}

void FlowGraph::setImmDominatorBlocks(BasicBlock* root){

    if(!root){
        /** Here find the entry node to the CFG **/
    }
    ASSERT(root && root->isEntry() && "Fatal: The root node should be valid and entry to cfg");

    LengauerTarjan dominatorAlg(getNumOfBasicBlocks(),root,getAllBlocks());
    dominatorAlg.immediateDominators();
}

void FlowGraph::depthFirstSearch(BasicBlock* root,BitSet<BasicBlock*>* visitedSet,bool visitedMarkOnSet,
                                 BitSet<BasicBlock*>* completedSet,LinkedList<BasicBlock*>* backEdges)
{

    if(visitedMarkOnSet){
        visitedSet->insert(root->getIndex());
    } else {
        visitedSet->remove(root->getIndex());
    }

    uint32_t numOfTargets = root->getNumOfTargets();
    for(uint32_t i=0;i<numOfTargets;i++){
        BasicBlock* target = root->getTargetBlock(i);

        if(visitedMarkOnSet != visitedSet->contains(target->getIndex())){
            depthFirstSearch(target,visitedSet,visitedMarkOnSet,completedSet,backEdges);
        } else if(backEdges && completedSet && 
                  (visitedMarkOnSet != completedSet->contains(target->getIndex()))) 
        {
            backEdges->insert(target);
            backEdges->insert(root);
        }
    }

    if(completedSet){
        if(visitedMarkOnSet){
            completedSet->insert(root->getIndex());
        } else {
            completedSet->remove(root->getIndex());
        }
    }
}

void Function::findMemoryFloatOps(){
    if(flowGraph){
        flowGraph->findMemoryFloatOps();
    }
}

AddressIterator Function::getAddressIterator() {
    return AddressIterator::newAddressIteratorWord(baseAddress,sizeInBytes);
}

AddressIterator Function::getInstructionIterator() {
    return AddressIterator::newAddressIteratorWord(baseAddress,insnSizeInBytes);
}

XCoffFile* Function::getXCoffFile(){
    return rawSection->getXCoffFile();
}

bool Function::isAnySymbolA(char* name){
    if(!name)
        return false;

    SymbolTable* symbolTable = rawSection->getSymbolTable();
    for(uint32_t i=0;i<numOfAllSymbols;i++){
        char* symName = symbolTable->getSymbolName(allSymbols[i]);
        if(!strcmp(symName,name)){
            free(symName);
            return true;
        }
        free(symName);
    }
    return false;
}

XCoffFile* FlowGraph::getXCoffFile(){
    return function->getXCoffFile();
}

XCoffFile* BasicBlock::getXCoffFile(){
    return flowGraph->getXCoffFile();
}

uint32_t FlowGraph::getNumOfMemoryOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfBasicBlocks;i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumOfMemoryOps();
    }
    return ret;
}

uint32_t FlowGraph::getNumOfFloatPOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfBasicBlocks;i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumOfFloatPOps();
    }
    return ret;
}

bool BasicBlock::findFirstInstPoint(uint64_t* addr){ 
    *addr = 0x0;
    AddressIterator ait = getInstructionIterator();
    while(ait.hasMore()){
        Instruction insn = getRawSection()->readInstruction(&ait);
        if(!insn.definesLeaders()){
            *addr = *ait; 
            return true;
        }
        ait++;
    }
    return false; 
}

bool BasicBlock::isDominatedBy(BasicBlock* bb){
    if(!bb){
        return false;
    }
    BasicBlock* candidate = (BasicBlock*)this;
    while(candidate){
        if(candidate == bb){
            return true;
        }
        candidate = candidate->getImmDominator();
    }
    return false;
}
