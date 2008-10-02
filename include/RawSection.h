#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>

class ElfFile;
class Instruction;
class BinaryInputFile;
class BinaryOutputFile;
class SectionHeader;

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
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    void printBytes(uint32_t bytesPerWord, uint32_t bytesPerLine);

    SectionHeader* getSectionHeader();
    char* getFilePointer() { return rawDataPtr; }
    uint16_t getSectionIndex() { return sectionIndex; }
    void setSectionIndex(uint16_t newidx) { sectionIndex = newidx; }
    ElfFile* getElfFile() { return elfFile; }
};

#endif /* _RawSection_h_ */

