#ifndef _BasicBlockCounter_h_
#define _BasicBlockCounter_h_

#include <ElfFileInst.h>

class BasicBlockCounter : public ElfFileInst {
private:
public:
    BasicBlockCounter(ElfFile* elf);
    ~BasicBlockCounter() {}

    void instrument();
};


#endif /* _BasicBlockCounter_h_ */
