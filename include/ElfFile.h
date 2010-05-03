#ifndef _ElfFile_h_
#define _ElfFile_h_

#include <Base.h>
#include <BinaryFile.h>
#include <ProgramHeader.h>
#include <Vector.h>

class AddressAnchor;
class DataReference;
class DataSection;
class DwarfLineInfoSection;
class DwarfSection;
class DynamicTable;
class FileHeader;
class GlobalOffsetTable;
class GnuVerneedTable;
class GnuVersymTable;
class HashTable;
class Instruction;
class NoteSection;
class RawSection;
class RelocationTable;
class SectionHeader;
class StringTable;
class Symbol;
class SymbolTable;
class TextSection;

class ElfFile {
private:
    bool is64BitFlag;
    bool staticLinked;

    char*        elfFileName;

    FileHeader*  fileHeader;
    Vector<ProgramHeader*> programHeaders;
    Vector<SectionHeader*> sectionHeaders;
    Vector<RawSection*> rawSections;
    Vector<StringTable*> stringTables;
    Vector<SymbolTable*> symbolTables;
    Vector<RelocationTable*> relocationTables;
    Vector<DwarfSection*> dwarfSections;
    Vector<TextSection*> textSections;
    Vector<NoteSection*> noteSections;
    Vector<DataSection*> dataSections;
    GlobalOffsetTable* globalOffsetTable;
    DynamicTable* dynamicTable;
    Vector<HashTable*> hashTables;
    GnuVerneedTable* gnuVerneedTable;
    GnuVersymTable* gnuVersymTable;
    StringTable* dynamicStringTable;
    SymbolTable* dynamicSymbolTable;
    RelocationTable* pltRelocationTable;
    RelocationTable* dynamicRelocationTable;
    DwarfLineInfoSection* lineInfoSection;

    Vector<AddressAnchor*>* addressAnchors;
    bool anchorsAreSorted;
    Vector<DataReference*> specialDataRefs;

    uint16_t sectionNameStrTabIdx;
    uint16_t dynamicSymtabIdx;
    uint64_t dynamicSectionAddress;
    uint16_t dynamicTableSectionIdx;
    uint16_t textSegmentIdx;
    uint16_t dataSegmentIdx;

    uint32_t numberOfFunctions;
    uint32_t numberOfBlocks;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatPOps;

    BinaryInputFile   binaryInputFile;

    void readFileHeader();

    void readSectionHeaders();
    void setSectionTypes();
    void readProgramHeaders();
    void readRawSections();

    uint32_t findSectionNameInStrTab(char* name);
    void findFunctions();
    void findMemoryFloatOps();

    bool verifyDynamic();
    void initDynamicFilePointers();

public:
    bool verify();

    ElfFile(char* f);
    ~ElfFile();

    bool is64Bit() { return is64BitFlag; }
    bool isStaticLinked() { return staticLinked; }
    void setStaticLinked(bool val) { staticLinked = val; }
    void swapSections(uint32_t idx1, uint32_t idx2);

    uint32_t getAddressAlignment(){ if (is64Bit()) { return sizeof(uint64_t); } else { return sizeof(uint32_t); } }
    void gatherDisassemblyStats();

    Vector<AddressAnchor*>* getAddressAnchors() { return addressAnchors; }
    uint32_t anchorProgramElements();
    Vector<AddressAnchor*>* searchAddressAnchors(uint64_t addr);
    void setAnchorsSorted(bool areSorted) { anchorsAreSorted = areSorted; }

    TextSection* getDotTextSection();
    TextSection* getDotFiniSection();
    TextSection* getDotInitSection();
    TextSection* getDotPltSection();

    void parse();
    void initSectionFilePointers();
    void dump(char* extension);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    void generateCFGs();

    void briefPrint();
    void print();
    void print(uint32_t printCodes);
    void displaySymbols();

    RawSection* findRawSection(uint64_t addr);

    void sortSectionHeaders();

    FileHeader*  getFileHeader() { return fileHeader; }
    ProgramHeader* getProgramHeader(uint32_t idx) { return programHeaders[idx]; }
    ProgramHeader* getProgramHeaderPHDR();
    SectionHeader* getSectionHeader(uint32_t idx) { return sectionHeaders[idx]; }
    RawSection* getRawSection(uint32_t idx) { return rawSections[idx]; }
    StringTable* getStringTable(uint32_t idx) { return stringTables[idx]; }
    SymbolTable* getSymbolTable(uint32_t idx) { return symbolTables[idx]; }
    RelocationTable* getRelocationTable(uint32_t idx) { return relocationTables[idx]; }
    DwarfSection* getDwarfSection(uint32_t idx) { return dwarfSections[idx]; }
    TextSection* getTextSection(uint32_t idx) { return textSections[idx]; }
    DataSection* getDataSection(uint32_t idx) { return dataSections[idx]; }
    DataSection* getDotDataSection();
    GlobalOffsetTable* getGlobalOffsetTable() { return globalOffsetTable; }
    DynamicTable* getDynamicTable() { return dynamicTable; }
    HashTable* getHashTable(uint32_t idx) { return hashTables[idx]; }
    NoteSection* getNoteSection(uint32_t idx) { return noteSections[idx]; }
    GnuVerneedTable* getGnuVerneedTable() { return gnuVerneedTable; }
    GnuVersymTable* getGnuVersymTable() { return gnuVersymTable; }
    StringTable* getDynamicStringTable() { return dynamicStringTable; }
    SymbolTable* getDynamicSymbolTable() { return dynamicSymbolTable; }
    RelocationTable* getPLTRelocationTable() { return pltRelocationTable; }
    RelocationTable* getDynamicRelocationTable() { return dynamicRelocationTable; }
    DwarfLineInfoSection* getLineInfoSection() { return lineInfoSection; }

    uint16_t getSectionNameStrTabIdx() { return sectionNameStrTabIdx; }

    uint32_t getNumberOfPrograms() { return programHeaders.size(); }
    uint32_t getNumberOfSections() { return sectionHeaders.size(); }
    uint32_t getNumberOfStringTables() { return stringTables.size(); }
    uint32_t getNumberOfSymbolTables() { return symbolTables.size(); }
    uint32_t getNumberOfRelocationTables() { return relocationTables.size(); }
    uint32_t getNumberOfDwarfSections() { return dwarfSections.size(); }
    uint32_t getNumberOfTextSections() { return textSections.size(); }
    uint32_t getNumberOfNoteSections() { return noteSections.size(); }
    uint32_t getNumberOfHashTables() { return hashTables.size(); }

    uint64_t getDynamicSectionAddress() { return dynamicSectionAddress; }
    uint16_t getDynamicTableSectionIdx() { return dynamicTableSectionIdx; }
    uint16_t getTextSegmentIdx() { return textSegmentIdx; }
    uint16_t getDataSegmentIdx() { return dataSegmentIdx; }
    uint32_t getDynamicSymtabIdx() { return dynamicSymtabIdx; }

    uint32_t getFileSize();
    char* getFileName() { return elfFileName; }

    uint64_t getDataSectionVAddr();
    uint32_t getDataSectionSize();
    uint64_t getBSSSectionVAddr();
    uint64_t getTextSectionVAddr();

    void setLineInfoFinder();
    void findLoops();
    uint32_t printDisassembly(bool instructionDetail);
    

    ProgramHeader* addSegment(uint16_t idx, uint32_t type, uint64_t offset, uint64_t vaddr, uint64_t paddr,
                        uint32_t memsz, uint32_t filesz, uint32_t flags, uint32_t align);
    uint64_t addSection(uint16_t idx, PebilClassTypes classtype, char* bytes, uint32_t name, uint32_t type, uint64_t flags, uint64_t addr, uint64_t offset, 
                        uint64_t size, uint32_t link, uint32_t info, uint64_t addralign, uint64_t entsize);

    uint16_t findSectionIdx(uint64_t addr);
    uint16_t findSectionIdx(char* name);
    RawSection* findDataSectionAtAddr(uint64_t addr);
    void testBitSet();

    uint32_t findSymbol4Addr(uint64_t addr,Symbol** buffer,uint32_t bufCnt,char** namestr=NULL);
    Symbol* lookupFunctionSymbol(uint64_t);
};

#endif /* _ElfFile_h_ */
