#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <Vector.h>

class AddressAnchor;
class BinaryInputFile;
class BinaryOutputFile;
class ElfFile;
class Instruction;
class RawSection;
class SectionHeader;

class DataReference : public Base {
private:
    uint64_t data;
    uint32_t sectionOffset;
    bool is64bit;

    RawSection* rawSection;
    AddressAnchor* addressAnchor;

public:
    DataReference(uint64_t dat, RawSection* rawsect, bool is64, uint32_t off);
    ~DataReference();

    uint64_t getBaseAddress(); 
    uint32_t getSectionOffset() { return sectionOffset; }
    void initializeAnchor(Base* link);
    AddressAnchor* getAddressAnchor() { return addressAnchor; }

    bool is64Bit() { return is64bit; }

    void dump(BinaryOutputFile* b, uint32_t offset);
    void print();
};


class RawSection : public Base {
protected:
    char* rawDataPtr;
    uint16_t sectionIndex;
    ElfFile* elfFile;
    HashCode hashCode;

    Vector<DataReference*> dataReferences;
public:
    RawSection(ElfClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~RawSection();

    virtual uint32_t read(BinaryInputFile* b) {}
    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify();

    char* charStream() { return rawDataPtr; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    void printBytes(uint32_t bytesPerWord, uint32_t bytesPerLine);

    uint32_t addDataReference(DataReference* dr) { dataReferences.append(dr); return dataReferences.size(); }

    SectionHeader* getSectionHeader();
    char* getFilePointer() { return rawDataPtr; }
    uint16_t getSectionIndex() { return sectionIndex; }
    void setSectionIndex(uint16_t newidx) { sectionIndex = newidx; }
    ElfFile* getElfFile() { return elfFile; }

    HashCode getHashCode() { return hashCode; }
};

#endif /* _RawSection_h_ */

