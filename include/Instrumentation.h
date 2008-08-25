#ifndef _Instrumentation_h_
#define _Instrumentation_h_

#include <Base.h>
#include <Instruction.h>
#include <CStructuresX86.h>
#include <TextSection.h>

#define PLT_INSTRUCTION_COUNT_32BIT 3
#define PLT_INSTRUCTION_COUNT_64BIT 3
#define BOOTSTRAP_INSTRUCTION_COUNT_32BIT 2
#define BOOTSTRAP_INSTRUCTION_COUNT_64BIT 7
#define WRAPPER_INSTRUCTION_COUNT_32BIT (2*X86_32BIT_GPRS + 2 + 2)
#define WRAPPER_INSTRUCTION_COUNT_64BIT (2*X86_64BIT_GPRS + 2 + 2)

#define PLT_RETURN_OFFSET_32BIT 6
#define PLT_RETURN_OFFSET_64BIT 6


class Instrumentation : public Base {
protected:
    uint32_t index;
public:
    Instrumentation(ElfClassTypes typ, uint32_t idx) : Base(typ),index(idx) {}
    ~Instrumentation() {}

    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t sizeNeeded() { __SHOULD_NOT_ARRIVE; }
    uint32_t getIndex() { return index; }
    virtual uint64_t getEntryPoint() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { return true; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; } 
};

class InstrumentationSnippet : public Instrumentation {
private:
    uint32_t numberOfInstructions;
    Instruction** instructions;

    uint64_t codeOffset;
public:
    InstrumentationSnippet(uint32_t idx);
    ~InstrumentationSnippet();    

    void print();
    uint32_t sizeNeeded();
    void setCodeOffsets(uint64_t off) { codeOffset = off; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t getNumberOfInstructions() { return numberOfInstructions; }
    Instruction* getInstruction(uint32_t idx) { ASSERT(idx < numberOfInstructions && "Index out of bounds"); return instructions[idx]; }
    uint64_t getEntryPoint();

    uint32_t addInstruction(Instruction* inst);
};

class InstrumentationFunction : public Instrumentation {
protected:
    char* functionName;

    Instruction** procedureLinkInstructions;
    uint32_t numberOfProcedureLinkInstructions;
    uint64_t procedureLinkOffset;

    Instruction** bootstrapInstructions;
    uint32_t numberOfBootstrapInstructions;
    uint64_t bootstrapOffset;

    Instruction** wrapperInstructions;
    uint32_t numberOfWrapperInstructions;
    uint64_t wrapperOffset;

    uint32_t globalData;
    uint64_t globalDataOffset;

    uint64_t relocationOffset;
public:
    InstrumentationFunction(uint32_t idx, char* funcName);
    ~InstrumentationFunction();

    void print();

    uint32_t sizeNeeded();
    virtual uint32_t bootstrapSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t wrapperSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t procedureLinkSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t globalDataSize() { __SHOULD_NOT_ARRIVE; }

    char* getFunctionName() { return functionName; }
    const char* briefName() { return "InstrumentationFunction"; }

    uint32_t getNumberOfProcedureLinkInstructions() { return numberOfProcedureLinkInstructions; }
    uint32_t getNumberOfBootstrapInstructions() { return numberOfBootstrapInstructions; }
    uint32_t getNumberOfWrapperInstructions() { return numberOfWrapperInstructions; }
    uint32_t getGlobalData() { return globalData; }
    uint64_t getGlobalDataOffset() { return globalDataOffset; }

    void setCodeOffsets(uint64_t plOffset, uint64_t bsOffset, uint64_t fwOffset);
    void setRelocationOffset(uint64_t relocOffset) { relocationOffset = relocOffset; }

    virtual uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateGlobalData(uint64_t textBaseAddress) { __SHOULD_NOT_ARRIVE; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint64_t getEntryPoint();
};

class InstrumentationFunction32 : public InstrumentationFunction {
public:
    InstrumentationFunction32(uint32_t idx, char* funcName) : InstrumentationFunction(idx, funcName) {}
    ~InstrumentationFunction32() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    uint32_t generateWrapperInstructions(uint64_t textBaseAddress);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapSize() { return Size__32_bit_Bootstrap; }
    uint32_t wrapperSize() { return Size__32_bit_Function_Wrapper; }
    uint32_t procedureLinkSize() { return Size__32_bit_Procedure_Link; }
    uint32_t globalDataSize() { return Size__32_bit_Global_Offset_Table_Entry; }
};

class InstrumentationFunction64 : public InstrumentationFunction {
public:
    InstrumentationFunction64(uint32_t idx, char* funcName) : InstrumentationFunction(idx, funcName) {}
    ~InstrumentationFunction64() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    uint32_t generateWrapperInstructions(uint64_t textBaseAddress);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapSize() { return Size__64_bit_Bootstrap; }
    uint32_t wrapperSize() { return Size__64_bit_Function_Wrapper; }
    uint32_t procedureLinkSize() { return Size__64_bit_Procedure_Link; }
    uint32_t globalDataSize() { return Size__64_bit_Global_Offset_Table_Entry; }
};

class InstrumentationPoint : public Base {
private:
    uint32_t index;
    Base* point;
    Instrumentation* instrumentation;

    Instruction** trampolineInstructions;
    uint32_t numberOfTrampolineInstructions;
    uint64_t trampolineOffset;

public:
    InstrumentationPoint(uint32_t idx, Base* pt, Instrumentation* inst);
    ~InstrumentationPoint() {}

    void print();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t getIndex() { return index; }
    uint64_t getSourceAddress();
    uint64_t getTargetOffset() { ASSERT(instrumentation); return instrumentation->getEntryPoint(); }

    uint32_t sizeNeeded();
    uint32_t generateTrampoline(uint32_t count, Instruction** insts, uint64_t offset, uint64_t returnOffset);
};


#endif /* _Instrumentation_h_ */

