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

#include <SymbolTable.h>

#include <BinaryFile.h>
#include <ElfFile.h>
#include <GnuVersion.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>
#include <TextSection.h>

void SymbolTable::sortForGnuHash(uint32_t firstSymIndex, uint32_t numberOfBuckets){
    ASSERT(stringTable);
    ASSERT(dynamic);

    GnuVersymTable* gnuVersymTable = elfFile->getGnuVersymTable();
    ASSERT(gnuVersymTable->getNumberOfSymbols() == symbols.size());

    PRINT_DEBUG_HASH("sorting for gnu hash with nbuckets = %d", numberOfBuckets);

    for (uint32_t i = firstSymIndex; i < symbols.size(); i++){
        PRINT_DEBUG_HASH("symbol[%d] bucketn %d", i, elf_gnu_hash(symbols[i]->getSymbolName()) % numberOfBuckets);
    }

    // used to keep track of the locations of symbols before/after sort, which we can use
    // to correctly reconfigure the relocation tables
    uint32_t sortRep[symbols.size()];
    for (uint32_t i = 0; i < symbols.size(); i++){
        sortRep[i] = i;
    }

    for (uint32_t i = firstSymIndex; i < symbols.size(); i++){
        for (uint32_t j = firstSymIndex; j < symbols.size() - 1; j++){
            uint32_t h1 = elf_gnu_hash(symbols[j]->getSymbolName()) % numberOfBuckets;
            uint32_t h2 = elf_gnu_hash(symbols[j + 1]->getSymbolName()) % numberOfBuckets;
            if (h1 > h2){
                // swap symbols in this table
                Symbol* tmpsym = symbols[j];
                symbols[j] = symbols[j + 1];
                symbols[j]->setIndex(j);
                symbols[j + 1] = tmpsym;
                symbols[j + 1]->setIndex(j + 1);

                // also swap symbol in gnu versym table
                uint16_t tmpv = gnuVersymTable->getSymbol(j);
                gnuVersymTable->setSymbol(j, gnuVersymTable->getSymbol(j + 1));
                gnuVersymTable->setSymbol(j + 1, tmpv);

                uint32_t tmp = sortRep[j];
                sortRep[j] = sortRep[j + 1];
                sortRep[j + 1] = tmp;
            }
        }
    }

    

    PRINT_DEBUG_HASH("symbol mapping:");
    for (uint32_t i = firstSymIndex; i < symbols.size(); i++){
        PRINT_DEBUG_HASH("\t\t%d -> %d", sortRep[i], i);
    }

    bool* doneMod[elfFile->getNumberOfRelocationTables()];
    for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
        doneMod[i] = new bool[elfFile->getRelocationTable(i)->getNumberOfRelocations()];
        bzero(doneMod[i], sizeof(bool) * elfFile->getRelocationTable(i)->getNumberOfRelocations());
    }
    for (uint32_t i = firstSymIndex; i < symbols.size(); i++){
        for (uint32_t k = 0; k < elfFile->getNumberOfRelocationTables(); k++){
            RelocationTable* relocTable = elfFile->getRelocationTable(k);
            if (relocTable->getSectionHeader()->GET(sh_link) == sectionIndex){
                for (uint32_t j = 0; j < relocTable->getNumberOfRelocations(); j++){
                    if (getElfFile()->is64Bit()){
                        if (relocTable->getRelocation(j)->getSymbol() == sortRep[i] &&
                            !doneMod[k][j]){
                            PRINT_DEBUG_HASH("\t\tmodifying relocation %d: %d -> %d", j, sortRep[i], i); 
                            relocTable->getRelocation(j)->setSymbolInfo(i);
                            doneMod[k][j] = true;
                        }
                    } else {
                        if (relocTable->getRelocation(j)->getSymbol() == sortRep[i] &&
                            !doneMod[k][j]){
                            PRINT_DEBUG_HASH("\t\tmodifying relocation %d: %d -> %d", j, sortRep[i], i); 
                            relocTable->getRelocation(j)->setSymbolInfo(i);
                            doneMod[k][j] = true;
                        }
                    }
                }
            }
        }
    } 
    for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
        delete[] doneMod[i];
    }

    for (uint32_t i = firstSymIndex; i < symbols.size(); i++){
        PRINT_DEBUG_HASH("symbol[%d] bucketn %d", i, elf_gnu_hash(symbols[i]->getSymbolName()) % numberOfBuckets);
    }
}

bool Symbol::isTextObjectSymbol(TextSection* text){
    if (getSymbolType() == STT_NOTYPE && GET(st_shndx) == text->getSectionIndex() && 
        text->inRange(GET(st_value))){
        return true;
    }
    return false;
}

bool Symbol::isFunctionSymbol(TextSection* text){
    if (getSymbolType() == STT_FUNC && GET(st_shndx) == text->getSectionIndex()){
        return true;
    }
    return false;
}

char* Symbol::getSymbolName(){
    if (table){
        return table->getSymbolName(index);
    }
    return NULL;
}

int compareSymbolValue(const void* arg1, const void* arg2){
    Symbol* sym1 = *((Symbol**)arg1);
    Symbol* sym2 = *((Symbol**)arg2);

    ASSERT(sym1 && sym2 && "Symbols should exist");

    uint64_t vl1 = sym1->GET(st_value);
    uint64_t vl2 = sym2->GET(st_value);

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    if(vl1 == vl2){
        if (sym1->getSymbolType() < sym2->getSymbolType()){
            return -1;
        }
        if (sym1->getSymbolType() > sym2->getSymbolType()){
            return 1;
        }
    }

    return 0;
}

int searchSymbolValue(const void* arg1,const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    Symbol* sym = *((Symbol**)arg2);

    ASSERT(sym && "Symbol should exist");

    uint64_t val = sym->GET(st_value);

    if(key < val)
        return -1;
    if(key > val)
        return 1;
    return 0;
}

/*
bool SymbolTable::symbolsAreSorted(){
    for (uint32_t i = 0; i < sortedSymbols.size()-1; i++){
        ASSERT(sortedSymbols[i] && sortedSymbols[i+1] && "Symbols should be initialized");
        if (sortedSymbols[i]->GET(st_value) > sortedSymbols[i]->GET(st_value)){
            return false;
        }
    }
    return true;
}

void SymbolTable::sortSymbols(){
    Symbol** symbolArray = new Symbol*[symbols.size()];
    for (uint32_t i = 0; i < symbols.size(); i++){
        symbolArray[i] = symbols[i];
    }

    qsort(symbolArray, symbols.size(), sizeof(Symbol*), compareSymbolValue);
    sortedSymbols.clear();

    for (uint32_t i = 0; i < symbols.size(); i++){
        sortedSymbols.append(symbolArray[i]);
    }
    delete[] symbolArray;
}
*/

uint32_t SymbolTable::findSymbol4Addr(uint64_t addr, Symbol** buffer, uint32_t buffCnt, char** namestr){
    uint32_t retValue = 0;

    // turns out we CANNOT use a sort-by-value scheme with a symbol table because
    // gnu hash tables require a different sorting. so we use a linear search
    for (uint32_t i = 0; i < symbols.size(); i++){
        if (symbols[i]->GET(st_value) == addr){
            if (retValue < buffCnt){
                buffer[retValue++] = symbols[i];
            }
        }
    }

    if (namestr){
        if(!retValue){
            *namestr = new char[__MAX_STRING_SIZE];
            sprintf(*namestr,"<__no_symbol_found>");
        } else {
            char* allnames = new char[__MAX_STRING_SIZE+2];
            *allnames = '\0';
            for(uint32_t i=0;i<retValue;i++){
                char* nm = getSymbolName(buffer[i]->getIndex());
                if((__MAX_STRING_SIZE-strlen(allnames)) > strlen(nm)){
                    sprintf(allnames+strlen(allnames),"%s ",nm);
                } else {
                    sprintf(allnames+strlen(allnames),"?");
                }
            }
            *namestr = allnames;
        }
    }

    return retValue;

    /*
    Symbol** sortedSymbols = new Symbol*[symbols.size()];
    for (uint32_t i = 0; i < symbols.size(); i++){
        sortedSymbols[i] = symbols[i];
    }
    qsort(sortedSymbols, symbols.size(), sizeof(Symbol*), compareSymbolValue);

    void* checkRes = bsearch(&addr,sortedSymbols,symbols.size(),sizeof(Symbol*),searchSymbolValue);

    if (checkRes){

        uint32_t sidx = (((char*)checkRes)-((char*)sortedSymbols))/sizeof(Symbol*);
        uint32_t eidx = sidx;
        for (;eidx < symbols.size();eidx++){
            Symbol* sym = sortedSymbols[eidx];
            if(sym->GET(st_value) != addr){
                break;
            }
        }
        eidx--;
        ASSERT(eidx < symbols.size() && "result of bsearch could not be verified");

        retValue = 0;
        for (;sidx<=eidx;sidx++){
            if(retValue < buffCnt){
                buffer[retValue++] = sortedSymbols[sidx];
            } else {
                break;
            }
        }
    }

    if (namestr){
        if(!retValue){
            *namestr = new char[__MAX_STRING_SIZE];
            sprintf(*namestr,"<__no_symbol_found>");
        } else {
            char* allnames = new char[__MAX_STRING_SIZE+2];
            *allnames = '\0';
            for(uint32_t i=0;i<retValue;i++){
                char* nm = getSymbolName(buffer[i]->getIndex());
                if((__MAX_STRING_SIZE-strlen(allnames)) > strlen(nm)){
                    sprintf(allnames+strlen(allnames),"%s ",nm);
                } else {
                    sprintf(allnames+strlen(allnames),"?");
                }
            }
            *namestr = allnames;
        }
    }

    return retValue;
    */
}

uint32_t SymbolTable::addSymbol(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t shndx){

    if (elfFile->is64Bit()){
        Symbol64* sym = new Symbol64(this, NULL, symbols.size());
        Elf64_Sym symEntry;
        symEntry.st_name = name;
        symEntry.st_value = value;
        symEntry.st_size = size;
        symEntry.st_info = ELF64_ST_INFO(bind,type);
        symEntry.st_other = other;
        symEntry.st_shndx = shndx;

        memcpy(sym->charStream(), &symEntry, Size__64_bit_Symbol);
        symbols.append(sym);
        sizeInBytes += Size__64_bit_Symbol;
    } else {
        Symbol32* sym = new Symbol32(this, NULL, symbols.size());
        Elf32_Sym symEntry;
        symEntry.st_name = name;
        symEntry.st_value = (uint32_t)value;
        symEntry.st_size = (uint32_t)size;
        symEntry.st_info = ELF32_ST_INFO(bind,type);
        symEntry.st_other = other;
        symEntry.st_shndx = shndx;

        memcpy(sym->charStream(), &symEntry, Size__32_bit_Symbol);
        symbols.append(sym);
        sizeInBytes += Size__32_bit_Symbol;
    }

    //    sortSymbols();
    verify();
    return symbols.size()-1;
}


SymbolTable::SymbolTable(char* rawPtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(PebilClassType_SymbolTable,rawPtr,size,scnIdx,elf)
{
    index = idx;
    sizeInBytes = size;

    ASSERT(elfFile && "elfFile should be initialized");
    if (elf->is64Bit()){
        symbolSize = Size__64_bit_Symbol;
    } else {
        symbolSize = Size__32_bit_Symbol;
    }

    SectionHeader *scnHdr = elfFile->getSectionHeader(scnIdx);
    if (scnHdr->GET(sh_type) == SHT_DYNSYM){
        ASSERT(scnHdr->hasAllocBit() && "Dynamic symbol table must be have alloc attribute");
        dynamic = 1;
    } else {
        dynamic = 0;
    }

    verify();
}

SymbolTable::~SymbolTable(){
    for (uint32_t i = 0; i < symbols.size(); i++){
        if (symbols[i]){
            delete symbols[i];
        }
    }
}


unsigned char Symbol32::getSymbolBinding(){
    return ELF32_ST_BIND(entry.st_info);
}
unsigned char Symbol64::getSymbolBinding(){
    return ELF64_ST_BIND(entry.st_info);
}
unsigned char Symbol32::getSymbolType(){
    return ELF32_ST_TYPE(entry.st_info);
}
unsigned char Symbol64::getSymbolType(){
    return ELF64_ST_TYPE(entry.st_info);
}

bool SymbolTable::verify(){

    if (sizeInBytes % symbolSize != 0){
        PRINT_ERROR("Symbol table section must have size n * %d", symbolSize);
        return false;
    }

    /*
    if (symbols.size() != sortedSymbols.size() && sortedSymbols.size()){
        PRINT_ERROR("There are a different number of symbols and sortedSymbols");
        return false;
    }
    */

    for (uint32_t i = 0; i < symbols.size(); i++){
        if (!symbols[i]){
            PRINT_ERROR("Symbol not allocated");
            return false;
        }
    }

    // STT_FILE: By convention the symbol's name gives the name of the source file associated with the object file. 
    // A file symbol has STB_LOCAL bindings, its section index is  SHN_ABS, and it precedes the other STB_LOCAL 
    // symbols of the file, if it is present.

    uint32_t fileSymbolIdx = symbols.size();
    uint32_t firstLocalSym = symbols.size();
    for (uint32_t i = 1; i < symbols.size(); i++){
        if (symbols[i]->getSymbolBinding() == STB_LOCAL && firstLocalSym == symbols.size()){
            firstLocalSym = i;
        }
        if (symbols[i]->getSymbolType() == STT_FILE){
            if (symbols[i]->GET(st_shndx) != SHN_ABS){
                PRINT_ERROR("File symbols must use absolute addressing");
                return false;
            }
            if (symbols[i]->getSymbolBinding() != STB_LOCAL){
                PRINT_ERROR("A file symbol must have local binding");
                return false;
            }
        }
        if (symbols[i]->getSymbolType() >= STT_LOPROC && symbols[i]->getSymbolType() <= STT_HIPROC){
            PRINT_ERROR("Symbol has a type that is reserved for precessor-specific semantics");
            return false;
        }

    }
    return true;
}


void SymbolTable::setStringTable(){
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(getSectionIndex()));
    SectionHeader* sh = elfFile->getSectionHeader(getSectionIndex());
    ASSERT(elfFile->getRawSection(sh->GET(sh_link)));

    RawSection* st = elfFile->getRawSection(sh->GET(sh_link));
    ASSERT(st->getType() == PebilClassType_StringTable);

    stringTable = (StringTable*)st;
    ASSERT(stringTable);
}


bool Symbol::verify(uint16_t targetSize){
    return true;
}

uint32_t Symbol32::read(BinaryInputFile* binaryInputFile){

    binaryInputFile->setInPointer(symbolPtr);
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Symbol)){
        PRINT_ERROR("Symbol (32) can not be read");
    }

    verify(Size__32_bit_Symbol);

    return sizeInBytes;    
}

uint32_t Symbol64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(symbolPtr);
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Symbol)){
        PRINT_ERROR("Symbol (64) can not be read");
    }

    verify(Size__64_bit_Symbol);

    return sizeInBytes;    
}

uint32_t SymbolTable::read(BinaryInputFile* binaryInputFile){
    
    binaryInputFile->setInPointer(getFilePointer());
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t totalBytesRead = 0;

    uint32_t numberOfSymbols = sizeInBytes / symbolSize;
    for (uint32_t i = 0; i < numberOfSymbols; i++){
        if (elfFile->is64Bit()){
            symbols.append(new Symbol64(this, getFilePointer() + (i * Size__64_bit_Symbol), i));
        } else {
            symbols.append(new Symbol32(this, getFilePointer() + (i * Size__32_bit_Symbol), i));
        }
        totalBytesRead += symbols[i]->read(binaryInputFile);
    }

    ASSERT(sizeInBytes == totalBytesRead && "size read from file does not match theorietical size of Symbol Table");
    return sizeInBytes;
}

char* SymbolTable::getSymbolName(uint32_t idx){
    ASSERT(stringTable && "String Table should be initialized");

    // idx 0 in the string table is null
    if (!symbols[idx]->GET(st_name)){
        return symbol_without_name;
    } else {
        return stringTable->getString(symbols[idx]->GET(st_name));
    }
}

void SymbolTable::print(){
    char tmpstr[__MAX_STRING_SIZE];
    PRINT_INFOR("SymbolTable : %d aka sect %d with %d symbols",index,getSectionIndex(),symbols.size());
    PRINT_INFOR("\tdyn? : %s", isDynamic() ? "yes" : "no");
    for (uint32_t i = 0; i < symbols.size(); i++){
        symbols[i]->print();
    }
}

void SymbolTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;

    for (uint32_t i = 0; i < symbols.size(); i++){
        if (elfFile->is64Bit()){
            binaryOutputFile->copyBytes(getSymbol(i)->charStream(),Size__64_bit_Symbol,offset+currByte);
            currByte += Size__64_bit_Symbol;
        } else {
            binaryOutputFile->copyBytes(getSymbol(i)->charStream(),Size__32_bit_Symbol,offset+currByte);
            currByte += Size__32_bit_Symbol;
        }
    }

    ASSERT(currByte == sizeInBytes && "Number of bytes written should be the same as the size of the section");
}



void Symbol::print(){

    char* bindstr = "UNK";
    switch(ELF32_ST_BIND(GET(st_info))){
        case STB_LOCAL: bindstr="LOC";break;
        case STB_GLOBAL:bindstr="GLB";break;
        case STB_WEAK:  bindstr="WEA";break;
        case STB_NUM:   bindstr="NUM";break;
        case STB_LOOS:  bindstr="LOS";break;
        case STB_HIOS:  bindstr="HIS";break;
        case STB_LOPROC:bindstr="LOP";break;
        case STB_HIPROC:bindstr="HIP";break;
        default: break;  
    }

    char* typestr = "UNK";
    switch(ELF32_ST_TYPE(GET(st_info))){
        case STT_NOTYPE: typestr="NOTY";break;
        case STT_OBJECT: typestr="OBJT";break;
        case STT_FUNC:   typestr="FUNC";break;
        case STT_SECTION:typestr="SECT";break;
        case STT_FILE:   typestr="FILE";break;
        case STT_COMMON: typestr="COMM";break;
        case STT_TLS:    typestr="TLS ";break;
        case STT_NUM:    typestr="NUM ";break;
        case STT_LOOS:   typestr="LOOS";break;
        case STT_HIOS:   typestr="HIOS";break;
        case STT_LOPROC: typestr="LOPR";break;
        case STT_HIPROC: typestr="HIPR";break;
        default:break;
    }

    char* symbolName = table->getSymbolName(index);

    PRINT_INFOR("\tsym%5d -- sx:%5d sz:%8lld bxt: %3s.%4s vl:%#12llx ot:%3d nx:%8u\t%s",index,
            GET(st_shndx),GET(st_size),bindstr,typestr,GET(st_value),GET(st_other),
            GET(st_name),symbolName ? symbolName : "");
}


