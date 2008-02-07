#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <defines/RawSection.d>

class ElfFile;

class RawSection : public Base {
protected:
    char* rawDataPtr;
    uint16_t sectionIndex;
    ElfFile* elfFile;

public:
    RawSection(ElfClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
        : Base(classType),rawDataPtr(rawPtr),sectionIndex(scnIdx),elfFile(elf) { sizeInBytes = size; }
    ~RawSection() {}

    uint32_t read(BinaryInputFile* b) {}
    virtual void print() { __SHOULD_NOT_ARRIVE; }

    char* getFilePointer() { return rawDataPtr; }
    uint16_t getSectionIndex() { return sectionIndex; }
    ElfFile* getElfFile() { return elfFile; }

};
#endif /* _RawSection_h_ */
