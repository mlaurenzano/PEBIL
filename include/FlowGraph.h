#ifndef _FlowGraph_h_
#define _FlowGraph_h_

#include <BitSet.h>
#include <Function.h>
#include <LinkedList.h>
#include <Vector.h>

class BasicBlock;
class Loop;
class TextSection;

class FlowGraph {
protected:

    Function* function;

    uint32_t numberOfBasicBlocks;
    BasicBlock** basicBlocks; /** sorted according to the base address,index is basic block id**/

    uint32_t numberOfLoops;
    Loop** loops;

public:
    FlowGraph(Function* f) : function(f),numberOfBasicBlocks(0),basicBlocks(NULL),numberOfLoops(0),loops(NULL) {}
        ~FlowGraph() {}

        TextSection* getTextSection();
        void print();
        uint32_t getIndex();
        Function* getFunction() { return function; }

        void assignSequenceNumbers();
        uint32_t initializeAllBlocks(BasicBlock** blockArr,BasicBlock* traceBlock,uint32_t arrCount);

        void findMemoryFloatOps();

        BasicBlock** getAllBlocks() { return basicBlocks; }
        uint32_t getAllBlocks(BasicBlock** arr);

        BitSet<BasicBlock*>* newBitSet();

        BasicBlock* getBlock(uint32_t idx) { return (idx < numberOfBasicBlocks ? basicBlocks[idx] : NULL); }

        uint32_t getNumberOfBasicBlocks() { return numberOfBasicBlocks; }
        uint32_t getNumberOfMemoryOps();
        uint32_t getNumberOfFloatOps();

        BasicBlock* getEntryBlock();
        Loop* getLoop(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfLoops); return loops[idx]; }
        uint32_t getNumberOfLoops() { return numberOfLoops; }
        void buildLoops();
        void printInnerLoops();

        void depthFirstSearch(BasicBlock* root,BitSet<BasicBlock*>* visitedSet,bool set,
                              BitSet<BasicBlock*>* completedSet=NULL,LinkedList<BasicBlock*>* backEdges=NULL);
        void setImmDominatorBlocks(BasicBlock* root=NULL);
};

#endif /* _FlowGraph_h_ */
