#ifndef _RelocationTable_h_
#define _RelocationTable_h_

#include <Base.h>
#include <SectionHeader.h>
#include <defines/RelocationTable.d>

class SymbolTable;
class ElfFile;
class SectionHeader;

class Relocation {
protected:
    Relocation(char* relPtr, uint32_t idx) : relocationPtr(relPtr), index(idx) {}
    virtual ~Relocation() {}
    char* relocationPtr;
    uint32_t index;
public:
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual uint32_t read(BinaryInputFile* binaryInputFile) { __SHOULD_NOT_ARRIVE; }
    virtual void print() { __SHOULD_NOT_ARRIVE; }

    RELOCATION_MACROS_BASIS("For the get_X field macros check the defines directory");
};

class Relocation32 : public Relocation {
protected:
    Elf32_Rel entry;
public:
    Relocation32(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~Relocation32() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print();

    RELOCATION_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class Relocation64 : public Relocation {
protected:
    Elf64_Rel entry;
public:
    Relocation64(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~Relocation64() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print();

    RELOCATION_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class RelocationAddend32 : public Relocation {
protected:
    Elf32_Rela entry;
public:
    RelocationAddend32(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~RelocationAddend32() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print();

    RELOCATION_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class RelocationAddend64 : public Relocation {
protected:
    Elf64_Rela entry;
public:
    RelocationAddend64(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~RelocationAddend64() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print();

    RELOCATION_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class RelocationTable : public Base {
protected:
    
    char* relocationTablePtr;
    ElfRelType type;
    SymbolTable* symbolTable;
    SectionHeader* sectionToRelocate;
    ElfFile* elfFile;
    uint32_t index;

    uint32_t numberOfRelocations;
    Relocation** relocations;
    uint32_t relocationSize;
    SectionHeader* sectionHeader;

public:

    RelocationTable(char* ptr, uint32_t sz, uint32_t nr, ElfRelType typ, ElfFile* elf, uint32_t idx, SectionHeader* sh);
    void print();
    uint32_t read(BinaryInputFile* b);

    uint32_t getNumberOfRelocations() { return numberOfRelocations; }

    ElfFile* getElfFile() { return elfFile; }
    uint32_t getSectionIndex() { ASSERT(sectionHeader); return sectionHeader->getIndex(); }
    uint32_t getIndex() { return index; }

    const char* briefName() { return "RelocationTable"; }
//    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

#endif

