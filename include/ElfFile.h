#ifndef _ElfFile_h_
#define _ElfFile_h_

#include <Base.h>
#include <BinaryFile.h>

class FileHeader;
class ProgramHeader;
class SectionHeader;
class StringTable;
class SymbolTable;
class RelocationTable;

class ElfFile {
private:
    bool is64BitFlag;

    char*        elfFileName;

    BinaryInputFile   binaryInputFile;

    FileHeader*  fileHeader;
    ProgramHeader**  programHeaders;
    SectionHeader** sectionHeaders;
    StringTable** stringTables;
    SymbolTable** symbolTables;
    RelocationTable** relocationTables;

    uint32_t numberOfPrograms;
    uint32_t numberOfSections;
    uint32_t numberOfStringTables;
    uint16_t sectionNameStrTabIdx;
    uint32_t numberOfSymbolTables;
    uint32_t numberOfRelocationTables;

    uint32_t numberOfFunctions;
    uint32_t numberOfBlocks;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatPOps;
    
    void readFileHeader();

    void readSectionHeaders();
    void readProgramHeaders();
    void setSectionNames();
    void readStringTables();
    void readSymbolTables();
    void readRelocationTables();
    void readRawSections();

    void processOverflowSections();
    void readRawSectionData();
//    void readSymbolStringTable(DebugSection* dbg);
    void readRelocLineInfoTable();


    void findFunctions();
    void generateCFGs();
    void findMemoryFloatOps();
    bool verify();

public:

    ElfFile(char* f): is64BitFlag(false),elfFileName(f),
                  fileHeader(NULL),programHeaders(NULL),sectionHeaders(NULL),
/*
rawSections(NULL),
                  symbolTable(NULL),stringTable(NULL),
*/
                  stringTables(NULL),symbolTables(NULL),relocationTables(NULL),numberOfPrograms(0),              
                  numberOfSections(0),
                  numberOfStringTables(0),sectionNameStrTabIdx(0),numberOfSymbolTables(0),numberOfRelocationTables(0),
                  numberOfFunctions(0),numberOfBlocks(0),numberOfMemoryOps(0),numberOfFloatPOps(0) {}

    ~ElfFile() { }

    bool is64Bit() { return is64BitFlag; }

    void parse();

    void briefPrint();
    void print();
    void displaySymbols();

//    RawSection* findRawSection(uint64_t addr);

    char* getElfFileName() { return elfFileName; }

    uint32_t      getNumberOfPrograms() { return numberOfPrograms; }
    uint32_t      getNumberOfSections() { return numberOfSections; }

    FileHeader*  getFileHeader() { return fileHeader; }
    ProgramHeader* getProgramHeader(uint32_t idx) { return programHeaders[idx]; }
    SectionHeader* getSectionHeader(uint32_t idx) { return sectionHeaders[idx]; }

    uint16_t getSectionNameStrTabIdx() { return sectionNameStrTabIdx; }
    uint16_t setSectionNameStrTabIdx();

    uint32_t getNumberOfStringTables() { return numberOfStringTables; }
    uint32_t getNumberOfSymbolTables() { return numberOfSymbolTables; }
    uint32_t getNumberOfRelocationTables() { return numberOfRelocationTables; }

    StringTable* getStringTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfStringTables); return stringTables[idx]; }
    SymbolTable* getSymbolTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfSymbolTables); return symbolTables[idx]; }
    RelocationTable* getRelocationTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfRelocationTables); return relocationTables[idx]; }

    StringTable* findStringTableWithSectionIdx(uint32_t idx);
    SymbolTable* findSymbolTableWithSectionIdx(uint32_t idx);
    RelocationTable* findRelocationTableWithSectionIdx(uint32_t idx);

//    RawSection* getRawSection(uint32_t idx) { return rawSections[idx]; }
//    LineInfoTable* getLineInfoTable(uint32_t idx);

    uint32_t getNumberOfFunctions() { return numberOfFunctions; }
    uint32_t getNumberOfBlocks()    { return numberOfBlocks; }
    uint32_t getNumberOfMemoryOps() { return numberOfMemoryOps; }
    uint32_t getNumberOfFloatPOps() { return numberOfFloatPOps; }

//    BasicBlock* findBasicBlock(HashCode* hashCode);
//    uint32_t getAllBlocks(BasicBlock** arr);

    uint32_t getFileSize();

//    RawSection* getLoaderSection();
    uint64_t getDataSectionVAddr();
    uint32_t getDataSectionSize();
//    RawSection* getBSSSection();
    uint64_t getBSSSectionVAddr();
    uint64_t getTextSectionVAddr();

    void setLineInfoFinder();
    void findLoops();

    void testBitSet();

};

#endif /* _ElfFile_h_ */
