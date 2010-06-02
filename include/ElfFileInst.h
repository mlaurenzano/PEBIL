#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

#include <Base.h>
#include <ElfFile.h>
#include <X86Instruction.h>
#include <Vector.h>

class BasicBlock;
class BinaryOutputFile;
class DataReference;
class Function;
class Instrumentation;
class InstrumentationFunction;
class InstrumentationPoint;
class InstrumentationSnippet;
class X86Instruction;
class LineInfo;
class LineInfoFinder;
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
#define TEXT_EXTENSION_INC  0x4000
#define DATA_EXTENSION_INC  0x4000
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

    Vector<InstrumentationSnippet*> instrumentationSnippets;
    Vector<InstrumentationFunction*> instrumentationFunctions;
    Vector<InstrumentationPoint*>* instrumentationPoints;
    Vector<char*> instrumentationLibraries;
    Vector<Function*> relocatedFunctions;
    Vector<uint64_t> relocatedFunctionOffsets;
    Vector<Function*> nonRelocatedFunctions;
    Vector<X86Instruction*> replacedInstructions;

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

    char* instSuffix;
    char* sharedLibraryPath;
    uint64_t flags;
    

    // instrumentation functions
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode) { return addInstrumentationPoint(instpoint, inst, instMode, FlagsProtectionMethod_full); }
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod);
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod, InstLocations loc);
    uint32_t addSharedLibrary(const char* libname);
    uint32_t addSharedLibraryPath();
    uint64_t addFunction(InstrumentationFunction* func);
    uint64_t addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset);
    uint64_t relocateDynamicSection();
    uint64_t getProgramBaseAddress();
    void extendTextSection(uint64_t totalSize, uint64_t headerSize);
    void allocateInstrumentationText(uint64_t totalSize, uint64_t headerSize);
    void extendDataSection();
    void extendDynamicTable();
    void buildInstrumentationSections();
    uint32_t generateInstrumentation();
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

public:
    ElfFileInst(ElfFile* elf);
    ~ElfFileInst();
    ElfFile* getElfFile() { return elfFile; }
    uint64_t getRegStorageOffset() { return regStorageOffset; }

    void gatherCoverageStats(bool relocHasOccurred, const char* msg);

    void print();
    void print(uint32_t printCodes);
    void dump();

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
    void setInstExtension(char* extension);

    char* getApplicationName() { return elfFile->getAppName(); }
    uint32_t getApplicationSize() { return elfFile->getFileSize(); }
    char* getInstSuffix() { return instSuffix; }

    char* getInstrumentationLibrary(uint32_t idx) { return instrumentationLibraries[idx]; }
    uint32_t getNumberOfInstrumentationLibraries() { return instrumentationLibraries.size(); }

    TextSection* getInstTextSection();
    RawSection* getInstDataSection();
    uint64_t getInstDataAddress();

    uint64_t reserveDataOffset(uint64_t size);
    uint32_t initializeReservedData(uint64_t address, uint32_t size, void* data);

    void functionSelect();
    uint64_t functionRelocateAndTransform(uint32_t offset);

    InstrumentationFunction* declareFunction(char* funcName);
    uint32_t declareLibrary(char* libName);

    InstrumentationFunction* getInstrumentationFunction(const char* funcName);
    uint32_t addInstrumentationSnippet(InstrumentationSnippet* snip);

    virtual void declare() { __SHOULD_NOT_ARRIVE; }
    virtual void instrument() { __SHOULD_NOT_ARRIVE; }
    virtual void usesModifiedProgram() { __SHOULD_NOT_ARRIVE; }
    virtual bool canRelocateFunction(Function* func) { return true; }
};


#endif /* _ElfFileInst_h_ */
