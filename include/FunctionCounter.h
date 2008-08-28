#ifndef _FunctionCounter_h_
#define _FunctionCounter_h_

#include <ElfFileInst.h>

class FunctionCounter : public ElfFileInst {
private:

public:
    FunctionCounter(ElfFile* elf);
    ~FunctionCounter();

    void declareInstrumentation();
    void reserveInstrumentation();
};


#endif /* _FunctionCounter_h_ */
