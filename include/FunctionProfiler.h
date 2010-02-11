#ifndef _FunctionProfiler_h_
#define _FunctionProfiler_h_

#include <InstrumentationTool.h>

class FunctionProfiler : public InstrumentationTool {
private:
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;
    InstrumentationFunction* functionEntry;
    InstrumentationFunction* functionExit;

public:
    FunctionProfiler(ElfFile* elf, char* inputFuncList);
    ~FunctionProfiler();

    void declare();
    void instrument();
};


#endif /* _FunctionProfiler_h_ */
