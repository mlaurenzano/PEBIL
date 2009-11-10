#ifndef _DwarfSection_h_
#define _DwarfSection_h_

#include <Base.h>
#include <CStructuresDwarf.h>
#include <RawSection.h>
#include <Vector.h>
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
        : RawSection(PebilClassType_DwarfSection, filePtr, size, scnIdx, elf),index(idx) {}
    ~DwarfSection() {}

    uint32_t getIndex() { return index; }
};

class DwarfLineInfoSection : public DwarfSection {
protected:
    Vector<LineInfoTable*> lineInfoTables; 

public:
    DwarfLineInfoSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~DwarfLineInfoSection();

    bool verify();
    void print();
    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* b, uint32_t offset);

    uint32_t getNumberOfLineInfoTables() { return lineInfoTables.size(); }
    LineInfoTable* getLineInfoTable(uint32_t idx) { return lineInfoTables[idx]; }
};

#endif /* _DwarfSection_h_ */

