#ifndef _BasicBlockCounter_h_
#define _BasicBlockCounter_h_

#include <ElfFileInst.h>

class BasicBlockCounter : public ElfFileInst {
private:
    char* fileSeperator;
public:
    BasicBlockCounter(ElfFile* elf);
    ~BasicBlockCounter();

    void declareInstrumentation();
    void reserveInstrumentation();
};


#endif /* _BasicBlockCounter_h_ */
