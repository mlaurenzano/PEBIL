#ifndef _FunctionTimer_h_
#define _FunctionTimer_h_

#include <InstrumentationTool.h>

class FunctionTimer : public InstrumentationTool {
private:
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;
    InstrumentationFunction* functionEntry;
    InstrumentationFunction* functionExit;

public:
    FunctionTimer(ElfFile* elf);
    ~FunctionTimer() {}

    void declare();
    void instrument();
};


#endif /* _FunctionTimer_h_ */
