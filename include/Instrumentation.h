/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _Instrumentation_h_
#define _Instrumentation_h_

#include <Base.h>
#include <X86Instruction.h>
#include <Vector.h>

class X86Instruction;
class InstrumentationPoint;

#define SNIPPET_TRAMPOLINE_DEFAULT false

#define PLT_RETURN_OFFSET_32BIT 6
#define PLT_RETURN_OFFSET_64BIT 6

// we leave the bootstrap mechanisms in place, but this functionality is
// acheived now using regular data initialization (ElfFileInst::initializeReservedData)
#define Size__32_bit_function_bootstrap 0
#define Size__64_bit_function_bootstrap 0

#define Size__32_bit_procedure_link 16
#define Size__64_bit_procedure_link 16
#define Size__32_bit_function_wrapper 128
#define Size__64_bit_function_wrapper 256

#define FXSTORAGE_RESERVED 0x1000
#define Size__trampoline_stackalign 0x1000
#define Size__trampoline_autoinc 0x80
#define Size__near_call_stack_inc 0x08

#define Size__32_bit_inst_function_call_support 5
#define Size__64_bit_inst_function_call_support 5

#define Size__uncond_jump 5
#define Size__flag_protect_full 2

#ifdef THREAD_SAFE
#define Size__32_bit_flag_protect_light 16
#define Size__64_bit_flag_protect_light 18
#else 
#define Size__32_bit_flag_protect_light 12
#define Size__64_bit_flag_protect_light 18
#endif // THREAD_SAFE

#define Num__64_bit_StackArgs 6
#define Num__32_bit_StackArgs 0
#define __MAX_ARGS_SUPPORTED 6

extern uint32_t map64BitArgToReg(uint32_t idx);
extern int compareInstFuncBaseAddress(const void* arg1, const void* arg2);
extern int compareInstBaseAddress(const void* arg1, const void* arg2);
extern int compareInstSourceAddress(const void* arg1, const void* arg2);

class InstrumentationPoint;
extern Vector<InstrumentationPoint*>* instpointFilterAddressRange(Base* object, Vector<InstrumentationPoint*>* instPoints);


class Instrumentation : public Base {
protected:
    Vector<X86Instruction*> bootstrapInstructions;
    uint64_t bootstrapOffset;

    bool distinctTrampoline;
    InstrumentationPoint* point;

public:
    bool canOverflow;

    Instrumentation(PebilClassTypes typ);
    ~Instrumentation();

    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t sizeNeeded() { __SHOULD_NOT_ARRIVE; }
    virtual uint64_t getEntryPoint() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { return true; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr) { __SHOULD_NOT_ARRIVE; } 

    virtual uint32_t getNumberOfCoreInstructions() { __SHOULD_NOT_ARRIVE; }
    virtual X86Instruction* getCoreInstruction(uint32_t idx) { __SHOULD_NOT_ARRIVE; }
    virtual X86Instruction* removeNextCoreInstruction() { __SHOULD_NOT_ARRIVE; }
    virtual bool hasMoreCoreInstructions() { __SHOULD_NOT_ARRIVE; }
    virtual void appendCoreInstruction(X86Instruction* ins) { __SHOULD_NOT_ARRIVE; }
    virtual void prependCoreInstruction(X86Instruction* ins) { __SHOULD_NOT_ARRIVE; }

    void setRequiresDistinctTrampoline(bool rdt) { distinctTrampoline = rdt; }
    bool requiresDistinctTrampoline() { return distinctTrampoline; }
    void setOverflowable(bool v) { canOverflow = v; }

    uint32_t bootstrapSize();
    void setBootstrapOffset(uint64_t off) { bootstrapOffset = off; }

    InstrumentationPoint* getInstrumentationPoint() { return point; }
    void setInstrumentationPoint(InstrumentationPoint* pt) { point = pt; }

};

class InstrumentationSnippet : public Instrumentation {
private:
    Vector<X86Instruction*> snippetInstructions;
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
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr);

    uint32_t reserveData(uint64_t offset, uint32_t size);

    uint32_t generateSnippetControl();

    uint64_t getEntryPoint();

    uint32_t getNumberOfCoreInstructions() { return snippetInstructions.size(); }
    X86Instruction* getCoreInstruction(uint32_t idx) { ASSERT(snippetInstructions[idx]); return snippetInstructions[idx]; }
    X86Instruction* removeNextCoreInstruction() { ASSERT(hasMoreCoreInstructions()); return snippetInstructions.remove(0); }
    bool hasMoreCoreInstructions() { return (snippetInstructions.size() != 0); }
    void appendCoreInstruction(X86Instruction* ins) { snippetInstructions.append(ins); }
    void prependCoreInstruction(X86Instruction* ins) { snippetInstructions.insert(ins, 0); }

    uint32_t addSnippetInstruction(X86Instruction* inst);
    void setCodeOffset(uint64_t off) { snippetOffset = off; }
    uint64_t getCodeOffset() { return snippetOffset; }

};

typedef enum {
    ArgumentType_Undefined = 0,
    ArgumentType_Address,
    ArgumentType_Constant,
    ArgumentType_TotalTypes
} ArgumentTypes;

typedef struct {
    uint32_t type;
    uint32_t reg;
    uint64_t offset;
} Argument;

class InstrumentationFunction : public Instrumentation {
protected:
    char* functionName;
    uint32_t index;

    bool staticLinked;
    uint64_t functionEntry;
    bool assumeFunctionFP;
    bool assumeFlagsUnsafe;

    Vector<X86Instruction*> procedureLinkInstructions;
    uint64_t procedureLinkOffset;

    Vector<X86Instruction*> wrapperInstructions;
    uint64_t wrapperOffset;

    uint32_t globalData;
    uint64_t globalDataOffset;

    uint64_t relocationOffset;

    Vector<Argument> arguments;
    bool skipWrapper;
    Vector<X86Instruction*> pltHooks;

public:
    InstrumentationFunction(uint32_t idx, char* funcName, uint64_t dataoffset, uint64_t fEntry);
    ~InstrumentationFunction();

    void print();

    bool isStaticLinked() { return (functionEntry != 0); }
    uint64_t getFunctionEntry() { return functionEntry; }
    void assumeNoFunctionFP();
    void assumeNoFlagsUnsafe();

    uint32_t sizeNeeded();
    uint32_t wrapperSize();
    uint32_t procedureLinkSize();
    uint32_t globalDataSize();
    void setSkipWrapper() { skipWrapper = true; }
    bool hasSkipWrapper() { return skipWrapper; }
    void addPLTHook(X86Instruction* hook) { pltHooks.append(hook); }

    virtual uint32_t bootstrapReservedSize() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t procedureLinkReservedSize() { Size__32_bit_procedure_link; }
    virtual uint32_t wrapperReservedSize() { __SHOULD_NOT_ARRIVE; }

    char* getFunctionName() { return functionName; }

    uint32_t getNumberOfProcedureLinkInstructions() { return procedureLinkInstructions.size(); }
    uint32_t getNumberOfBootstrapInstructions() { return bootstrapInstructions.size(); }
    uint32_t getNumberOfWrapperInstructions() { return wrapperInstructions.size(); }
    uint32_t getGlobalData() { return globalData; }
    uint64_t getGlobalDataOffset() { return globalDataOffset; }

    void setRelocationOffset(uint64_t relocOffset) { relocationOffset = relocOffset; }

    uint32_t getNumberOfCoreInstructions() { return wrapperInstructions.size(); }
    X86Instruction* getCoreInstruction(uint32_t idx) { ASSERT(wrapperInstructions[idx]); return wrapperInstructions[idx]; }
    X86Instruction* removeNextCoreInstruction() { ASSERT(hasMoreCoreInstructions()); return wrapperInstructions.remove(0); }
    bool hasMoreCoreInstructions() { return (wrapperInstructions.size() != 0); }
    void appendCoreInstruction(X86Instruction* ins) { wrapperInstructions.append(ins); }
    void prependCoreInstruction(X86Instruction* ins) { wrapperInstructions.insert(ins, 0); }

    virtual uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t fxStorageOffset, ElfFileInst* elfInst) { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t generateGlobalData(uint64_t textBaseAddress) { __SHOULD_NOT_ARRIVE; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr);

    uint32_t addConstantArgument();
    uint32_t addArgument(uint64_t offset);

    uint64_t getEntryPoint();

    void setWrapperOffset(uint64_t off) { wrapperOffset = off; }
    void setProcedureLinkOffset(uint64_t off) { procedureLinkOffset = off; }
};

class InstrumentationFunction32 : public InstrumentationFunction {
public:
    InstrumentationFunction32(uint32_t idx, char* funcName, uint64_t dataoffset, uint64_t fEntry) : InstrumentationFunction(idx,funcName,dataoffset,fEntry) {}
    ~InstrumentationFunction32() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t fxStorageOffset, ElfFileInst* elfInst);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapReservedSize() { return Size__32_bit_function_bootstrap; }
    uint32_t procedureLinkReservedSize() { return Size__32_bit_procedure_link; }
    uint32_t wrapperReservedSize() { return Size__32_bit_function_wrapper; }
};

class InstrumentationFunction64 : public InstrumentationFunction {
public:
    InstrumentationFunction64(uint32_t idx, char* funcName,uint64_t dataoffset, uint64_t fEntry) : InstrumentationFunction(idx,funcName,dataoffset,fEntry) {}
    ~InstrumentationFunction64() {}

    uint32_t generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress);
    uint32_t generateBootstrapInstructions(uint64_t textbaseAddress, uint64_t dataBaseAddress);
    virtual uint32_t generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t fxStorageOffset, ElfFileInst* elfInst);
    uint32_t generateGlobalData(uint64_t textBaseAddress);

    uint32_t bootstrapReservedSize() { return Size__64_bit_function_bootstrap; }
    uint32_t procedureLinkReservedSize() { return Size__64_bit_procedure_link; }
    uint32_t wrapperReservedSize() { return Size__64_bit_function_wrapper; }
};

// instrumentation points at the same place are executed in ascending order here (low executed last)
typedef enum {
    InstPriority_undefined = 0,
    InstPriority_sysinit,
    InstPriority_userinit,
    InstPriority_regular,
    InstPriority_low,
    InstPriority_Total_Types
} InstPriorities;

static const char* InstPriorityNames[InstPriority_Total_Types] = {
    "undefined",
    "sysinit",
    "userinit",
    "regular",
    "low"
};

class InstrumentationPoint : public Base {
protected:
    X86Instruction* point;
    Instrumentation* instrumentation;

    uint32_t numberOfBytes;
    InstLocations instLocation;
    int32_t offsetFromPoint;

    Vector<X86Instruction*> trampolineInstructions;
    uint64_t trampolineOffset;

    InstPriorities priority;
    InstrumentationModes instrumentationMode;

    FlagsProtectionMethods protectionMethod;
    BitSet<uint32_t>* deadRegs;

    Vector<X86Instruction*> precursorInstructions;
    Vector<X86Instruction*> postcursorInstructions;

public:

    InstrumentationPoint(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc);
    ~InstrumentationPoint();

    Vector<X86Instruction*>* swapInstructionsAtPoint(Vector<X86Instruction*>* replacements);

    void setInstSourceOffset(int32_t off) { offsetFromPoint = off; }
    uint32_t getInstSourceOffset() { return offsetFromPoint; }
    uint64_t getInstSourceAddress() { return getInstBaseAddress() + offsetFromPoint; }
    void print();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset, uint64_t addr);

    void setPriority(InstPriorities p) { ASSERT(p && p < InstPriority_Total_Types); priority = p; }
    InstPriorities getPriority() { return priority; }

    FlagsProtectionMethods getFlagsProtectionMethod();
    BitSet<uint32_t>* getProtectedRegisters();
    void setFlagsProtectionMethod(FlagsProtectionMethods p);

    uint64_t getTargetOffset() { ASSERT(instrumentation); return instrumentation->getEntryPoint(); }
    Instrumentation* getInstrumentation() { return instrumentation; }

    X86Instruction* getSourceObject() { return point; }
    PebilClassTypes getPointType() { ASSERT(getSourceObject()); return getSourceObject()->getType(); }
    uint64_t getInstBaseAddress() { return point->cacheBaseAddress; }
    InstLocations getInstLocation() { return instLocation; }

    uint32_t getNumberOfBytes() { return numberOfBytes; }
    InstrumentationModes getInstrumentationMode() { return instrumentationMode; }

    uint32_t sizeNeeded();
    virtual uint32_t generateTrampoline(Vector<X86Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, uint64_t currentOffset)
         { __SHOULD_NOT_ARRIVE; }
    uint64_t getTrampolineOffset() { return trampolineOffset; }

    X86Instruction* removeNextPrecursorInstruction() { ASSERT(hasMorePrecursorInstructions()); return precursorInstructions.remove(0); }
    bool hasMorePrecursorInstructions() { return (precursorInstructions.size() != 0); }
    uint32_t addPrecursorInstruction(X86Instruction* inst);
    X86Instruction* removeNextPostcursorInstruction() { ASSERT(hasMorePostcursorInstructions()); return postcursorInstructions.remove(0); }
    bool hasMorePostcursorInstructions() { return (postcursorInstructions.size() != 0); }
    uint32_t addPostcursorInstruction(X86Instruction* inst);

    uint32_t countPrecursorInstructions() { return precursorInstructions.size(); }
    uint32_t countPostcursorInstructions() { return postcursorInstructions.size(); }
    X86Instruction* getPrecursorInstruction(uint32_t i) { return precursorInstructions[i]; }
    X86Instruction* getPostcursorInstruction(uint32_t i) { return postcursorInstructions[i]; }

    bool verify();
    virtual void insertStateProtection() { __SHOULD_NOT_ARRIVE; }
};

class InstrumentationPoint32 : public InstrumentationPoint {
public:
    InstrumentationPoint32(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc);

    uint32_t generateTrampoline(Vector<X86Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, uint64_t currentOffset);
    void insertStateProtection();
};
class InstrumentationPoint64 : public InstrumentationPoint {
public:
    InstrumentationPoint64(Base* pt, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc);

    uint32_t generateTrampoline(Vector<X86Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool doReloc, uint64_t regStorageOffset, uint64_t currentOffset);
    void insertStateProtection();
};


#endif /* _Instrumentation_h_ */

