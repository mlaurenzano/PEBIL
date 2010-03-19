#ifndef _BasicBlockCounter_h_
#define _BasicBlockCounter_h_

#include <InstrumentationTool.h>

class BasicBlockCounter : public InstrumentationTool {
private:
    InstrumentationFunction* entryFunc;
    InstrumentationFunction* exitFunc;
public:
    BasicBlockCounter(ElfFile* elf);
    ~BasicBlockCounter() {}

    void declare();
    void instrument();

    const char* briefName() { return "BasicBlockCounter"; }
};


#endif /* _BasicBlockCounter_h_ */
