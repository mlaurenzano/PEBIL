#ifndef _FunctionCounter_h_
#define _FunctionCounter_h_

#include <InstrumentationTool.h>

class FunctionCounter : public InstrumentationTool {
private:

public:
    FunctionCounter(ElfFile* elf, char* inputFuncList);
    ~FunctionCounter();

    void declare();
    void instrument();
};


#endif /* _FunctionCounter_h_ */
