#ifndef _ElfFile_h_
#define _ElfFile_h_

#include <Base.h>
#include <BinaryFile.h>

class FileHeader;
class ProgramHeader;
class SectionHeader;
class RawSection;
class StringTable;
class SymbolTable;
class RelocationTable;
class DwarfSection;
class TextSection;
class Disassembler;
class GlobalOffsetTable;
class DynamicTable;

class ElfFile {
private:
    bool is64BitFlag;

    char*        elfFileName;

    BinaryInputFile   binaryInputFile;
    BinaryOutputFile  binaryOutputFile;

    Disassembler* disassembler;
    FileHeader*  fileHeader;
    ProgramHeader**  programHeaders;
    SectionHeader** sectionHeaders;
    RawSection** rawSections;
    StringTable** stringTables;
    SymbolTable** symbolTables;
    RelocationTable** relocationTables;
    DwarfSection** dwarfSections;
    TextSection** textSections;
    GlobalOffsetTable* globalOffsetTable;
    DynamicTable* dynamicTable;

    uint32_t numberOfPrograms;
    uint32_t numberOfSections;
    uint32_t numberOfStringTables;
    uint16_t sectionNameStrTabIdx;
    uint32_t numberOfSymbolTables;
    uint16_t dynamicSymtabIdx;
    uint32_t numberOfRelocationTables;
    uint32_t numberOfDwarfSections;
    uint32_t numberOfTextSections;

    uint32_t numberOfFunctions;
    uint32_t numberOfBlocks;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatPOps;
    
    void readFileHeader();

    void readSectionHeaders();
    void setSectionTypes();
    void readProgramHeaders();
    void readRawSections();
    void initRawSectionFilePointers();

    uint32_t findSectionNameInStrTab(char* name);
    void findFunctions();
    void generateCFGs();
    void findMemoryFloatOps();
    bool verify();



public:
    void addToStringTable(StringTable* stTable, char* newSt);
    void addDataSection(uint64_t size, char* bytes);
    void addTextSection(uint64_t size, char* bytes);
    void addSharedLibrary();

    ElfFile(char* f): is64BitFlag(false),elfFileName(f),disassembler(NULL),
        fileHeader(NULL),programHeaders(NULL),sectionHeaders(NULL),
        rawSections(NULL),globalOffsetTable(NULL),dynamicTable(NULL),
        stringTables(NULL),symbolTables(NULL),relocationTables(NULL),numberOfPrograms(0),              
        numberOfSections(0),
        numberOfStringTables(0),sectionNameStrTabIdx(0),numberOfSymbolTables(0),dynamicSymtabIdx(0),
        numberOfRelocationTables(0),numberOfDwarfSections(0),
        numberOfFunctions(0),numberOfBlocks(0),numberOfMemoryOps(0),numberOfFloatPOps(0) {}

    ~ElfFile() { }

    bool is64Bit() { return is64BitFlag; }

    uint32_t getAddressAlignment(){ if (is64Bit()) { return sizeof(uint64_t); } else { return sizeof(uint32_t); } }

    void parse();
    void dump(char* extension);

    void briefPrint();
    void print();
    void displaySymbols();

    RawSection* findRawSection(uint64_t addr);

    char* getElfFileName() { return elfFileName; }
    Disassembler* getDisassembler() { return disassembler; }

    FileHeader*  getFileHeader() { return fileHeader; }
    ProgramHeader* getProgramHeader(uint32_t idx) { return programHeaders[idx]; }
    SectionHeader* getSectionHeader(uint32_t idx) { return sectionHeaders[idx]; }
    RawSection* getRawSection(uint32_t idx) { return rawSections[idx]; }
    StringTable* getStringTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfStringTables); return stringTables[idx]; }
    SymbolTable* getSymbolTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfSymbolTables); return symbolTables[idx]; }
    RelocationTable* getRelocationTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfRelocationTables); return relocationTables[idx]; }
    DwarfSection* getDwarfSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfDwarfSections); return dwarfSections[idx]; }
    TextSection* getTextSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfTextSections); return textSections[idx]; }
    GlobalOffsetTable* getGlobalOffsetTable() { return globalOffsetTable; }
    DynamicTable* getDynamicTable() { return dynamicTable; }

    uint16_t getSectionNameStrTabIdx() { return sectionNameStrTabIdx; }

    uint32_t getNumberOfPrograms() { return numberOfPrograms; }
    uint32_t getNumberOfSections() { return numberOfSections; }
    uint32_t getNumberOfStringTables() { return numberOfStringTables; }
    uint32_t getNumberOfSymbolTables() { return numberOfSymbolTables; }
    uint32_t getNumberOfRelocationTables() { return numberOfRelocationTables; }
    uint32_t getNumberOfDwarfSections() { return numberOfDwarfSections; }
    uint32_t getNumberOfTextSections() { return numberOfTextSections; }

    uint32_t getNumberOfFunctions() { return numberOfFunctions; }
    uint32_t getNumberOfBlocks()    { return numberOfBlocks; }
    uint32_t getNumberOfMemoryOps() { return numberOfMemoryOps; }
    uint32_t getNumberOfFloatPOps() { return numberOfFloatPOps; }

//    BasicBlock* findBasicBlock(HashCode* hashCode);
//    uint32_t getAllBlocks(BasicBlock** arr);

    uint32_t getFileSize();

    uint64_t getDataSectionVAddr();
    uint32_t getDataSectionSize();
    uint64_t getBSSSectionVAddr();
    uint64_t getTextSectionVAddr();

    void setLineInfoFinder();
    void findLoops();
    uint32_t disassemble();
    uint32_t printDisassembledCode();

    void testBitSet();

};

#endif /* _ElfFile_h_ */
