#ifndef _DwarfSection_h_
#define _DwarfSection_h_

#include <Base.h>
#include <RawSection.h>

class ElfFile;

class DwarfSection : public RawSection {
protected:
    ~DwarfSection() {}
    uint32_t index;

public:
    DwarfSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf) 
        : RawSection(ElfClassTypes_dwarf_section,filePtr,size,index,elf),index(idx) {}

    uint32_t getIndex() { return index; }

    const char* briefName() { return "DwarfSection"; }
};

#endif /* _DwarfSection_h_ */
