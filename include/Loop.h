#ifndef _Loop_h_
#define _Loop_h_

#include <Base.h>
#include <BitSet.h>
#include <Function.h>

class Loop : public Base {
protected:
    uint32_t index;
    FlowGraph* flowGraph;
    BitSet<BasicBlock*>* blocks;
    BasicBlock* head;
    BasicBlock* tail;
public:
    Loop(BasicBlock* h, BasicBlock* t, FlowGraph* cfg, BitSet<BasicBlock*>* newBlocks);
    ~Loop();
    BasicBlock* getHead() { return head; }
    BasicBlock* getTail() { return tail; }
    uint32_t getNumberOfBlocks() { return blocks->size(); }
    uint32_t getAllBlocks(BasicBlock** arr);
    bool isBlockIn(uint32_t idx) { return blocks->contains(idx); }
    bool isInnerLoop(Loop* loop);
    bool isIdenticalLoop(Loop* loop);
    void print();
    void setIndex(uint32_t idx) { index = idx; }
    uint32_t getIndex() { return index; }
};

#endif // _Loop_h_

