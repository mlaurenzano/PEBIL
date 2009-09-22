#ifndef _PrintMemory_h_
#define _PrintMemory_h_

#include <InstrumentationTool.h>

class PrintMemory : public InstrumentationTool {
private:
    InstrumentationFunction* memFunc;

public:
    PrintMemory(ElfFile* elf, char* inputFuncList);
    ~PrintMemory() {}

    void declare();
    void instrument();

    const char* briefName() { return "PrintMemory"; }
};


#endif /* _PrintMemory_h_ */
