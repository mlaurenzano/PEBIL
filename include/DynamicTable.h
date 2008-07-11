#ifndef _DynamicTable_h_
#define _DynamicTable_h_

#include <Base.h>
#include <RawSection.h>
#include <BinaryFile.h>
#include <defines/DynamicTable.d>

class Dynamic;

#define DYNAMIC_ENT_D_UN_IGNORED  0
#define DYNAMIC_ENT_D_UN_IS_D_VAL 1
#define DYNAMIC_ENT_D_UN_IS_D_PTR 2


class DynamicTable : public RawSection {
protected:
    uint32_t numberOfDynamics;
    uint32_t dynamicSize;
    uint16_t segmentIndex;

    Dynamic** dynamics;

public:
    DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf);
    ~DynamicTable();

    void print();
    void printSharedLibraries(BinaryInputFile* b);
    uint32_t read(BinaryInputFile* b);
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t findEmptyDynamic();

    Dynamic* getDynamic(uint32_t index);
    uint32_t getNumberOfDynamics() { return numberOfDynamics; }
    uint16_t getSegmentIndex() { return segmentIndex; }
    uint32_t getNumberOfSharedLibraries();
    uint64_t getStringTableAddress();
    uint64_t getHashTableAddress();
    uint64_t getSymbolTableAddress();
    uint64_t getDynamicTableAddress();
    uint32_t getNumberOfRelocationTables();
    uint32_t getRelocationTableAddresses(uint64_t* relocAddresses);

    void relocateStringTable(uint64_t newAddr);

    virtual bool verify();

    const char* briefName() { return "DynamicTable"; }
};


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
};

#endif /* _DynamicTable_h_ */
