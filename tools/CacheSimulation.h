#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;

    Vector<Instruction*>* generateBufferedAddressCalculation32(Instruction* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);
    Vector<Instruction*>* generateBufferedAddressCalculation64(Instruction* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);

public:
    CacheSimulation(ElfFile* elf, char* inputFuncList, char* inputFileList);
    ~CacheSimulation() {}

    Vector<Instruction*>* generateBufferedAddressCalculation(Instruction* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);

    void declare();
    void instrument();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
