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

#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

#include <Base.h>
#include <ElfFile.h>
#include <X86Instruction.h>
#include <Vector.h>

class BasicBlock;
class BinaryOutputFile;
class DataReference;
class FlowGraph;
class Function;
class Instrumentation;
class InstrumentationFunction;
class InstrumentationPoint;
class InstrumentationSnippet;
class X86Instruction;
class LineInfo;
class LineInfoFinder;
class Loop;
class RawSection;
class SectionHeader;
class TextSection;

#define INST_SNIPPET_BOOTSTRAP_BEGIN 0
#define INST_SNIPPET_BOOTSTRAP_END 1
#define INST_POINT_BOOTSTRAP1 0

#define MAX_ARGUMENTS_32BIT 6
#define MAX_ARGUMENTS_64BIT 6

#define HAS_INSTRUMENTOR_FLAG(__flag, __n) ((__flag) & (__n))
#define InstrumentorFlag_none       0x0
#define InstrumentorFlag_norelocate 0x1

#define INSTHDR_RESERVE_AMT 0x1000
#define TEXT_EXTENSION_INC  0x40000
#define DATA_EXTENSION_INC  0x40000
#define DEFAULT_INST_SEGMENT_IDX 4
#define TEMP_SEGMENT_SIZE 0x10000000

typedef enum {
    ElfInstPhase_no_phase = 0,
    ElfInstPhase_extend_space,
    ElfInstPhase_user_declare,
    ElfInstPhase_user_reserve,
    ElfInstPhase_modify_control,
    ElfInstPhase_generate_instrumentation,
    ElfInstPhase_dump_file,
    ElfInstPhase_Total_Phases
} ElfInstPhases;

class ElfFileInst {
private:
    ElfFile* elfFile;

    BasicBlock* programEntryBlock;
    Vector<Function*> hiddenFunctions;

    Vector<uint64_t> pointerAddrs;
    Vector<uint64_t> pointerPtrs;

    Vector<InstrumentationSnippet*> instrumentationSnippets;
    Vector<InstrumentationFunction*> instrumentationFunctions;
    Vector<InstrumentationPoint*>* instrumentationPoints;
    Vector<char*> instrumentationLibraries;
    Vector<Function*> relocatedFunctions;
    Vector<uint64_t> relocatedFunctionOffsets;
    Vector<Function*> nonRelocatedFunctions;
    Vector<X86Instruction*> replacedInstructions;
    Vector<BasicBlock*> interposedBlocks;

    bool allowStatic;
    bool threadedMode;
    bool perInstruction;

    ProgramHeader* instSegment;

    uint16_t extraTextIdx;
    uint16_t extraDataIdx;
    uint16_t dataIdx;

    uint64_t usableDataOffset;
    uint64_t regStorageOffset;
    uint64_t fxStorageOffset;
    uint64_t regStorageReserved;
    uint64_t dynamicTableReserved;
    
    uint64_t relocatedTextSize;
    char* instrumentationData;
    uint64_t instrumentationDataSize;
    uint64_t instrumentationDataAddress;

    LineInfoFinder* lineInfoFinder;


    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable(uint32_t idx);

    void initializeDisabledFunctions(char* inputFuncList);

    void applyInstrumentationDataToRaw();
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    void declareLibraryList();
protected:
    Vector<Function*> allFunctions;
    Vector<Function*> exposedFunctions;
    Vector<BasicBlock*> exposedBasicBlocks;
    Vector<X86Instruction*> exposedInstructions;
    Vector<X86Instruction*> exposedMemOps;
    Vector<char*>* disabledFunctions;

    Vector<SectionHeader*> instSectionHeaders;
    Vector<ProgramHeader*> instProgramHeaders;

    uint32_t currentPhase;

    char* sharedLibraryPath;
    uint64_t flags;
    char* libraryList;
    
    BasicBlock* findExposedBasicBlock(HashCode hashCode);

    // instrumentation functions
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode);
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, InstLocations loc);
    uint32_t addSharedLibrary(const char* libname);
    uint32_t addSharedLibraryPath();
    uint64_t addFunction(InstrumentationFunction* func);
    uint64_t addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset);
    uint64_t relocateDynamicSection();
    void extendTextSection(uint64_t totalSize, uint64_t headerSize);
    void allocateInstrumentationText(uint64_t totalSize, uint64_t headerSize);
    void extendDataSection(uint32_t amt);
    void extendDynamicTable();
    void buildInstrumentationSections();
    uint32_t generateInstrumentation();
    void computeInstrumentationOffsets();
    void compressInstrumentation(uint32_t textSize);
    uint32_t relocateAndBloatFunction(Function* functionToRelocate, uint64_t offsetToRelocation, Vector<Vector<InstrumentationPoint*>*>* functionInstPoints);
    bool isEligibleFunction(Function* func);
    bool is64Bit() { return elfFile->is64Bit(); }

    bool isDisabledFunction(Function* func);

    uint32_t getNumberOfExposedFunctions() { return exposedFunctions.size(); }
    Function* getExposedFunction(uint32_t idx) { return exposedFunctions[idx]; }
    uint32_t getNumberOfExposedBasicBlocks() { return exposedBasicBlocks.size(); }
    BasicBlock* getExposedBasicBlock(uint32_t idx) { return exposedBasicBlocks[idx]; }
    uint32_t getNumberOfExposedInstructions() { return exposedInstructions.size(); }
    X86Instruction* getExposedInstruction(uint32_t idx) { return exposedInstructions[idx]; }
    uint32_t getNumberOfExposedMemOps() { return exposedMemOps.size(); }
    X86Instruction* getExposedMemOp(uint32_t idx) { return exposedMemOps[idx]; }    

    BasicBlock* getProgramExitBlock();
    Vector<X86Instruction*>* findAllCalls(char* fnames);

    BasicBlock* initInterposeBlock(FlowGraph* fg, uint32_t bbsrcidx, uint32_t bbtgtidx);

public:
    ElfFileInst(ElfFile* elf);
    ~ElfFileInst();
    ElfFile* getElfFile() { return elfFile; }
    uint64_t getRegStorageOffset() { return regStorageOffset; }

    void gatherCoverageStats(bool relocHasOccurred, const char* msg);

    void print();
    void print(uint32_t printCodes);
    void dump(char* extension);

    bool verify();

    void phasedInstrumentation();

    TextSection* getDotTextSection();
    TextSection* getDotFiniSection();
    TextSection* getDotInitSection();
    TextSection* getDotPltSection();

    BasicBlock* getProgramEntryBlock();

    void setInputFunctions(char* inputFuncList);

    LineInfoFinder* getLineInfoFinder() { return lineInfoFinder; }
    bool hasLineInformation() { return (lineInfoFinder != NULL); }
    void setPathToInstLib(char* libPath);
    void setAllowStatic() { allowStatic = true; }
    void setThreadedMode() { threadedMode = true; ASSERT(is64Bit() && "Threading support not available for IA32"); }
    bool isThreadedMode() { return threadedMode; }
    void setPerInstruction() { perInstruction = true; }
    bool isPerInstruction() { return perInstruction; }

    char* getApplicationName() { return elfFile->getAppName(); }
    uint32_t getApplicationSize() { return elfFile->getFileSize(); }
    char* getFullFileName() { return elfFile->getFileName(); }

    char* getInstrumentationLibrary(uint32_t idx) { return instrumentationLibraries[idx]; }
    uint32_t getNumberOfInstrumentationLibraries() { return instrumentationLibraries.size(); }

    TextSection* getInstTextSection();
    RawSection* getInstDataSection();
    uint64_t getInstDataAddress();

    uint64_t reserveDataOffset(uint64_t size);
    uint64_t reserveDataAddress(uint64_t size);
    uint32_t initializeReservedData(uint64_t address, uint32_t size, void* data);
    uint32_t initializeReservedPointer(uint64_t addr, uint64_t ptr);

    void functionSelect();
    uint64_t functionRelocateAndTransform(uint32_t offset);

    InstrumentationFunction* declareFunction(char* funcName);
    uint32_t declareLibrary(const char* libName);
    void setLibraryList(char* libList) { libraryList = libList; }

    InstrumentationFunction* getInstrumentationFunction(const char* funcName);
    uint32_t addInstrumentationSnippet(InstrumentationSnippet* snip);
    InstrumentationSnippet* addInstrumentationSnippet();

    virtual uint64_t reserveDynamicPoints() { __SHOULD_NOT_ARRIVE; }
    virtual void applyDynamicPoints(uint64_t dynArray) { __SHOULD_NOT_ARRIVE; }
    virtual void declare() { __SHOULD_NOT_ARRIVE; }
    virtual void instrument() { __SHOULD_NOT_ARRIVE; }
    virtual const char* getExtension() { __SHOULD_NOT_ARRIVE; }
    virtual bool canRelocateFunction(Function* func) { return true; }
};


#endif /* _ElfFileInst_h_ */
