#ifndef _PrintMemory_h_
#define _PrintMemory_h_

#include <ElfFileInst.h>

class PrintMemory : public ElfFileInst {
private:
    void printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos);

    InstrumentationFunction* memFunc;
public:
    PrintMemory(ElfFile* elf, char* inputFuncList);
    ~PrintMemory() {}

    void declare();
    void instrument();

    const char* briefName() { return "PrintMemory"; }
};


#endif /* _PrintMemory_h_ */
