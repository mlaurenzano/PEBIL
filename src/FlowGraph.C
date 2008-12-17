#include <FlowGraph.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Loop.h>
#include <LinkedList.h>
#include <LengauerTarjan.h>
#include <Stack.h>

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
    uint64_t vl1 = lp1->getHead()->getAddress();
    uint64_t vl2 = lp2->getHead()->getAddress();

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

    PRINT_DEBUG("Considering flowgraph for function %d -- has %d blocks", function->getIndex(),  numberOfBasicBlocks);

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
        PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,numberOfBasicBlocks);
        return;
    }

    ASSERT(!(backEdges.size() % 2) && "Fatal: Back edge list should be multiple of 2, (from->to)");
    BitSet<BasicBlock*>* inLoop = newBitSet();
    Stack<BasicBlock*> loopStack(numberOfBasicBlocks);
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

            DEBUG_MORE(
                newLoop->print()
            );
        }
    }

    ASSERT((loopList.size() == numberOfLoops) && 
        "Fatal: Number of loops should match backedges defining them");

    delete inLoop;

    PRINT_DEBUG("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,numberOfBasicBlocks);

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
    for (uint32_t i = 0; i < numberOfBasicBlocks; i++){
        if (basicBlocks[i]->isEntry()){
            return basicBlocks[i];
        }
    }
    ASSERT(0 && "No entry block found");
    return NULL;
}


TextSection* FlowGraph::getTextSection(){
    return function->getTextSection();
}

uint32_t FlowGraph::getIndex() { 
    return function->getIndex(); 
}

uint32_t FlowGraph::getAllBlocks(BasicBlock** arr){
    for(uint32_t i=0;i<numberOfBasicBlocks;i++)
        arr[i] = basicBlocks[i];
    return numberOfBasicBlocks;
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

        numberOfBasicBlocks = totalCount;
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

    return numberOfBasicBlocks;
}

void FlowGraph::findMemoryFloatOps(){
    if(!numberOfBasicBlocks)
        return;

    for(uint32_t i=0;i<numberOfBasicBlocks;i++){
        basicBlocks[i]->findMemoryFloatOps();
    }
}


void FlowGraph::print(){
    PRINT_INFOR("[G(idx %5d) (#bb %6d) (unq %#12llx)",
            getIndex(),numberOfBasicBlocks,function->getHashCode().getValue());

    if(!numberOfBasicBlocks){
        PRINT_INFOR("]");
        return;
    }

    for(uint32_t i=0;i<numberOfBasicBlocks;i++){
        basicBlocks[i]->print();
    }

    PRINT_INFOR("]");

    for (uint32_t i = 0; i < numberOfLoops; i++){
        loops[i]->print();
    }
}

BitSet<BasicBlock*>* FlowGraph::newBitSet() { 
    if(numberOfBasicBlocks)
        return new BitSet<BasicBlock*>(numberOfBasicBlocks,basicBlocks); 
    return NULL;
}

void FlowGraph::setImmDominatorBlocks(BasicBlock* root){

    if(!root){
        /** Here find the entry node to the CFG **/
    }
    ASSERT(root && root->isEntry() && "Fatal: The root node should be valid and entry to cfg");

    LengauerTarjan dominatorAlg(getNumberOfBasicBlocks(),root,getAllBlocks());
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
    for(uint32_t i=0;i<numberOfBasicBlocks;i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfMemoryOps();
    }
    return ret;
}

uint32_t FlowGraph::getNumberOfFloatOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<numberOfBasicBlocks;i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfFloatOps();
    }
    return ret;
}
