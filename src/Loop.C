#include <Loop.h>
#include <Function.h>
#include <BitSet.h>

Loop::~Loop(){
    delete blocks;
}

Loop::Loop(BasicBlock* h, BasicBlock* t, FlowGraph* cfg, BitSet<BasicBlock*>* newBlocks) { 
    index = Invalid_UInteger_ID;
    head = h; 
    tail = t;
    flowGraph = cfg; 
    blocks = cfg->newBitSet();
    blocks->clear();
    for (uint32_t i = 0; i < cfg->getNumOfBasicBlocks(); i++){
        if (newBlocks->contains(i)){
            blocks->insert(i);
        }
    }
}

void Loop::print(){
    PRINT_INFOR("Loop %u of cfg %u: Head %d (base %#llx), tail %d among %d blocks", 
                 getIndex(),flowGraph->getIndex(),
                 head->getIndex(), head->getBaseAddress(),tail->getIndex(),
                 flowGraph->getNumOfBasicBlocks());
    for (uint32_t i = 0; i < flowGraph->getNumOfBasicBlocks(); i++){
        if (blocks->contains(i)){
            PRINT_INFOR("\tMember Block %d", i);
        }
    }
}

uint32_t Loop::getAllBlocks(BasicBlock** arr){
    ASSERT(arr != NULL);
    uint32_t arrIdx = 0;
    for (uint32_t i = 0; i < flowGraph->getNumOfBasicBlocks(); i++){
    if(blocks->contains(i)){
        arr[arrIdx++] = flowGraph->getBlock(i);
    }
    }
    return blocks->size();
}

bool Loop::isInnerLoop(Loop* loop){
    for (uint32_t i = 0; i < flowGraph->getNumOfBasicBlocks(); i++){
        if (loop->isBlockIn(i) && !isBlockIn(i)){
            return false;
        }
    }
    return true;
}

bool Loop::isIdenticalLoop(Loop* loop){
    for (uint32_t i = 0; i < flowGraph->getNumOfBasicBlocks(); i++){
        if (isBlockIn(i) != loop->isBlockIn(i)){
            return false;
        }
    }
    return true;
}

