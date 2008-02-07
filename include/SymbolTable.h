#ifndef _SymbolTable_h_
#define _SymbolTable_h_

#include <Base.h>
#include <RawSection.h>
#include <ElfFile.h>
#include <defines/SymbolTable.d>

class StringTable;
class ElfFile;

class Symbol : public Base {
public:

    uint32_t index;
    char* symbolPtr;

    Symbol(char* symPtr, uint32_t idx) : symbolPtr(symPtr),index(idx) {}
    ~Symbol() {}

    SYMBOL_MACROS_BASIS("For the get_X field macros check the defines directory");

    uint32_t getIndex() { return index; }
    char* getSymbolPtr() { return symbolPtr; }
    bool verify(uint16_t targetSize);

    virtual unsigned char getSymbolBinding() { __SHOULD_NOT_ARRIVE; }
    virtual unsigned char getSymbolType() { __SHOULD_NOT_ARRIVE; }

    static Symbol* findSymbol(Symbol** symbols,uint32_t symbolCount,uint64_t value);
};

class Symbol32 : public Symbol {
private:
    Elf32_Sym entry;
protected:
public:
    Symbol32(char* symPtr, uint32_t idx) : Symbol(symPtr,idx){}
    ~Symbol32() {}
    char* charStream() { return (char*)&entry; }

    SYMBOL_MACROS_CLASS("For the get_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    unsigned char getSymbolBinding();
    unsigned char getSymbolType();
};

class Symbol64 : public Symbol {
private:
    Elf64_Sym entry;
protected:
public:
    Symbol64(char* symPtr, uint32_t idx) : Symbol(symPtr, idx) {}
    ~Symbol64() {}
    char* charStream() { return (char*)&entry; }

    SYMBOL_MACROS_CLASS("For the get_X field macros check the defines directory");

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

public:

    SymbolTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(ElfClassTypes_symbol_table,rawPtr,size,scnIdx,elf),index(idx)
    {
        sizeInBytes = size;

        uint32_t symbolSize;

        ASSERT(elfFile);
        if (elf->is64Bit()){
            symbolSize = Size__64_bit_Symbol;
        } else {
            symbolSize = Size__32_bit_Symbol;
        }

        ASSERT(sizeInBytes % symbolSize == 0 && "Symbol table section must have size n*symbolSize");
        numberOfSymbols = sizeInBytes / symbolSize;

        symbols = new Symbol*[numberOfSymbols];
    }



    ~SymbolTable();

    uint32_t getNumberOfSymbols() { return numberOfSymbols; }

    void print();
    void printSymbol(uint32_t index);
    void printSymbol32(uint32_t index);
    void printSymbol64(uint32_t index);
    uint32_t read(BinaryInputFile* b);
    bool verify();

    uint16_t setStringTable();

    Symbol* getSymbol(uint32_t index) { ASSERT(index < numberOfSymbols); return symbols[index]; }
    char* getSymbolName(uint32_t index);

    uint32_t getIndex() { return index; }

    const char* briefName() { return "SymbolTable"; }
};

#endif /* _SymbolTable_h_ */
