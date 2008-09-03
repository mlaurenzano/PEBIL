#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

#include <ElfFile.h>
#include <SectionHeader.h>
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

#define INST_SNIPPET_BOOTSTRAP_BEGIN 0
#define INST_SNIPPET_BOOTSTRAP_END 1
#define INST_SNIPPETS_RESERVED 2
#define INST_POINT_BOOTSTRAP 0
#define INST_POINTS_RESERVED 1

#define MAX_INST_FUNCTIONS 4

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
protected:
    uint32_t currentPhase;
    ElfFile* elfFile;

    uint32_t numberOfInstrumentationSnippets;
    InstrumentationSnippet** instrumentationSnippets;

    uint32_t numberOfInstrumentationFunctions;
    InstrumentationFunction** instrumentationFunctions;

    uint32_t numberOfInstrumentationLibraries;
    char** instrumentationLibraries;

    uint32_t numberOfInstrumentationPoints;
    InstrumentationPoint** instrumentationPoints;

    uint16_t extraTextIdx;
    uint16_t extraDataIdx;

    uint64_t usableDataOffset;
    uint64_t bssReserved;

    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable();

    uint64_t addInstrumentationPoint(Base* instpoint, Instrumentation* inst);

    // instrumentation functions
    uint32_t addSharedLibrary(const char* libname);
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
    void dump(char* extension);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    void verify();

    void instrument();

    TextSection* getTextSection();
    TextSection* getFiniSection();
    TextSection* getInitSection();

    TextSection* getExtraTextSection() { return (TextSection*)(elfFile->getRawSection(extraTextIdx)); }
    RawSection* getExtraDataSection() { return elfFile->getRawSection(extraDataIdx); }
    uint64_t getExtraDataAddress() { return elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr); }

    uint64_t reserveDataOffset(uint64_t size);

    uint32_t declareFunction(char* funcName);
    uint32_t declareLibrary(char* libName);

    InstrumentationFunction* getInstrumentationFunction(const char* funcName);
    uint32_t addInstrumentationSnippet(InstrumentationSnippet* snip);
    uint64_t addInstrumentationPoint(TextSection* instpoint, Instrumentation* inst);
    uint64_t addInstrumentationPoint(Function* instpoint, Instrumentation* inst);
    uint64_t addInstrumentationPoint(Instruction* instpoint, Instrumentation* inst);

    virtual void declareInstrumentation() { __SHOULD_NOT_ARRIVE; }
    virtual void reserveInstrumentation() { __SHOULD_NOT_ARRIVE; }
};


#endif /* _ElfFileInst_h_ */
