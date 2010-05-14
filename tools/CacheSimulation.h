#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;
    InstrumentationFunction* entryFunc;

    Vector<HashCode*> blocksToInst;
    Vector<InstrumentationPoint*> memInstPoints;
    Vector<uint32_t> memInstBlockIds;
    uint64_t instPointInfo;

    uint32_t phaseNo;
    char* extension;
    bool loopIncl;

public:
    CacheSimulation(ElfFile* elf, char* inputName, char* ext, uint32_t phase, bool lp);
    ~CacheSimulation();

    void declare();
    void instrument();
    void usesModifiedProgram();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
