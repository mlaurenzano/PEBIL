#ifndef _RelocationTable_h_
#define _RelocationTable_h_

#include <Base.h>
#include <RawSection.h>
#include <defines/RelocationTable.d>
#include <Vector.h>

class ElfFile;
class SectionHeader;
class SymbolTable;

class Relocation : public Base {
protected:
    Relocation(char* relPtr, uint32_t idx) : Base(PebilClassType_Relocation), relocationPtr(relPtr), index(idx) {}
    char* relocationPtr;
    uint32_t index;

public:
    virtual ~Relocation() {}
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual uint32_t read(BinaryInputFile* binaryInputFile) { __SHOULD_NOT_ARRIVE; }
    virtual void print(char*) { __SHOULD_NOT_ARRIVE; }
    virtual uint64_t getSymbol() { __SHOULD_NOT_ARRIVE; }
    virtual uint64_t getType() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { return true; }
    virtual void setSymbolInfo(uint32_t sym) { __SHOULD_NOT_ARRIVE; }

    RELOCATION_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");
};

class Relocation32 : public Relocation {
protected:
    Elf32_Rel entry;
public:
    Relocation32(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~Relocation32() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print(char*);
    uint64_t getSymbol() { return (uint64_t)ELF32_R_SYM (GET(r_info)); }
    uint64_t getType()   { return (uint64_t)ELF32_R_TYPE(GET(r_info)); }
    void setSymbolInfo(uint32_t sym);

    RELOCATION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
};

class Relocation64 : public Relocation {
protected:
    Elf64_Rel entry;
public:
    Relocation64(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~Relocation64() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print(char*);
    void setSymbolInfo(uint32_t sym)
;
    RELOCATION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
};

class RelocationAddend32 : public Relocation {
protected:
    Elf32_Rela entry;
public:
    RelocationAddend32(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~RelocationAddend32() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print(char*);
    uint64_t getSymbol() { return (uint64_t)ELF32_R_SYM (GET(r_info)); }
    uint64_t getType()   { return (uint64_t)ELF32_R_TYPE(GET(r_info)); }
    void setSymbolInfo(uint32_t sym);


    RELOCATION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
    // need a seperate macro set (actually just 1) for the relocation addend structure
    RELOCATIONADDEND_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
};

class RelocationAddend64 : public Relocation {
protected:
    Elf64_Rela entry;
public:
    RelocationAddend64(char* relPtr, uint32_t idx) : Relocation(relPtr,idx) {}
    ~RelocationAddend64() {}
    char* charStream() { return (char*)&entry; }
    uint32_t read(BinaryInputFile* binaryInputFile);
    void print(char*);
    uint64_t getSymbol() { return (uint64_t)ELF64_R_SYM (GET(r_info)); }
    uint64_t getType()   { return (uint64_t)ELF64_R_TYPE(GET(r_info)); }
    void setSymbolInfo(uint32_t sym);

    RELOCATION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
    // need a seperate macro set (actually just 1) for the relocation addend structure
    RELOCATIONADDEND_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");
};

class RelocationTable : public RawSection {
protected:
    ElfRelTypes type;
    SymbolTable* symbolTable;
    RawSection* relocationSection;
    uint32_t index;
    uint32_t relocationSize;

    Vector<Relocation*> relocations;
public:

    RelocationTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~RelocationTable();

    void print();
    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    bool verify();

    uint32_t getNumberOfRelocations() { return relocations.size(); }
    Relocation* getRelocation(uint32_t idx) { return relocations[idx]; }

    ElfFile* getElfFile() { return elfFile; }
    uint32_t getIndex() { return index; }
    uint32_t getRelocationSize() { return relocationSize; }

    void setSymbolTable();
    void setRelocationSection();

    uint32_t addRelocation(uint64_t offset, uint64_t info);
};

#endif

