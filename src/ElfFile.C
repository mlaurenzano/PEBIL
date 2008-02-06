#include <FileHeader.h>
#include <ProgramHeader.h>
#include <SectionHeader.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <RelocationTable.h>

TIMER(
	extern double cfg_s1;
	extern double cfg_s2;
	extern double cfg_s3;
	extern double cfg_s4;
	extern double cfg_s5;
);

DEBUG(
uint32_t readBytes = 0;
);

#include <BitSet.h>

bool ElfFile::verify(){

        // enforce constrainst on where PT_INTERP segments fall
    uint32_t ptInterpIdx = numberOfPrograms;
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if (programHeaders[i]->GET(p_type) == PT_INTERP){
            if (ptInterpIdx < numberOfPrograms){
                PRINT_ERROR("Cannot have multiple PT_INTERP segments");
                return false;
            }
            ptInterpIdx = i;
        }
        if (programHeaders[i]->GET(p_type) == PT_LOAD){
            if (ptInterpIdx < numberOfPrograms){
                PRINT_ERROR("PT_INTERP segment must preceed any loadable segment");
                return false;
            }
        }
    }

    uint32_t hashSectionCount = 0;
    uint32_t dynlinkSectionCount = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_HASH){
            hashSectionCount++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_DYNAMIC){
            dynlinkSectionCount++;
        }
    }
        // enfoce that an elf file can have only one hash section
    if (hashSectionCount > 1){
        PRINT_ERROR("Elf file cannot have more than one hash section");
        return false;
    }
        // enfoce that an elf file can have only one dynamic linking section
    if (dynlinkSectionCount > 1){
        PRINT_ERROR("Elf file cannot have more than one dynamic linking section");
        return false;
    }

}

/*
void ElfFile::testBitSet(){
    PRINT_INFOR("Testing BitSet functionality");
    BitSet<BasicBlock*>** foo = new BitSet<BasicBlock*>*[3];

    foo[0] = new BitSet<BasicBlock*>(23);
    foo[1] = new BitSet<BasicBlock*>(23);
    foo[0]->print();
    foo[0]->setall();
    foo[0]->print();

    foo[0]->remove(0);
    foo[0]->remove(1);
    foo[1]->insert(1);
    foo[1]->insert(3);

    foo[0]->print();
    foo[1]->print();

    *foo[0] |= *foo[1];
    
    foo[0]->print();
    foo[1]->print();
}
*/

void ElfFile::parse(){

    TIMER(double t1 = timer());	

    binaryInputFile.readFileInMemory(elfFileName); 

    unsigned char e_ident[EI_NIDENT];
    bzero(&e_ident,(sizeof(unsigned char) * EI_NIDENT));

    if(!binaryInputFile.copyBytes(&e_ident,(sizeof(unsigned char) * EI_NIDENT))){
        PRINT_ERROR("The magic number can not be read\n");
    }
 
    if (ISELFMAGIC(e_ident[EI_MAG0],e_ident[EI_MAG1],e_ident[EI_MAG2],e_ident[EI_MAG3])){
    } else {
        PRINT_ERROR("The magic number [%#x%#x%#x%#x] is not a valid one",e_ident[EI_MAG0],e_ident[EI_MAG1],e_ident[EI_MAG2],e_ident[EI_MAG3]);
    }

    if(ISELF64BIT(e_ident[EI_CLASS])){
        PRINT_INFOR("The executable is 64-bit");
        is64BitFlag = true;
    } else if(ISELF32BIT(e_ident[EI_CLASS])){
        PRINT_INFOR("The executable is 32-bit");
    } else {
        PRINT_ERROR("The class identifier is not a valid one [%#x]",e_ident[EI_CLASS]);
    }

    readFileHeader();
    readProgramHeaders();
    readSectionHeaders();
    readStringTables();
    readSymbolTables();
    readRelocationTables();
    readRawSections();

}

void ElfFile::readFileHeader() {

    PRINT_INFOR("Parsing the header");
    if(is64Bit()){
        fileHeader = new FileHeader64();
    } else {
        fileHeader = new FileHeader32();
    }
    ASSERT(fileHeader);
    fileHeader->read(&binaryInputFile);
    DEBUG(
        readBytes += fileHeader->getSizeInBytes();
        ASSERT(binaryInputFile.alreadyRead() == readBytes);
    );

    numberOfSections = fileHeader->GET(e_shnum);
    ASSERT(numberOfSections && "FATAL : This file has no sections!!!!!");

    numberOfPrograms = fileHeader->GET(e_phnum);
    ASSERT(numberOfSections && "FATAL : This file has no programs!!!!!");

}

void ElfFile::readSectionHeaders(){
    PRINT_INFOR("Parsing the section header table");
    sectionHeaders = new SectionHeader*[numberOfSections];
    ASSERT(sectionHeaders);

    binaryInputFile.setInPointer(fileHeader->getSectionHeaderTablePtr());
    PRINT_INFOR("Found %d section header entries, reading at location %#x\n", numberOfSections, binaryInputFile.currentOffset());

    for (uint32_t i = 0; i < numberOfSections; i++){
        if(is64Bit()){
            sectionHeaders[i] = new SectionHeader64(i);
        } else {
            sectionHeaders[i] = new SectionHeader32(i);
        }
        ASSERT(sectionHeaders[i]);
        sectionHeaders[i]->read(&binaryInputFile);
        DEBUG(
            readBytes += sectionHeaders[i]->getSizeInBytes();
            ASSERT(binaryInputFile.alreadyRead() == readBytes);
            PRINT_DEBUG("read %d bytes for section header %d", sectionHeaders[i]->getSizeInBytes(), i);
        );
    }
}

void ElfFile::readProgramHeaders(){

    PRINT_INFOR("Parsing the program header table");
    programHeaders = new ProgramHeader*[numberOfPrograms];
    ASSERT(programHeaders);

    binaryInputFile.setInPointer(fileHeader->getProgramHeaderTablePtr());
    PRINT_INFOR("Found %d program header entries, reading at location %#x\n", numberOfPrograms, binaryInputFile.currentOffset());

    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if(is64Bit()){
            programHeaders[i] = new ProgramHeader64(i);
        } else {
            programHeaders[i] = new ProgramHeader32(i);
        }
        ASSERT(programHeaders[i]);
        programHeaders[i]->read(&binaryInputFile);
        DEBUG(
            readBytes += programHeaders[i]->getSizeInBytes();
            ASSERT(binaryInputFile.alreadyRead() == readBytes);
        );
    }
}

uint16_t ElfFile::setSectionNameStrTabIdx(){
    ASSERT(fileHeader->GET(e_shstrndx) && "No section name string table");

    for (uint32_t i = 0; i < numberOfStringTables; i++){
        if ((stringTables[i]->getSectionHeader())->getIndex() == fileHeader->GET(e_shstrndx)){
            sectionNameStrTabIdx = i;
            return sectionNameStrTabIdx;
        }
    }

    ASSERT(0 && "should have found section header strtab");
}

void ElfFile::setSectionNames(){
    PRINT_INFOR("Reading the scnhdr string table");

    ASSERT(sectionHeaders && "Section headers not present");
    ASSERT(sectionNameStrTabIdx && "Section header string table index must be defined");

    char* stringTablePtr = getStringTable(sectionNameStrTabIdx)->getStringTablePtr();
    uint64_t stringTableSize = getStringTable(sectionNameStrTabIdx)->getStringTableSize();

    PRINT_INFOR("String table is located at %#x and has %d bytes", stringTablePtr, stringTableSize);
    PRINT_INFOR("Setting section header names from string table");

    // skip first section header since it is reserved and its values are null
    for (uint32_t i = 1; i < numberOfSections; i++){
        ASSERT(sectionHeaders[i]->getSectionNamePtr() == NULL && "Section Header name shouldn't already be set");
        uint32_t sectionNameOffset = sectionHeaders[i]->GET(sh_name);
        ASSERT(sectionNameOffset && "Section header name should be in string table");
        sectionHeaders[i]->setSectionNamePtr(stringTablePtr + sectionNameOffset);
    }
}

void ElfFile::readStringTables(){
    if (!sectionHeaders){
        return;
    }
    ASSERT(numberOfStringTables == 0 && "string tables should not have been set yet");

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_STRTAB){
            numberOfStringTables++;
        }
    }

    PRINT_INFOR("Found %d string tables", numberOfStringTables);
    ASSERT(numberOfStringTables && "File contains no string tables");

    stringTables = new StringTable*[numberOfStringTables];
    uint32_t currIdx = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_STRTAB){

            PRINT_INFOR("\tSetting string table %d", currIdx);
            ASSERT(currIdx < numberOfStringTables);

            char* stringTablePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
            uint64_t stringTableSize = (uint64_t)sectionHeaders[i]->GET(sh_size);

            stringTables[currIdx] = new StringTable(stringTablePtr, stringTableSize, currIdx, sectionHeaders[i]);
            stringTables[currIdx]->read(&binaryInputFile);
            currIdx++;
        }
    }

    setSectionNameStrTabIdx();
    ASSERT(sectionNameStrTabIdx != SHN_UNDEF && "File has no string table");
    ASSERT(sectionNameStrTabIdx <= numberOfSections && "String Table section idx out of section header table boundary");
    setSectionNames();
}

void ElfFile::readSymbolTables(){
    if (!sectionHeaders){
        return;
    }
    ASSERT(numberOfSymbolTables == 0 && "string tables should not have been set yet");

    uint32_t numberOfShtSymtab = 0;
    uint32_t numberOfShtDynsym = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_SYMTAB){
            numberOfSymbolTables++;
            numberOfShtSymtab++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_DYNSYM){
            numberOfSymbolTables++;
            numberOfShtDynsym++;
        }
    }

    ASSERT(numberOfShtSymtab <= MAX_SHT_SYMTAB_COUNT && "Cannot have multiple sections of this type");
    ASSERT(numberOfShtDynsym <= MAX_SHT_DYNSYM_COUNT && "Cannot have multiple sections of this type");

    ASSERT(numberOfSymbolTables && "No symbol table sections found");

    symbolTables = new SymbolTable*[numberOfSymbolTables];
    uint32_t currIdx = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_SYMTAB || sectionHeaders[i]->GET(sh_type) == SHT_DYNSYM){

            ASSERT(currIdx < numberOfSymbolTables);

            uint64_t symbolTableSize = (uint64_t)sectionHeaders[i]->GET(sh_size);
            char* symbolTablePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
            uint32_t numberOfSymbols = 0;
            uint16_t strtabSectionIdx = sectionHeaders[i]->GET(sh_link);
            uint32_t lastLocalIdx = sectionHeaders[i]->GET(sh_info);

            if (is64Bit()){
                ASSERT(symbolTableSize % sizeof(Elf64_Sym) == 0 && "Symbol table must be just symbols");
                numberOfSymbols = symbolTableSize / sizeof(Elf64_Sym);
            } else {
                ASSERT(symbolTableSize % sizeof(Elf32_Sym) == 0 && "Symbol table must be just symbols");
                numberOfSymbols = symbolTableSize / sizeof(Elf32_Sym);
            }
            symbolTables[currIdx] = new SymbolTable(symbolTablePtr, numberOfSymbols, strtabSectionIdx, lastLocalIdx, currIdx, this, sectionHeaders[i]);
            symbolTables[currIdx]->read(&binaryInputFile);
            currIdx++;
        }
    }

}

void ElfFile::readRelocationTables(){
    ASSERT(numberOfRelocationTables == 0 && "Relocation tables should not have been read yet");
    PRINT_INFOR("Parsing relocation tables");

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_REL){
            numberOfRelocationTables++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_RELA){
            numberOfRelocationTables++;
        }
    }

    relocationTables = new RelocationTable*[numberOfRelocationTables];
    PRINT_INFOR("Found %d relocation tables", numberOfRelocationTables);

    numberOfRelocationTables = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){

        char* relocationTablePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
        uint64_t relocationTableSize = sectionHeaders[i]->GET(sh_size);
        uint32_t sz;
        ElfRelType typ;

        if (sectionHeaders[i]->GET(sh_type) == SHT_REL){
            typ = ElfRelType_rel;
            if (is64Bit()){
                sz = Size__64_bit_Relocation;
            } else {
                sz = Size__32_bit_Relocation;
            }
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_RELA){
            typ = ElfRelType_rela;
            if (is64Bit()){
                sz = Size__64_bit_Relocation_Addend;
            } else {
                sz = Size__32_bit_Relocation_Addend;
            }
        }

        if (sectionHeaders[i]->GET(sh_type) == SHT_REL || sectionHeaders[i]->GET(sh_type) == SHT_RELA){
            ASSERT(relocationTableSize % sz == 0 && "Relocation table must contain only relocation entries");
            relocationTables[numberOfRelocationTables] = new RelocationTable(relocationTablePtr,sz,(relocationTableSize/sz),typ,this,numberOfRelocationTables,sectionHeaders[i]);
            relocationTables[numberOfRelocationTables]->read(&binaryInputFile);
            numberOfRelocationTables++;
        }

    }    
}

void ElfFile::readRawSections(){
}

void ElfFile::processOverflowSections(){
/*
    PRINT_INFOR("Processing the overflow sections");
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(sectionHeaders[i]->IS_SECT_TYPE(OVRFLO)){
            uint32_t whichSection = sectionHeaders[i]->GET(s_nreloc);
            ASSERT((whichSection <= numberOfSections) && "FATAL : Which section is this??\n");
            ASSERT(!sectionHeaders[whichSection]->getOverFlowSection() && "FATAL : More than 1 overflow??\n");
            sectionHeaders[whichSection]->setOverFlowSection(sectionHeaders[i]);
        }
    }
*/
}

void ElfFile::readRawSectionData(){
/*
    PRINT_INFOR("Parsing the raw section data");

    rawSections = new RawSection*[numberOfSections + 1];
    ASSERT(rawSections);

    rawSections[0] = NULL;
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSections[i] = RawSection::newRawSection(sectionHeaders[i],this);
        if(rawSections[i]){
            rawSections[i]->read(&binaryInputFile);
        }
    }
*/
}

/*
void ElfFile::readSymbolStringTable(DebugSection* debugRawSect){

    PRINT_INFOR("Parsing the string and symbol tables");

    uint32_t numberOfSyms = fileHeader->GET(f_nsyms);
    if(numberOfSyms){
        char* startPtr = fileHeader->getSymbolTablePtr();
        ASSERT(startPtr && "FATAL : Somehow the symbol table exists but the data does not");

        char* stringTablePtr = startPtr + (numberOfSyms * Size__NN_bit_SymbolTable_Entry);
        if(binaryInputFile.isInBuffer(stringTablePtr)){
            stringTable = new StringTable(stringTablePtr);
            stringTable->read(&binaryInputFile);
        } else {
            ASSERT(NULL && "FATAL : Some how the string table does not exist");
        }

        symbolTable = new SymbolTable(startPtr,numberOfSyms,this);
        symbolTable->setStringTable(stringTable);
        symbolTable->setDebugSection(debugRawSect);
        symbolTable->read(&binaryInputFile);
    } else {
        ASSERT(numberOfSyms && "FATAL : At this moment we can not handle executables with no symbol table");
    }

}
*/

void ElfFile::readRelocLineInfoTable(){
/*
    PRINT_INFOR("Parsing the LineInformation and Relocation tables");

    processOverflowSections();

    for(uint32_t i=1;i<=numberOfSections;i++){
        RelocationTable* relocationTable = sectionHeaders[i]->readRelocTable(&binaryInputFile,this);
        if(relocationTable){
            relocationTable->setSymbolTable(symbolTable);
        }
        LineInfoTable* lineInfoTable = sectionHeaders[i]->readLineInfoTable(&binaryInputFile,this);
        if(lineInfoTable){
            lineInfoTable->setSymbolTable(symbolTable);
        }
    }
*/
}

void ElfFile::briefPrint() 
{ 

    if(fileHeader) 
        fileHeader->print(); 
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        programHeaders[i]->print();
    }
    for (uint32_t i = 0; i < numberOfSections; i++){
        sectionHeaders[i]->print();
    }
    for (uint32_t i = 0; i < numberOfStringTables; i++){       
        stringTables[i]->print();
    }
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        symbolTables[i]->print();
    }
    for (uint32_t i = 0; i < numberOfRelocationTables; i++){
        relocationTables[i]->print();
    }
/*
    if(aOutHeader) 
        aOutHeader->print(); 
*/
}

void ElfFile::print(){
/*
    if(fileHeader) fileHeader->print();
    if(aOutHeader) aOutHeader->print();
    for(uint32_t i=1;i<=numberOfSections;i++){
        sectionHeaders[i]->print();
        rawSections[i]->print();
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(sectionHeaders[i]->getRelocationTable())
            sectionHeaders[i]->getRelocationTable()->print();
        if(sectionHeaders[i]->getLineInfoTable())
            sectionHeaders[i]->getLineInfoTable()->print();
    }
    if(symbolTable) symbolTable->print();
    if(stringTable) stringTable->print();
*/
}

void ElfFile::displaySymbols(){
/*
    ASSERT(rawSections && "FATAL : Raw data is not read");
    ASSERT(symbolTable && "FATAL : Symbol table is missing");

    uint32_t numberOfSymbols = symbolTable->getNumberOfSymbols();
    Symbol** addressSymbols = new Symbol*[numberOfSymbols];
    numberOfSymbols = symbolTable->filterSortAddressSymbols(addressSymbols,numberOfSymbols);

    if(numberOfSymbols){
        for(uint32_t i=1;i<=numberOfSections;i++){
            if(!rawSections[i]->IS_SECT_TYPE(OVRFLO)){
                rawSections[i]->displaySymbols(addressSymbols,numberOfSymbols);
            }
        }
    }
    delete[] addressSymbols;
*/
}

void ElfFile::findFunctions(){
/*
    ASSERT(symbolTable && "FATAL : Symbol table is missing");
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSections[i]->findFunctions();
    }
*/
}

void ElfFile::generateCFGs(){
/*
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSections[i]->generateCFGs();
    }
*/
}

void ElfFile::findMemoryFloatOps(){
/*
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSections[i]->findMemoryFloatOps();
    }
*/
}

/*
RawSection* ElfFile::findRawSection(uint64_t addr){

    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(rawSections[i]->inRange(addr))
            return rawSections[i];
    }

    return NULL;
}

*/

/*
RawSection* ElfFile::getBSSSection(){
    if(bssSectionIndex)
        return rawSections[bssSectionIndex];
    return NULL;
}
*/

/*
RawSection* ElfFile::getLoaderSection(){
    if(loaderSectionIndex)
        return rawSections[loaderSectionIndex];
    return NULL;
}
*/


/*
BasicBlock* ElfFile::findBasicBlock(HashCode* hashCode){
    ASSERT(hashCode->isBlock() && "FATAL: The given hashcode for the block is incorrect");

    uint32_t sectionNo = hashCode->getSection();
    uint32_t functionNo = hashCode->getFunction(); 
    uint32_t blockNo = hashCode->getBlock();

    if(sectionNo >= numberOfSections)
        return NULL;

    Function* func = rawSections[sectionNo]->getFunction(functionNo);
    if(!func)
        return NULL;

    return func->getBlock(blockNo);
}
*/

/*
uint32_t ElfFile::getAllBlocks(BasicBlock** arr){
    uint32_t ret = 0;
    for(uint32_t i=1;i<=numberOfSections;i++){
        uint32_t n = rawSections[i]->getAllBlocks(arr);
        arr += n;
        ret += n;
    }
    return ret;
}
*/

/*
RelocationTable* ElfFile::getRelocationTable(uint32_t idx){ 
    return sectionHeaders[idx]->getRelocationTable(); 
}
LineInfoTable* ElfFile::getLineInfoTable(uint32_t idx){ 
    return sectionHeaders[idx]->getLineInfoTable(); 
}
uint32_t ElfFile::getFileSize() { 
    return binaryInputFile.getSize(); 
}
uint64_t ElfFile::getDataSectionVAddr(){
    return sectionHeaders[dataSectionIndex]->GET(s_vaddr);
}
uint32_t ElfFile::getDataSectionSize(){
    return sectionHeaders[dataSectionIndex]->GET(s_size);
}
uint64_t ElfFile::getBSSSectionVAddr(){
    if(bssSectionIndex)
        return sectionHeaders[bssSectionIndex]->GET(s_vaddr);
    return 0;
}
uint64_t ElfFile::getTextSectionVAddr(){
    return sectionHeaders[textSectionIndex]->GET(s_vaddr);
}
*/

void ElfFile::setLineInfoFinder(){
/*
    PRINT_INFOR("Building LineInfoFinders");
    for (uint32_t i = 1; i <= numberOfSections; i++){
        rawSections[i]->buildLineInfoFinder();
    }
*/
}

void ElfFile::findLoops(){
/*
    PRINT_INFOR("Finding Loops");
    for (uint32_t i = 1; i <= numberOfSections; i++){
        rawSections[i]->buildLoops();
    }
*/
}

StringTable* ElfFile::findStringTableWithSectionIdx(uint32_t idx){
    for (uint32_t i = 0; i < numberOfStringTables; i++){
        if (idx == stringTables[i]->getSectionIndex()){
            return stringTables[i];
        }
    }
    return NULL;
}
SymbolTable* ElfFile::findSymbolTableWithSectionIdx(uint32_t idx){
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        if (idx == symbolTables[i]->getSectionIndex()){
            return symbolTables[i];
        }
    }
    return NULL;
}
RelocationTable* ElfFile::findRelocationTableWithSectionIdx(uint32_t idx){
    for (uint32_t i = 0; i < numberOfRelocationTables; i++){
        if (idx == relocationTables[i]->getSectionIndex()){
            return relocationTables[i];
        }
    }
    return NULL;
}

