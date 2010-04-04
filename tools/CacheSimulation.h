#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;

    Vector<InstrucX86*>* generateBufferedAddressCalculation32(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);
    Vector<InstrucX86*>* generateBufferedAddressCalculation64(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);
    Vector<InstrucX86*>* tmp_generateBufferedAddressCalculation64(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);

public:
    CacheSimulation(ElfFile* elf);
    ~CacheSimulation() {}

    Vector<InstrucX86*>* generateBufferedAddressCalculation(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method);

    void declare();
    void instrument();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
