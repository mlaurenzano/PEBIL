#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <Vector.h>

class AddressAnchor;
class BinaryInputFile;
class BinaryOutputFile;
class ElfFile;
class X86Instruction;
class RawSection;
class SectionHeader;

class DataReference : public Base {
private:
    uint64_t data;
    uint64_t sectionOffset;
    bool is64bit;

    RawSection* rawSection;
    AddressAnchor* addressAnchor;

public:
    DataReference(uint64_t dat, RawSection* rawsect, uint32_t addrAlign, uint64_t off);
    ~DataReference();

    uint64_t getBaseAddress(); 
    uint64_t getSectionOffset() { return sectionOffset; }
    void initializeAnchor(Base* link);
    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    uint64_t getData() { return data; }

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
    RawSection(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~RawSection();

    virtual uint32_t read(BinaryInputFile* b);
    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify();

    char* charStream(uint32_t offset) { ASSERT(offset < sizeInBytes); return (char*)(rawDataPtr+offset); }
    virtual char* charStream() { return rawDataPtr; }
    char* getFilePointer() { return rawDataPtr; }
    char* getStreamAtAddress(uint64_t addr);
    uint64_t getAddressFromOffset(uint32_t offset);

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    virtual void printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine);
    uint32_t addDataReference(DataReference* dr) { dataReferences.append(dr); return dataReferences.size(); }

    SectionHeader* getSectionHeader();
    uint16_t getSectionIndex() { return sectionIndex; }
    void setSectionIndex(uint16_t newidx) { sectionIndex = newidx; }
    ElfFile* getElfFile() { return elfFile; }

    HashCode getHashCode() { return hashCode; }
};

class DataSection : public RawSection {
protected:
    char* rawBytes;

public:
    DataSection(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~DataSection();

    char* charStream() { return rawBytes; }
    void printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine);
    void printBytes() { printBytes(0,0,0); }

    uint32_t extendSize(uint32_t sz);
    void setBytesAtAddress(uint64_t addr, uint32_t size, char* buff);
    void setBytesAtOffset(uint64_t offset, uint32_t size, char* buff);

    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
};

#endif /* _RawSection_h_ */

