#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <BinaryFile.h>

class ElfFile;
class Instruction;

class RawSection : public Base {
protected:
    char* rawDataPtr;
    uint16_t sectionIndex;
    ElfFile* elfFile;

public:
    RawSection(ElfClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
        : Base(classType),rawDataPtr(rawPtr),sectionIndex(scnIdx),elfFile(elf) { sizeInBytes = size; }
    ~RawSection() {}

    virtual uint32_t read(BinaryInputFile* b) {}
    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { return true; }

    char* charStream() { return rawDataPtr; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { binaryOutputFile->copyBytes(charStream(),getSizeInBytes(),offset); }
    void printBytes(uint32_t bytesPerWord, uint32_t bytesPerLine);

    char* getFilePointer() { return rawDataPtr; }
    uint16_t getSectionIndex() { return sectionIndex; }
    void setSectionIndex(uint16_t newidx){ sectionIndex = newidx; }
    ElfFile* getElfFile() { return elfFile; }
};

class DwarfSection : public RawSection {
 protected:
    uint32_t index;

 public:
    DwarfSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(ElfClassTypes_DwarfSection,filePtr,size,scnIdx,elf),index(idx) {}
    ~DwarfSection() {}

    uint32_t getIndex() { return index; }

    const char* briefName() { return "DwarfSection"; }
};


#endif /* _RawSection_h_ */

