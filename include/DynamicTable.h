#ifndef _DynamicTable_h_
#define _DynamicTable_h_

#include <Base.h>
#include <defines/DynamicTable.d>
#include <RawSection.h>
#include <Vector.h>

class BinaryInputFile;

#define DYNAMIC_ENT_D_UN_IGNORED  0
#define DYNAMIC_ENT_D_UN_IS_D_VAL 1
#define DYNAMIC_ENT_D_UN_IS_D_PTR 2

class Dynamic : public Base {
public:
    uint32_t index;
    char* dynPtr;

    virtual uint32_t read(BinaryInputFile* binaryInputFile) { __SHOULD_NOT_ARRIVE; }
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; }

    char* getDynamicPtr() { return dynPtr; }
    Dynamic(char* dPtr, uint32_t idx);
    ~Dynamic() {}
    void print(char* s);
    virtual void clear() { __SHOULD_NOT_ARRIVE; }

    DYNAMIC_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");
};


class Dynamic32 : public Dynamic {
private:
    Elf32_Dyn entry;
public:
    Dynamic32(char* dPtr, uint32_t idx) : Dynamic(dPtr,idx){}
    ~Dynamic32() {}

    virtual char* charStream() { return (char*)&entry; }

    DYNAMIC_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    void clear() { bzero(charStream(), Size__32_bit_Dynamic_Entry); }
};

class Dynamic64 : public Dynamic {
private:
    Elf64_Dyn entry;
public:
    Dynamic64(char* dPtr, uint32_t idx) : Dynamic(dPtr,idx){}
    ~Dynamic64() {}

    virtual char* charStream() { return (char*)&entry; }

    DYNAMIC_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    void clear() { bzero(charStream(), Size__64_bit_Dynamic_Entry); }
};

class DynamicTable : public RawSection {
protected:
    uint32_t dynamicSize;
    uint16_t segmentIndex;

    Vector<Dynamic*> dynamics;

public:
    DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf);
    ~DynamicTable();

    void print();
    void printSharedLibraries(BinaryInputFile* b);
    uint32_t read(BinaryInputFile* b);
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t findEmptyDynamic();

    Dynamic* getDynamic(uint32_t idx) { return dynamics[idx]; }
    uint32_t getNumberOfDynamics() { return dynamics.size(); }
    uint16_t getSegmentIndex() { return segmentIndex; }
    uint32_t countDynamics(uint32_t type);
    Dynamic* getDynamicByType(uint32_t type, uint32_t idx);
    uint32_t extendTable(uint32_t num);

    void relocateStringTable(uint64_t newAddr);

    virtual bool verify();
};


#endif /* _DynamicTable_h_ */
