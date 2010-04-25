#include <FlowGraph.h>

#include <BasicBlock.h>
#include <BitSet.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <LengauerTarjan.h>
#include <LinkedList.h>
#include <Loop.h>
#include <Stack.h>
#include <set>

using namespace std;

void FlowGraph::flowAnalysis(){
    Vector<BitSet<uint32_t>*> uses;
    Vector<BitSet<uint32_t>*> defs;
    Vector<BitSet<uint32_t>*> ins;
    Vector<BitSet<uint32_t>*> outs;
    Vector<BitSet<uint32_t>*> ins_prime;
    Vector<BitSet<uint32_t>*> outs_prime;
    Vector<std::set<uint32_t>*> succs;
    Vector<X86Instruction*> allInstructions;
    uint32_t maxElts = 32;

    // reindex instructions
    uint32_t currIdx = 0;
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            instruction->setIndex(currIdx++);
        }
    }
    PRINT_DEBUG_LIVE_REGS("Flow analysis on function %s (%d instructions)", function->getName(), currIdx);

    // initialize data structures
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            uses.append(instruction->getUseRegs());
            defs.append(instruction->getDefRegs());
            ins.append(new BitSet<uint32_t>(maxElts));
            outs.append(new BitSet<uint32_t>(maxElts));
            ins_prime.append(new BitSet<uint32_t>(maxElts));
            outs_prime.append(new BitSet<uint32_t>(maxElts));

            allInstructions.append(instruction);

            succs.append(new std::set<uint32_t>());
            if (j == bb->getNumberOfInstructions() - 1){
                for (uint32_t k = 0; k < bb->getNumberOfTargets(); k++){
                    succs.back()->insert(bb->getTargetBlock(k)->getLeader()->getIndex());
                }
            } else {
                succs.back()->insert(bb->getInstruction(j+1)->getIndex());
            }
        }
    }
    ASSERT(allInstructions.size() == currIdx);
    ASSERT(uses.size() == currIdx);
    ASSERT(defs.size() == currIdx);
    ASSERT(ins.size() == currIdx);
    ASSERT(outs.size() == currIdx);
    ASSERT(ins_prime.size() == currIdx);
    ASSERT(outs_prime.size() == currIdx);
    ASSERT(succs.size() == currIdx);

    DEBUG_LIVE_REGS(
                    for (uint32_t i = 0; i < allInstructions.size(); i++){
                        allInstructions[i]->print();
                        PRINT_INFO();
                        PRINT_OUT("instruction %d succ list: ", i);
                        for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                            PRINT_OUT("%d ", (*it));
                        }
                        PRINT_OUT("\n");
                        PRINT_REG_LIST(uses, maxElts, i);
                        PRINT_REG_LIST(defs, maxElts, i);
                    }
                    )

    bool setsSame = false;
    uint32_t iterCount = 0;
    while (!setsSame){
        for (uint32_t i = 0; i < allInstructions.size(); i++){
            // ins'[n] = ins[n]
            *(ins_prime[i]) = *(ins[i]);

            // outs'[n] = outs[n]
            *(outs_prime[i]) = *(outs[i]);

            PRINT_DEBUG_LIVE_REGS("before in[n] = use[n] U (out[n] - def[n])");
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(uses, maxElts, i);
            PRINT_REG_LIST(defs, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);

            // in[n] = use[n] U (out[n] - def[n])
            BitSet<uint32_t>* tmpbt = new BitSet<uint32_t>(*(outs[i]));
            *(tmpbt) -= *(defs[i]);
            *(ins[i]) |= *(uses[i]);
            *(ins[i]) |= *(tmpbt);
            delete tmpbt;

            PRINT_DEBUG_LIVE_REGS("after in[n] = use[n] U (out[n] - def[n])");
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);

            // out[n] = U(s in succ[n]) in[s]
            //            (outs[i])->clear();
            for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                *(outs[i]) |= *(ins[(*it)]);
                PRINT_REG_LIST(ins, maxElts, (*it));
            }

            PRINT_DEBUG_LIVE_REGS("after out[n] = U(s in succ[n]) in[s]");
            PRINT_REG_LIST(outs, maxElts, i);
        }

        // check if in/out have changed this iteration for any n
        setsSame = true;
        for (uint32_t i = 0; i < allInstructions.size() && setsSame; i++){
            if (!(*(ins[i]) == *(ins_prime[i]))){
                PRINT_DEBUG_LIVE_REGS("ins %d different", i);
                setsSame = false;
            }
            if (!(*(outs[i]) == *(outs_prime[i]))){
                PRINT_DEBUG_LIVE_REGS("outs %d different", i);
                setsSame = false;
            }
        }

        for (uint32_t i = 0; i < allInstructions.size(); i++){
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(ins_prime, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            PRINT_REG_LIST(outs_prime, maxElts, i);
        }   

        iterCount++;
    }

    for (uint32_t i = 0; i < allInstructions.size(); i++){
        allInstructions[i]->setLiveIns(ins[i]);
        allInstructions[i]->setLiveOuts(outs[i]);
    }

    for (uint32_t i = 0; i < allInstructions.size(); i++){
        delete uses[i];
        delete defs[i];
        delete ins[i];
        delete outs[i];
        delete ins_prime[i];
        delete outs_prime[i];
        delete succs[i];
    }
}

bool FlowGraph::verify(){
    if (blocks.size()){
        if (blocks[0]->getBaseAddress() != function->getBaseAddress()){
            PRINT_ERROR("First block of flowGraph should begin at function start");
            return false;
        }
        if (blocks.back()->getBaseAddress()+blocks.back()->getNumberOfBytes() != function->getBaseAddress()+function->getSizeInBytes()){
            PRINT_ERROR("Flowgraph of function %s: last block of flowGraph should end (%#llx) at function end (%#llx)", 
                        function->getName(), blocks.back()->getBaseAddress()+blocks.back()->getNumberOfBytes(),
                        function->getBaseAddress()+function->getSizeInBytes());
            return false;
        }
    }
    for (int32_t i = 0; i < blocks.size()-1; i++){
        if (blocks[i]->getBaseAddress()+blocks[i]->getNumberOfBytes() != blocks[i+1]->getBaseAddress()){
            PRINT_ERROR("Blocks %d and %d in FlowGraph should be adjacent -- %#llx != %#llx", i, i+1, blocks[i]->getBaseAddress()+blocks[i]->getNumberOfBytes(), blocks[i+1]->getBaseAddress());
            return false;
        }
    }
    for (uint32_t i = 0; i < blocks.size(); i++){
        if (!blocks[i]->verify()){
            return false;
        }
    }
    return true;
}

void FlowGraph::addBlock(Block* block){
    if (block->getType() == PebilClassType_BasicBlock){
        basicBlocks.append((BasicBlock*)block);
    }
    blocks.insertSorted(block,compareBaseAddress);
}

void FlowGraph::setBaseAddress(uint64_t newBaseAddr){
    uint64_t currentOffset = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        blocks[i]->setBaseAddress(newBaseAddr+currentOffset);
        currentOffset += blocks[i]->getNumberOfBytes();
    }
}

uint32_t FlowGraph::getNumberOfBytes(){
    uint32_t numberOfBytes = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        numberOfBytes += blocks[i]->getNumberOfBytes();
    }
    return numberOfBytes;
}

uint32_t FlowGraph::getNumberOfInstructions(){
    uint32_t numberOfInstructions = 0;
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        numberOfInstructions += basicBlocks[i]->getNumberOfInstructions();
    }
    return numberOfInstructions;
}

void FlowGraph::connectGraph(BasicBlock* entry){
    ASSERT(entry);
    entry->setEntry();

    basicBlocks.sort(compareBaseAddress);

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        basicBlocks[i]->setIndex(i);
        for (uint32_t j = 0; j < basicBlocks[i]->getNumberOfInstructions(); j++){
            basicBlocks[i]->getInstruction(j)->setIndex(j);
        }
    }

    uint64_t* addressCache = new uint64_t[basicBlocks.size()];
    uint64_t* targetAddressCache = new uint64_t[basicBlocks.size()];

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        addressCache[i] = basicBlocks[i]->getBaseAddress();
        targetAddressCache[i] = basicBlocks[i]->getTargetAddress();
        PRINT_DEBUG_CFG("caching block addresses %#llx -> %#llx", addressCache[i], targetAddressCache[i]);
    }


    // detect incoming and outgoing edges
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->findExitInstruction()){
            PRINT_DEBUG_CFG("Setting block %d as exit block", i);
            basicBlocks[i]->setExit();
        }
        if (basicBlocks[i]->controlFallsThrough() && i+1 < basicBlocks.size()){
            if (targetAddressCache[i] != addressCache[i+1]){
                PRINT_DEBUG_CFG("Adding adjacent blocks to list %d -> %d", i, i+1);
                basicBlocks[i]->addTargetBlock(basicBlocks[i+1]);
                basicBlocks[i+1]->addSourceBlock(basicBlocks[i]);
            }
        }
        if (function->inRange(targetAddressCache[i])){
            for (uint32_t j = 0; j < basicBlocks.size(); j++){
                if (targetAddressCache[i] == addressCache[j]){
                    PRINT_DEBUG_CFG("Adding jump target to list %d(%llx) -> %d(%llx)", i, addressCache[i], j, targetAddressCache[i]);
                    basicBlocks[j]->addSourceBlock(basicBlocks[i]);
                    basicBlocks[i]->addTargetBlock(basicBlocks[j]);
                }
            }
        }
    }

    delete[] addressCache;
    delete[] targetAddressCache;

    
    // determine which blocks are reachable
    BitSet<BasicBlock*>* edgeSet = newBitSet();
    edgeSet->setall();
    depthFirstSearch(getEntryBlock(),edgeSet,false);

    uint32_t unreachableCount = edgeSet->size(); /* all members with their bit set are unvisited ones **/
    if(unreachableCount){
        BasicBlock** unreachableBlocks = edgeSet->duplicateMembers();
        for(uint32_t i = 0; i < unreachableCount; i++){
            unreachableBlocks[i]->setNoPath();
            PRINT_DEBUG_CFG("\tBlock %d at %#llx is unreachable",unreachableBlocks[i]->getIndex(), unreachableBlocks[i]->getBaseAddress());
        }
        delete[] unreachableBlocks;
    }
    PRINT_DEBUG_CFG("******** Found %d unreachable blocks for function %s",unreachableCount,getFunction()->getName());
    delete edgeSet;

}

void FlowGraph::printLoops(){
    if (basicBlocks.size()){
        PRINT_INFOR("Flowgraph has base address %#llx", basicBlocks[0]->getBaseAddress());
    }
    if (loops.size()){
        print();
    }
    for (uint32_t i = 0; i < loops.size(); i++){
        loops[i]->print();
    }
}

void FlowGraph::printInnerLoops(){
    for (uint32_t i = 0; i < loops.size(); i++){
        for (uint32_t j = 0; j < loops.size(); j++){
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

BasicBlock** FlowGraph::getAllBlocks(){
    BasicBlock** allBlocks = new BasicBlock*[basicBlocks.size()];
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        allBlocks[i] = basicBlocks[i];
    }
    return allBlocks;
}

uint32_t FlowGraph::buildLoops(){

    ASSERT(!loops.size());
    PRINT_DEBUG_LOOP("Considering flowgraph for function %d -- has %d blocks", function->getIndex(),  basicBlocks.size());

    BasicBlock** allBlocks = new BasicBlock*[basicBlocks.size()]; 
    getAllBlocks(basicBlocks.size(), allBlocks);

    LinkedList<BasicBlock*> backEdges;
    BitSet <BasicBlock*>* visitedBitSet = newBitSet();
    BitSet <BasicBlock*>* completedBitSet = newBitSet();

    depthFirstSearch(allBlocks[0],visitedBitSet,true,completedBitSet,&backEdges);

    delete[] allBlocks;
    delete visitedBitSet;
    delete completedBitSet;

    if(backEdges.empty()){
        PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),loops.size(),basicBlocks.size());
        return 0;
    }

    ASSERT(!(backEdges.size() % 2) && "Fatal: Back edge list should be multiple of 2, (from->to)");
    BitSet<BasicBlock*>* inLoop = newBitSet();
    Stack<BasicBlock*> loopStack(basicBlocks.size());
    LinkedList<Loop*> loopList;

    uint32_t numberOfLoops = 0;
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
                uint32_t numberOfSources = top->getNumberOfSources();
                for (uint32_t m = 0; m < numberOfSources; m++){
                    BasicBlock* pred = top->getSourceBlock(m);
                    if (!inLoop->contains(pred->getIndex())){
                        inLoop->insert(pred->getIndex());
                        loopStack.push(pred);
                    }
                }
            }

            Loop* newLoop = new Loop(to, from, this, inLoop);
            loopList.insert(newLoop);

            DEBUG_LOOP(newLoop->print();)
        }
    }

    ASSERT((loopList.size() == numberOfLoops) && 
        "Fatal: Number of loops should match backedges defining them");

    delete inLoop;

    PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,basicBlocks.size());

    if (numberOfLoops){
        uint32_t i = 0;
        while (!loopList.empty()){
            loops.append(loopList.shift());
        }
        for (i=0; i < loops.size(); i++){
            loops[i]->setIndex(i);
        }
    }
    ASSERT(loops.size() == numberOfLoops);

    DEBUG_MORE(printInnerLoops());
}

BasicBlock* FlowGraph::getEntryBlock(){
    BasicBlock* entryBlock = NULL;
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->isEntry()){
            ASSERT(!entryBlock && "There should not be multiple entry blocks to the same graph");
            entryBlock = basicBlocks[i];
        }
    }
    return entryBlock;
}

Vector<BasicBlock*>* FlowGraph::getExitBlocks(){
    Vector<BasicBlock*>* exitBlocks = new Vector<BasicBlock*>();
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->isExit()){
            (*exitBlocks).append(basicBlocks[i]);
        } else if (!getFunction()->inRange(basicBlocks[i]->getBaseAddress())){
            (*exitBlocks).append(basicBlocks[i]);
        }        
    }
    return exitBlocks;
}


TextSection* FlowGraph::getTextSection(){
    return function->getTextSection();
}

uint32_t FlowGraph::getIndex() { 
    return function->getIndex(); 
}

uint32_t FlowGraph::getAllBlocks(uint32_t sz, BasicBlock** arr){
    ASSERT(sz == basicBlocks.size());
    for (uint32_t i = 0; i < basicBlocks.size(); i++)
        arr[i] = basicBlocks[i];
    return basicBlocks.size();
}

void FlowGraph::findMemoryFloatOps(){
    if(!basicBlocks.size())
        return;

    for(uint32_t i=0;i<basicBlocks.size();i++){
        basicBlocks[i]->findMemoryFloatOps();
    }
}


void FlowGraph::print(){
    PRINT_INFOR("[G(idx %5d) (#bb %6d) (unq %#12llx)",
            getIndex(),basicBlocks.size(),function->getHashCode().getValue());

    if(!basicBlocks.size()){
        PRINT_INFOR("]");
        return;
    }

    for(uint32_t i=0;i<basicBlocks.size();i++){
        basicBlocks[i]->print();
    }

    PRINT_INFOR("]");

    for (uint32_t i = 0; i < loops.size(); i++){
        //        loops[i]->print();
    }
}

BitSet<BasicBlock*>* FlowGraph::newBitSet() { 

    BasicBlock** blocks = getAllBlocks();
    blockCopies.append(blocks);

    if(basicBlocks.size())
        return new BitSet<BasicBlock*>(basicBlocks.size(),blocks); 
    return NULL;
}

FlowGraph::~FlowGraph(){
    for (uint32_t i = 0; i < blockCopies.size(); i++){
        delete[] blockCopies[i];
    }
    for (uint32_t i = 0; i < loops.size(); i++){
        delete loops[i];
    }
    for (uint32_t i = 0; i < blocks.size(); i++){
        delete blocks[i];
    }
}

void FlowGraph::setImmDominatorBlocks(BasicBlock* root){

    if(!root){
        /** Here find the entry node to the CFG **/
        ASSERT(basicBlocks.size());
        root = basicBlocks[0];
    }
    ASSERT(root);
    ASSERT(root->isEntry() && "Fatal: The root node should be valid and entry to cfg");
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        ASSERT(basicBlocks[i]);
    }

    BasicBlock** allBlocks = getAllBlocks();
    LengauerTarjan dominatorAlg(getNumberOfBasicBlocks(),root,allBlocks);
    dominatorAlg.immediateDominators();
    delete[] allBlocks;
}

void FlowGraph::depthFirstSearch(BasicBlock* root, BitSet<BasicBlock*>* visitedSet, bool visitedMarkOnSet,
                                 BitSet<BasicBlock*>* completedSet, LinkedList<BasicBlock*>* backEdges)
{

    if(visitedMarkOnSet){
        visitedSet->insert(root->getIndex());
    } else {
        visitedSet->remove(root->getIndex());
    }

    uint32_t numberOfTargets = root->getNumberOfTargets();
    for(uint32_t i=0;i<numberOfTargets;i++){
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

uint32_t FlowGraph::getNumberOfMemoryOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<basicBlocks.size();i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfMemoryOps();
    }
    return ret;
}

uint32_t FlowGraph::getNumberOfFloatOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<basicBlocks.size();i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfFloatOps();
    }
    return ret;
}
