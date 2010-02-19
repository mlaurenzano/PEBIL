#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

#include <Base.h>
#include <ElfFile.h>
#include <Instruction.h>
#include <Vector.h>

class AddressAnchor;
class BasicBlock;
class BinaryOutputFile;
class DataReference;
class Function;
class Instrumentation;
class InstrumentationFunction;
class InstrumentationPoint;
class InstrumentationSnippet;
class Instruction;
class LineInfo;
class LineInfoFinder;
class RawSection;
class SectionHeader;
class TextSection;

#define Size__uncond_jump 5
#define Size__flag_protect_full 2
#define Size__32_bit_flag_protect_light 12
#define Size__64_bit_flag_protect_light 18


#define INST_SNIPPET_BOOTSTRAP_BEGIN 0
#define INST_SNIPPET_BOOTSTRAP_END 1
#define INST_POINT_BOOTSTRAP1 0

#define MAX_ARGUMENTS_32BIT 6
#define MAX_ARGUMENTS_64BIT 6

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

    Vector<AddressAnchor*> addressAnchors;
    Vector<DataReference*> specialDataRefs;

    ProgramHeader* instTextSegment;
    ProgramHeader* instDataSegment;

    uint16_t extraTextIdx;
    uint16_t extraDataIdx;
    uint16_t dataIdx;

    uint64_t usableDataOffset;
    uint64_t regStorageOffset;
    uint64_t regStorageReserved;
    uint64_t programDataSize;
    uint64_t programBssSize;
    
    uint64_t relocatedTextSize;
    char* instrumentationData;
    uint64_t instrumentationDataSize;

    LineInfoFinder* lineInfoFinder;

    bool anchorsAreSorted;

    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable();

    void initializeDisabledFunctions(char* inputFuncList);

protected:
    Vector<Function*> exposedFunctions;
    Vector<BasicBlock*> exposedBasicBlocks;
    Vector<Instruction*> exposedInstructions;
    Vector<Instruction*> exposedMemOps;
    Vector<char*> disabledFunctions;

    uint32_t currentPhase;

    char* instSuffix;
    char* sharedLibraryPath;

    // instrumentation functions
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode) { return addInstrumentationPoint(instpoint, inst, instMode, FlagsProtectionMethod_full); }
    InstrumentationPoint* addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod);
    uint32_t addSharedLibrary(const char* libname);
    uint32_t addSharedLibraryPath();
    uint64_t addFunction(InstrumentationFunction* func);
    uint64_t addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset);
    void addInstrumentationFunction(const char* funcname);
    uint64_t relocateDynamicSection();
    uint64_t getProgramBaseAddress();
    void extendTextSection(uint64_t size);
    void allocateInstrumentationText(uint64_t size);
    void extendDataSection();
    void buildInstrumentationSections();
    void generateInstrumentation();
    uint32_t relocateAndBloatFunction(Function* functionToRelocate, uint64_t offsetToRelocation);
    bool isEligibleFunction(Function* func);
    bool is64Bit() { return elfFile->is64Bit(); }

    bool isDisabledFunction(Function* func);


    uint32_t getNumberOfExposedFunctions() { return exposedFunctions.size(); }
    Function* getExposedFunction(uint32_t idx) { return exposedFunctions[idx]; }
    uint32_t getNumberOfExposedBasicBlocks() { return exposedBasicBlocks.size(); }
    BasicBlock* getExposedBasicBlock(uint32_t idx) { return exposedBasicBlocks[idx]; }
    uint32_t getNumberOfExposedInstructions() { return exposedInstructions.size(); }
    Instruction* getExposedInstruction(uint32_t idx) { return exposedInstructions[idx]; }
    uint32_t getNumberOfExposedMemOps() { return exposedMemOps.size(); }
    Instruction* getExposedMemOp(uint32_t idx) { return exposedMemOps[idx]; }    

    BasicBlock* getProgramExitBlock();

public:
    ElfFileInst(ElfFile* elf, char* inputFileList);
    ~ElfFileInst();
    ElfFile* getElfFile() { return elfFile; }
    uint64_t getRegStorageOffset() { return regStorageOffset; }

    void gatherCoverageStats(bool relocHasOccurred, const char* msg);

    void print();
    void print(uint32_t printCodes);
    void dump(char* extension);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool verify();

    void phasedInstrumentation();
    uint32_t anchorProgramElements();
    Vector<AddressAnchor*>* searchAddressAnchors(uint64_t addr);

    TextSection* getTextSection();
    TextSection* getFiniSection();
    TextSection* getInitSection();
    TextSection* getPltSection();

    BasicBlock* getProgramEntryBlock();

    LineInfoFinder* getLineInfoFinder() { return lineInfoFinder; }
    bool hasLineInformation() { return (lineInfoFinder != NULL); }
    void setPathToInstLib(char* libPath);

    char* getApplicationName() { return elfFile->getFileName(); }
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

    void patchProgramContents(); 

    virtual void declare() { __SHOULD_NOT_ARRIVE; }
    virtual void instrument() { __SHOULD_NOT_ARRIVE; }
    virtual bool canRelocateFunction(Function* func) { return true; }
};


#endif /* _ElfFileInst_h_ */
