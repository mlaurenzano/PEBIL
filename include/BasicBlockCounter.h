#ifndef _BasicBlockCounter_h_
#define _BasicBlockCounter_h_

#include <ElfFileInst.h>

class BasicBlockCounter : public ElfFileInst {
private:
    void printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos);
public:
    BasicBlockCounter(ElfFile* elf);
    ~BasicBlockCounter() {}

    void instrument();

    const char* briefName() { return "BasicBlockCounter"; }
};


#endif /* _BasicBlockCounter_h_ */
