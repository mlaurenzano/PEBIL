#ifndef _FunctionCounter_h_
#define _FunctionCounter_h_

#include <InstrumentationTool.h>

class FunctionCounter : public InstrumentationTool {
private:
    InstrumentationFunction* entryFunc;
    InstrumentationFunction* exitFunc;

public:
    FunctionCounter(ElfFile* elf);
    ~FunctionCounter() {}

    void declare();
    void instrument();
};


#endif /* _FunctionCounter_h_ */
