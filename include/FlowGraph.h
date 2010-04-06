#ifndef _FlowGraph_h_
#define _FlowGraph_h_

#include <BitSet.h>
#include <Function.h>
#include <LinkedList.h>
#include <Vector.h>

class BasicBlock;
class Block;
class Loop;
class TextSection;

class FlowGraph {
protected:

    Function* function;

    Vector<Block*> blocks; // contains both BasicBlocks and RawBlocks
    Vector<BasicBlock*> basicBlocks; // only BasicBlocks
    Vector<Loop*> loops;

    Vector<BasicBlock**> blockCopies;
public:
    FlowGraph(Function* f) : function(f) {}
    ~FlowGraph();

    void setBaseAddress(uint64_t newBaseAddress);
    void flowAnalysis();

    uint32_t getNumberOfInstructions();
    TextSection* getTextSection();
    void print();
    uint32_t getIndex();
    Function* getFunction() { return function; }
    void connectGraph(BasicBlock* entry);
    
    void assignSequenceNumbers();
    
    void findMemoryFloatOps();
    
    BitSet<BasicBlock*>* newBitSet();
    
    BasicBlock* getBasicBlock(uint32_t idx) { return basicBlocks[idx]; }
    Block* getBlock(uint32_t idx) { return blocks[idx]; }
    uint32_t getNumberOfBasicBlocks() { return basicBlocks.size(); }
    uint32_t getNumberOfBlocks() { return blocks.size(); }
    uint32_t getNumberOfBytes();
    
    uint32_t getNumberOfMemoryOps();
    uint32_t getNumberOfFloatOps();
    
    BasicBlock* getEntryBlock();
    Vector<BasicBlock*>* getExitBlocks();

    Loop* getLoop(uint32_t idx) { return loops[idx]; }
    uint32_t getNumberOfLoops() { return loops.size(); }
    uint32_t buildLoops();
    void printInnerLoops();
    void printLoops();

    void addBlock(Block* block);    
    
    BasicBlock** getAllBlocks();
    uint32_t getAllBlocks(uint32_t sz, BasicBlock** arr);
    
    void depthFirstSearch(BasicBlock* root,BitSet<BasicBlock*>* visitedSet,bool set,
                          BitSet<BasicBlock*>* completedSet=NULL,LinkedList<BasicBlock*>* backEdges=NULL);
    
    void setImmDominatorBlocks(BasicBlock* root=NULL);

    bool verify();
};

#endif /* _FlowGraph_h_ */
