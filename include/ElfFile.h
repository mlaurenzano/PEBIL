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
class HashTable;
class NoteSection;
class Symbol;

class ElfFile {
private:
    bool is64BitFlag;

    char*        elfFileName;

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
    HashTable* hashTable;
    NoteSection** noteSections;

    uint32_t numberOfPrograms;
    uint32_t numberOfSections;
    uint32_t numberOfStringTables;
    uint16_t sectionNameStrTabIdx;
    uint32_t numberOfSymbolTables;
    uint16_t dynamicSymtabIdx;
    uint32_t numberOfRelocationTables;
    uint32_t numberOfDwarfSections;
    uint32_t numberOfTextSections;
    uint32_t numberOfNoteSections;
    uint64_t dynamicSectionAddress;
    uint16_t dynamicTableSectionIdx;
    uint16_t textSegmentIdx;
    uint16_t dataSegmentIdx;

    uint32_t numberOfFunctions;
    uint32_t numberOfBlocks;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatPOps;

    BinaryInputFile   binaryInputFile;
    BinaryOutputFile  binaryOutputFile;

    void readFileHeader();

    void readSectionHeaders();
    void setSectionTypes();
    void readProgramHeaders();
    void readRawSections();
    void initSectionFilePointers();

    uint32_t findSectionNameInStrTab(char* name);
    void findFunctions();
    void generateCFGs();
    void findMemoryFloatOps();


public:
    bool verify();

    ElfFile(char* f): is64BitFlag(false),elfFileName(f),
        disassembler(NULL),fileHeader(NULL),programHeaders(NULL),sectionHeaders(NULL),
        rawSections(NULL),stringTables(NULL),symbolTables(NULL),relocationTables(NULL),
        dwarfSections(NULL),textSections(NULL),globalOffsetTable(NULL),dynamicTable(NULL),
        hashTable(NULL),noteSections(NULL),
        numberOfPrograms(0),numberOfSections(0),
        numberOfStringTables(0),sectionNameStrTabIdx(0),numberOfSymbolTables(0),dynamicSymtabIdx(0),
        numberOfRelocationTables(0),numberOfDwarfSections(0),numberOfTextSections(0),numberOfNoteSections(0),
        dynamicSectionAddress(0),dynamicTableSectionIdx(0),textSegmentIdx(0),dataSegmentIdx(0),
        numberOfFunctions(0),numberOfBlocks(0),numberOfMemoryOps(0),numberOfFloatPOps(0) {}

    ~ElfFile();

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
    void sortSectionHeaders();

    FileHeader*  getFileHeader() { return fileHeader; }
    ProgramHeader* getProgramHeader(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfPrograms); ASSERT(programHeaders); ASSERT(programHeaders[idx]); return programHeaders[idx]; }
    SectionHeader* getSectionHeader(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfSections); return sectionHeaders[idx]; }
    RawSection* getRawSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfSections); return rawSections[idx]; }
    StringTable* getStringTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfStringTables); return stringTables[idx]; }
    SymbolTable* getSymbolTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfSymbolTables); return symbolTables[idx]; }
    RelocationTable* getRelocationTable(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfRelocationTables); return relocationTables[idx]; }
    DwarfSection* getDwarfSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfDwarfSections); return dwarfSections[idx]; }
    TextSection* getTextSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfTextSections); return textSections[idx]; }
    GlobalOffsetTable* getGlobalOffsetTable() { return globalOffsetTable; }
    DynamicTable* getDynamicTable() { return dynamicTable; }
    HashTable* getHashTable() { return hashTable; }
    NoteSection* getNoteSection(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfNoteSections); return noteSections[idx]; }

    uint16_t getSectionNameStrTabIdx() { return sectionNameStrTabIdx; }

    uint32_t getNumberOfPrograms() { return numberOfPrograms; }
    uint32_t getNumberOfSections() { return numberOfSections; }
    uint32_t getNumberOfStringTables() { return numberOfStringTables; }
    uint32_t getNumberOfSymbolTables() { return numberOfSymbolTables; }
    uint32_t getNumberOfRelocationTables() { return numberOfRelocationTables; }
    uint32_t getNumberOfDwarfSections() { return numberOfDwarfSections; }
    uint32_t getNumberOfTextSections() { return numberOfTextSections; }
    uint32_t getNumberOfNoteSections() { return numberOfNoteSections; }

    uint64_t getDynamicSectionAddress() { return dynamicSectionAddress; }
    uint16_t getDynamicTableSectionIdx() { return dynamicTableSectionIdx; }
    uint16_t getTextSegmentIdx() { return textSegmentIdx; }
    uint16_t getDataSegmentIdx() { return dataSegmentIdx; }
    uint32_t getDynamicSymtabIdx() { return dynamicSymtabIdx; }

    uint32_t getFileSize();

    uint64_t getDataSectionVAddr();
    uint32_t getDataSectionSize();
    uint64_t getBSSSectionVAddr();
    uint64_t getTextSectionVAddr();

    void setLineInfoFinder();
    void findLoops();
    void initTextSections();
    uint32_t printDisassembledCode();
    uint64_t addSection(uint16_t idx, ElfClassTypes classtype, char* bytes, uint32_t name, uint32_t type, uint64_t flags, uint64_t addr, uint64_t offset, 
                        uint64_t size, uint32_t link, uint32_t info, uint64_t addralign, uint64_t entsize);

    void testBitSet();

    uint32_t findSymbol4Addr(uint64_t addr,Symbol** buffer,uint32_t bufCnt,char** namestr=NULL);
};

class ElfFileInst {
private:
    ElfFile* elfFile;

    // for the text sections/segment we just keep the offset since we can compute
    // the address from the base address of the program
    uint64_t extraTextOffset;
    uint32_t extraTextSize;

    uint64_t pltAddress;
    uint32_t pltSize;

    // for data sections/segment we keep (file) offset and (memory) address
    uint64_t extraDataOffset;
    uint64_t extraDataAddress;
    uint32_t extraDataSize;

    uint64_t gotOffset;
    uint64_t gotAddress;
    uint32_t gotSize;

    uint32_t addStringToDynamicStringTable(const char* str);
    uint32_t addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx);
    uint32_t expandHashTable();

public:
    ElfFileInst(ElfFile* elf);
    ~ElfFileInst() {}
    ElfFile* getElfFile() { return elfFile; }

    void print();

    // instrumentation functions
    void addSharedLibrary(const char* libname);
    void addInstrumentationFunction(const char* funcname);
    uint64_t relocateDynamicSection();
    uint64_t getProgramBaseAddress();
    uint64_t extendTextSection(uint32_t size);
    uint64_t extendDataSection(uint32_t size);
    uint64_t reserveProcedureLinkageTable(uint32_t size);
    uint64_t reserveGlobalOffsetTable(uint32_t size);
};


#endif /* _ElfFile_h_ */
