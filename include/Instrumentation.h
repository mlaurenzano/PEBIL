#ifndef _Instrumentation_h_
#define _Instrumentation_h_

#include <Base.h>
#include <CStructuresX86.h>
#include <Vector.h>

class Instruction;

#define PLT_RETURN_OFFSET_32BIT 6
#define PLT_RETURN_OFFSET_64BIT 6

#define Size__32_bit_function_bootstrap 128
#define Size__64_bit_function_bootstrap 128
#define Size__32_bit_procedure_link 16
#define Size__64_bit_procedure_link 16
#define Size__32_bit_function_wrapper 64
#define Size__64_bit_function_wrapper 128


class Instrumentation : public Base {
protected:
    Vector<Instruction*> bootstrapInstructions;
    uint64_t bootstrapOffset;
public:
    Instrumentation(ElfClassTypes typ);
    ~Instrumentation();

    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t sizeNeeded() { __SHOULD_NOT_ARRIVE; }
    virtual uint64_t getEntryPoint() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { return true; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; } 

    uint32_t bootstrapSize();
};

class InstrumentationSnippet : public Instrumentation {
private:
    Vector<Instruction*> snippetInstructions;
    uint64_t snippetOffset;

    uint32_t numberOfDataEntries;
    uint32_t* dataEntrySizes;
    uint64_t* dataEntryOffsets;

public:
    InstrumentationSnippet();
    ~InstrumentationSnippet();    

    void print();
    uint32_t bootstrapSize();
    uint32_t snippetSize();
    uint32_t dataSize();

    void setCodeOffsets(uint64_t btOffset, uint64_t spOffset);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t reserveData(uint64_t offset, uint32_t size);
    void initializeReservedData(uint32_t idx, uint32_t size, char* buff, uint64_t dataBaseAddress);

    uint32_t generateSnippetControl();

    uint64_t getEntryPoint();

    uint32_t addSnippetInstruction(Instruction* inst);
};

class InstrumentationFunction : public Instrumentation {
protected:
    char* functionName;
    uint32_t index;

    Vector<Instruction*> procedureLinkInstructions;
    uint64_t procedureLinkOffset;

    Vector<Instruction*> wrapperInstructions;
    uint64_t wrapperOffset;

    uint32_t globalData;
    uint64_t globalDataOffset;

    uint64_t relocationOffset;

    uint32_t numberOfArguments;
    uint64_t* argumentOffsets;
    uint32_t* argumentValues;
public:
    InstrumentationFunction(uint32_t idx, char* funcName, uint64_t dataoffset);
    ~InstrumentationFunction();

    void print();

    uint32_t sizeNeeded();
    uint32_t wrapperSize();
    uint32_t procedureLinkSize();
    uint32_t globalDataSize();

    virtual uint32_t bootstrapReservedSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t procedureLinkReservedSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t wrapperReservedSize() { __SHOULD_NOT_ARRIVE; }

    char* getFunctionName() { return functionName; }
    const char* briefName() { return "InstrumentationFunction"; }

    uint32_t getNumberOfProcedureLinkInstructions() { return procedureLinkInstructions.size(); }
    uint32_t getNumberOfBootstrapInstructions() { return bootstrapInstructions.size(); }
    uint32_t getNumberOfWrapperInstructions() { return wrapperInstructions.size(); }
    uint32_t getGlobalData() { return globalData; }
    uint64_t getGlobalDataOffset() { return globalDataOffset; }

    void setCodeOffsets(uint64_t plOffset, uint64_t bsOffset, uint64_t fwOffset);
    void setRelocationOffset(uint64_t relocOffset) { relocationOffset = relocOffset; }

    virtual uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateGlobalData(uint64_t textBaseAddress) { __SHOULD_NOT_ARRIVE; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    uint32_t addArgument(uint64_t offset);
    uint32_t addArgument(uint64_t offset, uint32_t value);

    uint64_t getEntryPoint();
};

class InstrumentationFunction32 : public InstrumentationFunction {
public:
    InstrumentationFunction32(uint32_t idx, char* funcName, uint64_t dataoffset) : InstrumentationFunction(idx,funcName,dataoffset) {}
    ~InstrumentationFunction32() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapReservedSize() { return Size__32_bit_function_bootstrap; }
    uint32_t procedureLinkReservedSize() { return Size__32_bit_procedure_link; }
    uint32_t wrapperReservedSize() { return Size__32_bit_function_wrapper; }
};

class InstrumentationFunction64 : public InstrumentationFunction {
public:
    InstrumentationFunction64(uint32_t idx, char* funcName,uint64_t dataoffset) : InstrumentationFunction(idx,funcName,dataoffset) {}
    ~InstrumentationFunction64() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapReservedSize() { return Size__64_bit_function_bootstrap; }
    uint32_t procedureLinkReservedSize() { return Size__64_bit_procedure_link; }
    uint32_t wrapperReservedSize() { return Size__64_bit_function_wrapper; }
};

class InstrumentationPoint : public Base {
private:
    Base* point;
    Instrumentation* instrumentation;

    uint64_t sourceAddress;

    Vector<Instruction*> trampolineInstructions;
    uint64_t trampolineOffset;

public:
    InstrumentationPoint(Base* pt, Instrumentation* inst);
    ~InstrumentationPoint();

    void print();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint64_t getSourceAddress() { return sourceAddress; }
    uint64_t getTargetOffset() { ASSERT(instrumentation); return instrumentation->getEntryPoint(); }

    uint32_t sizeNeeded();
    uint32_t generateTrampoline(uint32_t count, Instruction** insts, uint64_t offset, uint64_t returnOffset);
};


#endif /* _Instrumentation_h_ */

