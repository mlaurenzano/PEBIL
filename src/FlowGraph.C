#include <FlowGraph.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Loop.h>
#include <LinkedList.h>
#include <LengauerTarjan.h>
#include <Stack.h>

void FlowGraph::testGraphAvailability(){

    BitSet<BasicBlock*>* badBlocks = newBitSet();

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (!basicBlocks[i]->findInstrumentationPoint() && !basicBlocks[i]->containsOnlyControl()){
            for (uint32_t j = 0; j < basicBlocks[i]->getNumberOfSources(); j++){
                if (!basicBlocks[i]->getSourceBlock(j)->findInstrumentationPoint()){
                    badBlocks->insert(i);
                }
            }
        }
    }

    PRINT_INFOR("Graph for function %s has %d/%d bad blocks", function->getName(), badBlocks->size(), basicBlocks.size());
    BasicBlock** allBadBlocks = badBlocks->duplicateMembers();
    for (uint32_t i = 0; i < badBlocks->size(); i++){
        PRINT_INFOR("block %d @ %llx", allBadBlocks[i]->getIndex(), allBadBlocks[i]->getAddress());
        allBadBlocks[i]->print();
        allBadBlocks[i]->printSourceBlocks();
    }

    delete badBlocks;
}

void FlowGraph::connectGraph(BasicBlock* entry){
    ASSERT(entry);
    entry->setEntry();

    uint64_t* addressCache = new uint64_t[basicBlocks.size()];
    uint64_t* targetAddressCache = new uint64_t[basicBlocks.size()];

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        addressCache[i] = basicBlocks[i]->getAddress();
        targetAddressCache[i] = basicBlocks[i]->getTargetAddress();
    }


    // detect incoming and outgoing edges
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->findExitInstruction()){
            PRINT_DEBUG_CFG("Setting block %d as exit block", i);
            basicBlocks[i]->setExit();
        }
        if (basicBlocks[i]->passesControlToNext() && i+1 < basicBlocks.size()){
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
            PRINT_DEBUG_CFG("\tBlock %d is unreachable",unreachableBlocks[i]->getIndex());
        }
        delete[] unreachableBlocks;
    }
    PRINT_DEBUG_CFG("******** Found %d unreachable blocks for function %s",unreachableCount,getFunction()->getName());
    delete edgeSet;

#ifdef DEBUG_CFG
    testGraphAvailability();
#endif
}

void FlowGraph::printLoops(){
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
    uint64_t vl1 = lp1->getHead()->getAddress();
    uint64_t vl2 = lp2->getHead()->getAddress();

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

#ifdef DEBUG_LOOP
            newLoop->print();
#endif
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
    ASSERT(entryBlock && "No entry block found");
    return entryBlock;
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
}

void FlowGraph::setImmDominatorBlocks(BasicBlock* root){

    if(!root){
        /** Here find the entry node to the CFG **/
        ASSERT(basicBlocks.size());
        root = basicBlocks[0];
    }
    ASSERT(root && root->isEntry() && "Fatal: The root node should be valid and entry to cfg");
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
