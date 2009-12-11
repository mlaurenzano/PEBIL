#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;

    Vector<Instruction*>* generateBufferedAddressCalculation32(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize);
    Vector<Instruction*>* generateBufferedAddressCalculation64(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize);

public:
    CacheSimulation(ElfFile* elf, char* inputFuncList);
    ~CacheSimulation() {}

    Vector<Instruction*>* generateBufferedAddressCalculation(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize);

    void declare();
    void instrument();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
