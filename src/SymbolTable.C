#include <SectionHeader.h>
#include <SymbolTable.h>
#include <StringTable.h>
#include <ElfFile.h>
#include <BinaryFile.h>

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

int compareSymbolValue(const void* arg1,const void* arg2){
/*
    Symbol* sym1 = *((Symbol**)arg1);
    Symbol* sym2 = *((Symbol**)arg2);
    uint64_t vl1 = sym1->GET(n_value);
    uint64_t vl2 = sym2->GET(n_value);

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;

    ASSERT((sym1->GET(n_scnum) == sym2->GET(n_scnum)) && 
           "FATAL: Two symbols with the same address but different sections");

*/
    return 0;
}

int searchSymbolValue(const void* arg1,const void* arg2){
/*
    uint64_t key = *((uint64_t*)arg1);
    Symbol* sym = *((Symbol**)arg2);
    uint64_t val = sym->GET(n_value);

    if(key < val)
        return -1;
    if(key > val)
        return 1;
*/
    return 0;
}

bool SymbolTable::verify(){

    for (uint32_t i = 0; i < numberOfSymbols; i++){
        if (!symbols[i]){
            PRINT_ERROR("Symbol not allocated");
            return false;
        }
    }

    // STT_FILE: By convention the symbol's name gives the name of the source file associated with the object file. 
    // A file symbol has STB_LOCAL bindings, its section index is  SHN_ABS, and it precedes the other STB_LOCAL 
    // symbols of the file, if it is present.

    uint32_t fileSymbolIdx = numberOfSymbols;
    uint32_t firstLocalSym = numberOfSymbols;
    for (uint32_t i = 0; i < numberOfSymbols; i++){
        if (symbols[i]->getSymbolBinding() == STB_LOCAL && firstLocalSym == numberOfSymbols){
            firstLocalSym = i;
        }
        if (symbols[i]->getSymbolType() == STT_FILE){
            if (symbols[i]->GET(st_shndx) != SHN_ABS){
                PRINT_ERROR("File symbols must use absolute addressing");
                return false;
            }
            if (fileSymbolIdx < numberOfSymbols){
                PRINT_ERROR("Only one symbol of type STT_FILE allowed in a symbol table");
                return false;
            }
            if (symbols[i]->getSymbolBinding() == STB_LOCAL && i > firstLocalSym){
                PRINT_ERROR("If a file symbol has local binding, it must precede all other symbols with local binding");
                return false;
            }
            fileSymbolIdx = i;
        }
    }
    return true;
}


uint16_t SymbolTable::setStringTable(){
    ASSERT(elfFile);
    ASSERT(elfFile->getSectionHeader(getSectionIndex()));
    SectionHeader* sh = elfFile->getSectionHeader(getSectionIndex());
    ASSERT(elfFile->getRawSection(sh->GET(sh_link)));

    RawSection* st = elfFile->getRawSection(sh->GET(sh_link));
    ASSERT(st->getType() == ElfClassTypes_string_table);
    stringTable = (StringTable*)st;
    return stringTable->getSectionIndex();
}

SymbolTable::~SymbolTable(){
/*
    for(uint32_t i=0;i<numberOfSymbols;i++)
        delete symbols[i];
    delete[] symbols;
 */
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

    //    PRINT_INFOR("Reading %d symbols for symtable %d", numberOfSymbols, index);

    for (uint32_t i = 0; i < numberOfSymbols; i++){
        if (elfFile->is64Bit()){
            symbols[i] = new Symbol64(getFilePointer() + (i * Size__64_bit_Symbol), i);
        } else {
            symbols[i] = new Symbol32(getFilePointer() + (i * Size__32_bit_Symbol), i);
        }
        symbols[i]->read(binaryInputFile);
    }

    return sizeInBytes;
}

char* SymbolTable::getSymbolName(uint32_t idx){
    ASSERT(stringTable);
    ASSERT(symbols[idx]);
    return stringTable->getString(symbols[idx]->GET(st_name));
}

void SymbolTable::print(){
    PRINT_INFOR("SYMBOLTABLE(%d): %d symbols, section(%d)", index, numberOfSymbols, getSectionIndex());
    PRINT_INFOR("TYPE(IDX):\tVALUE\tSIZE\tINFO\tOTHER\tSCNIDX\tNAMEIDX\tNAME");
    for (uint32_t i = 0; i < numberOfSymbols; i++){
        printSymbol(i);
    }
}

void SymbolTable::printSymbol64(uint32_t idx){
    ASSERT(idx < numberOfSymbols && "idx out of symbols[] bounds");
    ASSERT(symbols[idx]);
    Symbol32* sym = (Symbol32*)symbols[idx];

    PRINT_INFOR("\tSYM64(%d):\t%d\t%24s\t%#16x\t%d\t%d\t%d\t%d", idx, sym->GET(st_name), getSymbolName(idx), 
        sym->GET(st_value), sym->GET(st_size), sym->GET(st_info), sym->GET(st_other), sym->GET(st_shndx));
}

void SymbolTable::printSymbol32(uint32_t idx){
    ASSERT(idx < numberOfSymbols && "idx out of symbols[] bounds");
    ASSERT(symbols[idx]);
    Symbol32* sym = (Symbol32*)symbols[idx];

//    PRINT_INFOR("going to use name offset %d for symbol %d", sym->GET(st_name), idx);
    PRINT_INFOR("\tSYM32(%d):\t%d\t%24s\t%#16x\t%d\t%d\t%d\t%d", idx, sym->GET(st_name), getSymbolName(idx), 
        sym->GET(st_value), sym->GET(st_size), sym->GET(st_info), sym->GET(st_other), sym->GET(st_shndx));

/*
    PRINT_INFOR("\tSYM32(%d): %#9x\t%d\t%d\t%d\t%d\t%d\t%s", idx, sym->GET(st_value), sym->GET(st_size), sym->GET(st_info),
        sym->GET(st_other), sym->GET(st_shndx), sym->GET(st_name), getSymbolName(idx));
*/
}

void SymbolTable::printSymbol(uint32_t idx){

    ASSERT(elfFile);
    if (elfFile->is64Bit()){
        printSymbol64(idx);
    } else {
        printSymbol32(idx);
    }

}
