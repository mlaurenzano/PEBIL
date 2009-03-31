#ifndef _Instrumentation_h_
#define _Instrumentation_h_

#include <Base.h>
#include <Instruction.h>
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

#define BOOTSTRAP_BYTE_RATIO 11

#define TRAMPOLINE_FRAME_AUTOINC_SIZE 0x1000

extern int compareSourceAddress(const void* arg1,const void* arg2);

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
    void setBootstrapOffset(uint64_t off) { bootstrapOffset = off; }
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
    void setCodeOffset(uint64_t off) { snippetOffset = off; }
    uint64_t getCodeOffset() { return snippetOffset; }

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

    uint32_t getNumberOfProcedureLinkInstructions() { return procedureLinkInstructions.size(); }
    uint32_t getNumberOfBootstrapInstructions() { return bootstrapInstructions.size(); }
    uint32_t getNumberOfWrapperInstructions() { return wrapperInstructions.size(); }
    uint32_t getGlobalData() { return globalData; }
    uint64_t getGlobalDataOffset() { return globalDataOffset; }

    void setRelocationOffset(uint64_t relocOffset) { relocationOffset = relocOffset; }

    virtual uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateGlobalData(uint64_t textBaseAddress) { __SHOULD_NOT_ARRIVE; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    uint32_t addArgument(uint64_t offset);
    uint32_t addArgument(uint64_t offset, uint32_t value);

    uint64_t getEntryPoint();

    void setWrapperOffset(uint64_t off) { wrapperOffset = off; }
    void setProcedureLinkOffset(uint64_t off) { procedureLinkOffset = off; }
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

typedef enum {
    InstPriority_undefined = 0,
    InstPriority_datainit,
    InstPriority_userinit,
    InstPriority_regular,
    InstPriority_Total_Types
} InstPriorities;

class InstrumentationPoint : public Base {
protected:
    Base* point;
    Instrumentation* instrumentation;

    uint32_t numberOfBytes;
    InstLocations instLocation;

    Vector<Instruction*> trampolineInstructions;
    uint64_t trampolineOffset;

    InstPriorities priority;

public:
    InstrumentationPoint(Base* pt, Instrumentation* inst, uint32_t size, InstLocations loc);
    ~InstrumentationPoint();

    Vector<Instruction*>* swapInstructionsAtPoint(Vector<Instruction*>* replacements);

    void print();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    void setPriority(InstPriorities p) { ASSERT(p && p < InstPriority_Total_Types); priority = p; }
    InstPriorities getPriority() { return priority; }

    uint64_t getSourceAddress();
    uint64_t getTargetOffset() { ASSERT(instrumentation); return instrumentation->getEntryPoint(); }
    Instrumentation* getInstrumentation() { return instrumentation; }

    Base* getSourceObject() { return point; }

    uint32_t getNumberOfBytes() { return numberOfBytes; }
    uint32_t sizeNeeded();
    virtual uint32_t generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, bool stackIsSafe)
         { __SHOULD_NOT_ARRIVE; }
    uint64_t getTrampolineOffset() { return trampolineOffset; }

    bool verify();
};

class InstrumentationPoint32 : public InstrumentationPoint {
public:
    InstrumentationPoint32(Base* pt, Instrumentation* inst, uint32_t size, InstLocations loc) :
        InstrumentationPoint(pt, inst, size, loc) {}
    uint32_t generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, bool stackIsSafe);
};
class InstrumentationPoint64 : public InstrumentationPoint {
public:
    InstrumentationPoint64(Base* pt, Instrumentation* inst, uint32_t size, InstLocations loc) :
        InstrumentationPoint(pt, inst, size, loc) {}
    uint32_t generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, bool stackIsSafe);
};


#endif /* _Instrumentation_h_ */

