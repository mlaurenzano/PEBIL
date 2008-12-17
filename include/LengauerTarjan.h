#ifndef _LengauerTarjan_h_
#define _LengauerTarjan_h_

#include <LinkedList.h>

class BasicBlock;

class LengauerTarjan {
private:
    uint32_t              reachableCount;
    uint32_t              nodeCount;
    uint32_t              rootLoc;

    uint32_t              *dom;
    uint32_t              *parent; 
    uint32_t              *ancestor;
    uint32_t              *child;
    uint32_t              *vertex;
    uint32_t              *label;
    uint32_t              *semi;
    uint32_t              *size;

    LinkedList<uint32_t>* bucket;

    BasicBlock**     locToBasicBlock;
    uint32_t*        basicBlockToLoc;

    void       depthFirstSearch(uint32_t vertexV,uint32_t* dfsNo);
    void       COMPRESS(uint32_t vertexV);
    uint32_t   EVAL(uint32_t vertexV);
    void       LINK(uint32_t vertexV,uint32_t vertexW);

public:

    LengauerTarjan(uint32_t blockCount, BasicBlock* root, BasicBlock** blocks); 
    ~LengauerTarjan();
    void immediateDominators();
};

#endif
