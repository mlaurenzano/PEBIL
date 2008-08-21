#ifndef _ElfFileInst_h_
#define _ElfFileInst_h_

class ElfFile;
class Instruction;
class BinaryOutputFile;


class ElfFileInst {
private:
    ElfFile* elfFile;

    uint16_t extraTextIdx;
    uint64_t extraTextOffset;

    uint32_t relocOffset;

    uint64_t pltOffset;
    uint64_t bootstrapOffset;
    uint64_t trampOffset;

    uint16_t extraDataIdx;
    uint64_t extraDataOffset;

    uint64_t gotOffset;

    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable();

    uint32_t generateProcedureLinkageTable32();
    uint32_t generateProcedureLinkageTable64();

    Instruction** pltInstructions;
    uint32_t numberOfPLTInstructions;

    Instruction** bootstrapInstructions;
    uint32_t numberOfBootstrapInstructions;

    Instruction** trampInstructions;
    uint32_t numberOfTrampInstructions;

    uint32_t* gotEntries;
    uint32_t numberOfGOTEntries;

public:
    ElfFileInst(ElfFile* elf);
    ~ElfFileInst();
    ElfFile* getElfFile() { return elfFile; }

    void print();
    void dump(char* extension);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    void instrument();

    // instrumentation functions
    uint32_t addSharedLibrary(const char* libname, const char* funcname);
    void addPLTRelocationEntry(uint32_t symbolIndex);
    void addInstrumentationFunction(const char* funcname);
    uint64_t relocateDynamicSection();
    uint64_t getProgramBaseAddress();
    void extendTextSection(uint64_t size);
    void extendDataSection(uint64_t size);
    uint32_t generateProcedureLinkageTable();
    void generateGlobalOffsetTable(uint32_t pltReturnOffset);
    void generateFunctionCall();
};


#endif /* _ElfFileInst_h_ */
