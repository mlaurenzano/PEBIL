#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

#include <Base.h>
#include <Vector.h>
#include <ElfFile.h>

class SectionHeader;
class Instruction;
class BinaryOutputFile;
class InstrumentationPoint;
class Instrumentation;
class InstrumentationFunction;
class InstrumentationSnippet;
class Instruction;
class Function;
class TextSection;
class RawSection;
class LineInfoFinder;

#define SIZE_NEEDED_AT_INST_POINT 5

#define INST_SNIPPET_BOOTSTRAP_BEGIN 0
#define INST_SNIPPET_BOOTSTRAP_END 1
#define INST_POINT_BOOTSTRAP 0

#define MAX_ARGUMENTS_32BIT 6
#define MAX_ARGUMENTS_64BIT 6

typedef enum {
    ElfInstPhase_no_phase = 0,
    ElfInstPhase_extend_space,
    ElfInstPhase_user_reserve,
    ElfInstPhase_modify_control,
    ElfInstPhase_generate_instrumentation,
    ElfInstPhase_dump_file,
    ElfInstPhase_Total_Phases
} ElfInstPhases;

class ElfFileInst {
protected:
    uint32_t currentPhase;
    ElfFile* elfFile;

    char* instSuffix;
    char* sharedLibraryPath;

    Vector<InstrumentationSnippet*> instrumentationSnippets;
    Vector<InstrumentationFunction*> instrumentationFunctions;
    Vector<InstrumentationPoint*> instrumentationPoints;
    Vector<char*> instrumentationLibraries;

    uint16_t extraTextIdx;
    uint16_t extraDataIdx;

    uint64_t usableDataOffset;
    uint64_t bssReserved;

    LineInfoFinder* lineInfoFinder;

    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable();

    uint64_t addInstrumentationPoint(Base* instpoint, Instrumentation* inst);

    // instrumentation functions
    uint32_t addSharedLibrary(const char* libname);
    uint32_t addSharedLibraryPath();
    uint64_t addFunction(InstrumentationFunction* func);
    uint64_t addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset);
    void addInstrumentationFunction(const char* funcname);
    uint64_t relocateDynamicSection();
    uint64_t getProgramBaseAddress();
    void extendTextSection(uint64_t size);
    void extendDataSection(uint64_t size);
    void generateInstrumentation();

public:
    ElfFileInst(ElfFile* elf);
    ~ElfFileInst();
    ElfFile* getElfFile() { return elfFile; }

    void print();
    void print(uint32_t printCodes);
    void dump(char* extension);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    void verify();

    void phasedInstrumentation();

    TextSection* getTextSection();
    TextSection* getFiniSection();
    TextSection* getInitSection();

    LineInfoFinder* getLineInfoFinder() { return lineInfoFinder; }
    bool hasLineInformation() { return (lineInfoFinder != NULL); }
    void setPathToInstLib(char* libPath);

    char* getApplicationName() { return elfFile->getFileName(); }
    uint32_t getApplicationSize() { return elfFile->getFileSize(); }
    char* getInstSuffix() { return instSuffix; }

    char* getInstrumentationLibrary(uint32_t idx) { return instrumentationLibraries[idx]; }
    uint32_t getNumberOfInstrumentationLibraries() { return instrumentationLibraries.size(); }

    TextSection* getExtraTextSection();
    RawSection* getExtraDataSection();
    uint64_t getExtraDataAddress();

    uint64_t reserveDataOffset(uint64_t size);
    uint32_t initializeReservedData(uint64_t address, uint32_t size, void* data);

    uint32_t declareFunction(char* funcName);
    uint32_t declareLibrary(char* libName);

    InstrumentationFunction* getInstrumentationFunction(const char* funcName);
    uint32_t addInstrumentationSnippet(InstrumentationSnippet* snip);
    uint64_t addInstrumentationPoint(TextSection* instpoint, Instrumentation* inst);
    uint64_t addInstrumentationPoint(Function* instpoint, Instrumentation* inst);
    uint64_t addInstrumentationPoint(Instruction* instpoint, Instrumentation* inst);

    virtual void declare() { __SHOULD_NOT_ARRIVE; }
    virtual void instrument() { __SHOULD_NOT_ARRIVE; }
};


#endif /* _ElfFileInst_h_ */
