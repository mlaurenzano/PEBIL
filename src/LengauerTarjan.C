#include <LengauerTarjan.h>

#include <BasicBlock.h>
#include <BinaryFile.h>
#include <BitSet.h>
#include <Function.h>
#include <X86Instruction.h>
#include <LinkedList.h>
#include <Loop.h>
#include <RawSection.h>
#include <SectionHeader.h>
#include <Stack.h>
#include <SymbolTable.h>

void LengauerTarjan::depthFirstSearch(uint32_t vertexV,uint32_t* dfsNo){

    semi[vertexV]     = ++(*dfsNo);
    vertex[*dfsNo]    = vertexV;

    label[vertexV]    = vertexV;
    ancestor[vertexV] = 0;
    child[vertexV]    = 0;
    size[vertexV]     = 1;

    BasicBlock* bb = locToBasicBlock[vertexV];

    PRINT_DEBUG_CFG("Mark %d with %d (vertex[%d] = %d)",bb->getIndex(),*dfsNo,*dfsNo,vertexV);

    uint32_t numberOfTargets = bb->getNumberOfTargets();
    for(uint32_t i=0;i<numberOfTargets;i++){
        BasicBlock* target = bb->getTargetBlock(i);
        uint32_t vertexW = basicBlockToLoc[target->getIndex()];
        if(semi[vertexW] == 0){
            parent[vertexW] = vertexV;
            depthFirstSearch(vertexW,dfsNo);
        }
    }
    ASSERT(!bb->isUnreachable());

#if 0
    LinkedList<uint32_t> searchStack; 
    searchStack.insert(vertexV);

    while(!searchStack.empty()){

        vertexV           = searchStack.shift();

        if(semi[vertexV] != 0){
            continue;
        }

        semi[vertexV]     = ++(*dfsNo);
        vertex[*dfsNo]    = vertexV;

        label[vertexV]    = vertexV;
        ancestor[vertexV] = 0;
        child[vertexV]    = 0;
        size[vertexV]     = 1;

        BasicBlock* bb = locToBasicBlock[vertexV];
        ASSERT(!bb->isUnreachable());
        uint32_t numberOfTargets = bb->getNumberOfTargets();
//        for(int32_t i=numberOfTargets-1;i>=0;i--){
        for(uint32_t i=0;i<numberOfTargets;i++){
            BasicBlock* target = bb->getTargetBlock(i);
            uint32_t vertexW = basicBlockToLoc[target->getIndex()];
            if(semi[vertexW] == 0){
                parent[vertexW] = vertexV;
                searchStack.insert(vertexW);
            }
        }
        PRINT_DEBUG("Mark %d with %d\n",bb->getIndex(),*dfsNo);
        ASSERT(!bb->isUnreachable());
    }
#endif
}

void LengauerTarjan::COMPRESS(uint32_t vertexV){
    if(ancestor[ancestor[vertexV]] != 0){
        COMPRESS(ancestor[vertexV]);
        if(semi[label[ancestor[vertexV]]] < semi[label[vertexV]]){
            label[vertexV] = label[ancestor[vertexV]];
        }
        ancestor[vertexV] = ancestor[ancestor[vertexV]];
    }
}

uint32_t LengauerTarjan::EVAL(uint32_t vertexV){
    if(ancestor[vertexV] == 0){
        return label[vertexV];
    }
    COMPRESS(vertexV);
    if(semi[label[ancestor[vertexV]]] >= semi[label[vertexV]]){
        return label[vertexV];
    }
    return label[ancestor[vertexV]];
}

void LengauerTarjan::LINK(uint32_t vertexV,uint32_t vertexW){
    uint32_t vertexS = vertexW;
    while(semi[label[vertexW]] < semi[label[child[vertexS]]]){
        if((size[vertexS]+size[child[child[vertexS]]]) >= (2*size[child[vertexS]])){
            ancestor[child[vertexS]] = vertexS;
            child[vertexS] = child[child[vertexS]];
        }
        else{
            size[child[vertexS]] = size[vertexS];
            ancestor[vertexS] = child[vertexS];
            vertexS = child[vertexS];
        }
    }
    label[vertexS] = label[vertexW];
    size[vertexV] += size[vertexW];
    if(size[vertexV] < (2*size[vertexW])){
        uint32_t vertexT = vertexS;
        vertexS = child[vertexV];
        child[vertexV] = vertexT;
    }
    while(vertexS != 0){
        ancestor[vertexS] = vertexV;
        vertexS = child[vertexS];
    }
}

LengauerTarjan::LengauerTarjan(uint32_t blockCount, BasicBlock* root, BasicBlock** blocks) 
{

    reachableCount = 0;
    nodeCount = blockCount;
    locToBasicBlock    = new BasicBlock*[blockCount+1];
    basicBlockToLoc    = new uint32_t[blockCount+1];

    locToBasicBlock[0] = NULL;
    basicBlockToLoc[blockCount] = 0;

    uint32_t lastUnreachIdx = blockCount;

    for (uint32_t i = 0; i < blockCount; i++){
        ASSERT(blocks[i]);
        if (blocks[i]->isUnreachable()){
            locToBasicBlock[lastUnreachIdx] = blocks[i];
            basicBlockToLoc[blocks[i]->getIndex()] = lastUnreachIdx;
            lastUnreachIdx--;
            PRINT_DEBUG_CFG("not reachable %d %d", i, blocks[i]->getIndex());
        } else {
            reachableCount++;
            locToBasicBlock[reachableCount] = blocks[i];
            basicBlockToLoc[blocks[i]->getIndex()] = reachableCount;
            PRINT_DEBUG_CFG("reachable %d %d", i, blocks[i]->getIndex());
        }
    }

    PRINT_DEBUG_CFG("There are %d reachable blocks here", reachableCount);

    ASSERT((lastUnreachIdx == reachableCount) && 
        "Fatal: when reachability is divided they should end up same index");

    dom        = new uint32_t[reachableCount+1];

    parent     = new uint32_t[reachableCount+1];
    ancestor   = new uint32_t[reachableCount+1];
    child      = new uint32_t[reachableCount+1];
    vertex     = new uint32_t[reachableCount+1];
    label      = new uint32_t[reachableCount+1];
    semi       = new uint32_t[reachableCount+1];
    size       = new uint32_t[reachableCount+1];

    bucket     = new LinkedList<uint32_t>[reachableCount+1];

    for(uint32_t i=0;i<=reachableCount;i++){
        semi[i] = 0;    
        vertex[i] = 0;
    }
    rootLoc = basicBlockToLoc[root->getIndex()];
}

LengauerTarjan::~LengauerTarjan(){
    delete[] locToBasicBlock;
    delete[] basicBlockToLoc;
    delete[] parent;
    delete[] ancestor;
    delete[] child;
    delete[] vertex;
    delete[] label;
    delete[] semi;
    delete[] size;
    delete[] dom;
    delete[] bucket;
}

void LengauerTarjan::immediateDominators(){

    uint32_t dfsNo = 0;
    depthFirstSearch(rootLoc,&dfsNo);

    size[0]  = 0;
    label[0] = 0;
    semi[0]  = 0;

    for(int32_t i=reachableCount;i>1;i--){
        uint32_t vertexW =  vertex[i];

        BasicBlock* bb = locToBasicBlock[vertexW];
        ASSERT(bb && "Basic Block should be initialized");
        uint32_t numberOfSources = bb->getNumberOfSources();

        for(uint32_t j=0;j<numberOfSources;j++){
            BasicBlock* source = bb->getSourceBlock(j);
            if(!source->isUnreachable()){
                uint32_t vertexV = basicBlockToLoc[source->getIndex()];
                uint32_t vertexU = EVAL(vertexV);
                if(semi[vertexU] < semi[vertexW]){
                    semi[vertexW] = semi[vertexU];
                }
            }
        }

        bucket[vertex[semi[vertexW]]].insert(vertexW);

        LINK(parent[vertexW],vertexW);

        LinkedList<uint32_t>* bs = &(bucket[parent[vertexW]]);
        while(!bs->empty()){
            uint32_t vertexV = bs->shift();
            uint32_t vertexU = EVAL(vertexV);
            dom[vertexV] = ( semi[vertexU] < semi[vertexV] ) ? vertexU : parent[vertexW];
        }
    }
    for(uint32_t i=2;i<=reachableCount;i++){
        uint32_t vertexW = vertex[i];
        if(dom[vertexW] != vertex[semi[vertexW]]){
            dom[vertexW] = dom[dom[vertexW]];
        }
    }

    dom[rootLoc] = 0;
    PRINT_DEBUG("ROOT is %d",rootLoc);

    for(uint32_t i=1;i<=reachableCount;i++){
        uint32_t vertexW = vertex[i];
        BasicBlock* bb = locToBasicBlock[vertexW];
        ASSERT(!bb->isUnreachable());
        BasicBlock* immDom = locToBasicBlock[dom[vertexW]];
        if(immDom){
            PRINT_DEBUG("Reachable : Immediate Dominator of %d is %d",bb->getIndex(),immDom->getIndex());
            bb->setImmDominator(immDom);
        } else {
            PRINT_DEBUG("Reachable : Immediate Dominator of %d is Entry",bb->getIndex());
        }
    }
    for(int32_t i=nodeCount;i>reachableCount;i--){
        BasicBlock* bb = locToBasicBlock[i];
        ASSERT(bb->isUnreachable());
        BasicBlock* immDom = locToBasicBlock[rootLoc];
        PRINT_DEBUG("Un-Reachable : Immediate Dominator of %d is %d",bb->getIndex(),immDom->getIndex());
        bb->setImmDominator(immDom);
    }
}
