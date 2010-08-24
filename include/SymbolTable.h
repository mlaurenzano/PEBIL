/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SymbolTable_h_
#define _SymbolTable_h_

#include <Base.h>
#include <RawSection.h>
#include <defines/SymbolTable.d>
#include <Vector.h>

class ElfFile;
class StringTable;
class SymbolTable;
class TextSection;

static char* symbol_without_name = "<__no_name__pebil>";

extern int compareSymbolValue(const void* arg1,const void* arg2);
extern int searchSymbolValue(const void* arg1,const void* arg2);

class Symbol : public Base {
public:

    uint32_t index;
    char* symbolPtr;
    SymbolTable* table;

    Symbol(SymbolTable* tbl, char* symPtr, uint32_t idx) : Base(PebilClassType_Symbol),table(tbl),symbolPtr(symPtr),index(idx) {}
        ~Symbol(){};

    SYMBOL_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    bool isFunctionSymbol(TextSection* text);
    bool isTextObjectSymbol(TextSection* text);

    void print();
    uint32_t getIndex() { return index; }
    char* getSymbolPtr() { return symbolPtr; }
    bool verify(uint16_t targetSize);
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }
    char* getSymbolName();
    void setIndex(uint32_t idx) { index = idx; }

    virtual unsigned char getSymbolBinding() { __SHOULD_NOT_ARRIVE; }
    virtual unsigned char getSymbolType() { __SHOULD_NOT_ARRIVE; }

    static Symbol* findSymbol(Symbol** symbols,uint32_t symbolCount,uint64_t value);
};

class Symbol32 : public Symbol {
private:
    Elf32_Sym entry;
protected:
public:
    Symbol32(SymbolTable* tbl, char* symPtr, uint32_t idx) : Symbol(tbl, symPtr,idx){ sizeInBytes = Size__32_bit_Symbol; }
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
    Symbol64(SymbolTable* tbl, char* symPtr, uint32_t idx) : Symbol(tbl, symPtr, idx) { sizeInBytes = Size__64_bit_Symbol; }
    ~Symbol64() {}
    char* charStream() { return (char*)&entry; }

    SYMBOL_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    unsigned char getSymbolBinding();
    unsigned char getSymbolType();
};

class SymbolTable : public RawSection {
protected:
    Vector<Symbol*> symbols;
    //    Vector<Symbol*> sortedSymbols;

    StringTable* stringTable;
    uint32_t index;
    bool dynamic;
    uint32_t symbolSize;

    //    bool symbolsAreSorted();
    //    void sortSymbols();
public:

    SymbolTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~SymbolTable();

    uint32_t addSymbol(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t shndx);
    uint32_t getNumberOfSymbols() { return symbols.size(); }

    void print();
    uint32_t read(BinaryInputFile* b);
    bool verify();
    bool isDynamic() { return dynamic; }

    void setStringTable();

    Symbol* getSymbol(uint32_t index) { return symbols[index]; }
    char* getSymbolName(uint32_t index);

    uint32_t getIndex() { return index; }
    StringTable* getStringTable(){ return stringTable; }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t findSymbol4Addr(uint64_t addr, Symbol** buffer, uint32_t bufCnt, char** namestr=NULL);
    void sortForGnuHash(uint32_t firstSymIndex, uint32_t numberOfBuckets);
};

#endif /* _SymbolTable_h_ */
