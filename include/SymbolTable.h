#ifndef _SymbolTable_h_
#define _SymbolTable_h_

#include <Base.h>
#include <RawSection.h>
#include <ElfFile.h>
#include <SectionHeader.h>
#include <defines/SymbolTable.d>

class StringTable;
class ElfFile;

static char* symbol_without_name = "<__no_name__x86_instrumentor>";

class Symbol : public Base {
public:

    uint32_t index;
    char* symbolPtr;

    Symbol(char* symPtr, uint32_t idx) : Base(ElfClassTypes_Symbol),symbolPtr(symPtr),index(idx) {}
        ~Symbol(){};

    SYMBOL_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    void print(char* symbolName);
    uint32_t getIndex() { return index; }
    char* getSymbolPtr() { return symbolPtr; }
    bool verify(uint16_t targetSize);
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    virtual unsigned char getSymbolBinding() { __SHOULD_NOT_ARRIVE; }
    virtual unsigned char getSymbolType() { __SHOULD_NOT_ARRIVE; }

    static Symbol* findSymbol(Symbol** symbols,uint32_t symbolCount,uint64_t value);
};

class Symbol32 : public Symbol {
private:
    Elf32_Sym entry;
protected:
public:
    Symbol32(char* symPtr, uint32_t idx) : Symbol(symPtr,idx){ sizeInBytes = Size__32_bit_Symbol; }
    ~Symbol32() {}
    char* charStream() { return (char*)&entry; }

    SYMBOL_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    unsigned char getSymbolBinding();
    unsigned char getSymbolType();
};

class Symbol64 : public Symbol {
private:
    Elf64_Sym entry;
protected:
public:
    Symbol64(char* symPtr, uint32_t idx) : Symbol(symPtr, idx) { sizeInBytes = Size__64_bit_Symbol; }
    ~Symbol64() {}
    char* charStream() { return (char*)&entry; }

    SYMBOL_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    unsigned char getSymbolBinding();
    unsigned char getSymbolType();
};

class SymbolTable : public RawSection {
protected:
    uint32_t numberOfSymbols;
    Symbol** symbols;
    StringTable* stringTable;
    uint32_t index;
    bool dynamic;

    Symbol** sortedSymbols;
public:

    SymbolTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~SymbolTable();

    uint32_t addSymbol(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t shndx);
    uint32_t getNumberOfSymbols() { return numberOfSymbols; }

    void print();
    uint32_t read(BinaryInputFile* b);
    bool verify();
    bool isDynamic() { return dynamic; }

    uint16_t setStringTable();

    Symbol* getSymbol(uint32_t index) { ASSERT(index < numberOfSymbols); return symbols[index]; }
    char* getSymbolName(uint32_t index);

    uint32_t getIndex() { return index; }
    StringTable* getStringTable(){ return stringTable; }

    const char* briefName() { return "SymbolTable"; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t findSymbol4Addr(uint64_t addr,Symbol** buffer,uint32_t bufCnt,char** namestr=NULL);
};

#endif /* _SymbolTable_h_ */
