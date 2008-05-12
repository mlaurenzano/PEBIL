#include <SectionHeader.h>
#include <SymbolTable.h>
#include <StringTable.h>
#include <ElfFile.h>
#include <BinaryFile.h>

unsigned char Symbol32::getSymbolBinding(){
    return ELF32_ST_BIND(GET(st_info));
}
unsigned char Symbol64::getSymbolBinding(){
    return ELF64_ST_BIND(GET(st_info));
}
unsigned char Symbol32::getSymbolType(){
    return ELF32_ST_TYPE(GET(st_info));
}
unsigned char Symbol64::getSymbolType(){
    return ELF64_ST_TYPE(GET(st_info));
}

int compareSymbolValue(const void* arg1,const void* arg2){
    return 0;
}

int searchSymbolValue(const void* arg1,const void* arg2){
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

    // idx 0 in the string table is null
    if (!symbols[idx]->GET(st_name)){
        return symbol_without_name;
    } else {
        return stringTable->getString(symbols[idx]->GET(st_name));
    }
}

void SymbolTable::print(){
    PRINT_INFOR("SymbolTable(%d): section %d, %d symbols", index, getSectionIndex(), numberOfSymbols);
    PRINT_INFOR("TYPE(IDX):\tNameIdx\t%24s\t%18s\tSize\tInfo\tBind\tType\tOther\tScnIdx", "Name", "Value");

    for (uint32_t i = 0; i < numberOfSymbols; i++){
        symbols[i]->print(getSymbolName(i));
    }
}


void Symbol::print(char* symbolName){
    char sizeStr[3];

    if (getSizeInBytes() == Size__32_bit_Symbol){
        sprintf(sizeStr,"32");
    } else {
        sprintf(sizeStr,"64");
    }

    PRINT_INFOR("\tSym%s(%d):\t%d\t%24s\t0x%016llx\t%lld\t0x%02hhx\t0x%02hhx\t0x%02hhx\t%hhd\t%hd", sizeStr, index, GET(st_name), symbolName, 
        GET(st_value), GET(st_size), GET(st_info), getSymbolBinding(), getSymbolType(), GET(st_other), GET(st_shndx));

}
