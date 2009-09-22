#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;

public:
    CacheSimulation(ElfFile* elf, char* inputFuncList);
    ~CacheSimulation() {}

    void declare();
    void instrument();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
