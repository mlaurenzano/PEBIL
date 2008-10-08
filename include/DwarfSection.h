#ifndef _DwarfSection_h_
#define _DwarfSection_h_

#include <Base.h>
#include <RawSection.h>
#include <CStructuresDwarf.h>
#include <defines/LineInformation.d>

class LineInfoTable;

#define SIZE_DWARF_NAME_BEGINS 7
#define DWARF_NAME_BEGINS ".debug_"

#define DWARF_LINE_INFO_SCN_NAME ".debug_line"

class DwarfSection : public RawSection {
protected:
    uint32_t index;

public:
    DwarfSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(ElfClassTypes_DwarfSection,filePtr,size,scnIdx,elf),index(idx) {}
    ~DwarfSection() {}

    uint32_t getIndex() { return index; }
    virtual const char* briefName() { return "DwarfSection"; }
};

class DwarfLineInfoSection : public DwarfSection {
protected:
    LineInfoTable** lineInfoTables;
    uint32_t numberOfLineInfoTables;

public:
    DwarfLineInfoSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~DwarfLineInfoSection();

    const char* briefName() { return "DwarfLineInfoSection"; }

    bool verify();
    void print();
    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* b, uint32_t offset);

    uint32_t getNumberOfLineInfoTables() { return numberOfLineInfoTables; }
    LineInfoTable* getLineInfoTable(uint32_t idx) { ASSERT(idx < numberOfLineInfoTables); return lineInfoTables[idx]; }
};

#endif /* _DwarfSection_h_ */

