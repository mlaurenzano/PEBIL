#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>
#include <SimpleHash.h>
#include <DFPattern.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;
    InstrumentationFunction* entryFunc;

    char* bbFile;
    char* dfPatternFile;

    SimpleHash<BasicBlock*> blocksToInst;
    SimpleHash<DFPatternType> dfpSet;

    Vector<DFPatternSpec> dfpBlocks;
    uint32_t dfpTaggedBlockCnt;

    Vector<InstrumentationPoint*> memInstPoints;
    Vector<uint32_t> memInstBlockIds;
    uint64_t instPointInfo;

    void filterBBs();

public:
    CacheSimulation(ElfFile* elf, char* inputName, char* ext, uint32_t phase, bool lpi, bool dtl, char* dfpFile);
    ~CacheSimulation();

    void declare();
    void instrument();
    void usesModifiedProgram();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
