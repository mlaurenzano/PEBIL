#include <FileHeader.h>
#include <ProgramHeader.h>
#include <SectionHeader.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <RawSection.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <RelocationTable.h>
#include <DwarfSection.h>
#include <Disassemble.h>
#include <BitSet.h>


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


void ElfFile::findFunctions(){
/*
    ASSERT(symbolTable && "FATAL : Symbol table is missing");
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSections[i]->findFunctions();
    }
*/
}


void ElfFile::printDisassembledCode(){
    struct disassemble_info disInfo;
    x86inst_set_disassemble_info(&disInfo,(uint32_t)is64Bit());

    PRINT_INFOR("disassemble_info size = %d, priv points to %x", sizeof(struct disassemble_info), disInfo.private_data);

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint32_t instructionCount = 0;
    uint64_t instructionAddress;

    for (uint32_t i = 1; i < numberOfSections; i++){
        if (sectionHeaders[i]->hasExecInstrBit()){
            PRINT_INFOR("Disassembly of Section %s", sectionHeaders[i]->getSectionNamePtr());
            sectionHeaders[i]->print();
            instructionCount = 0;

            for (currByte = 0; currByte < sectionHeaders[i]->GET(sh_size); currByte += instructionLength, instructionCount++){
                instructionAddress = (uint64_t)((uint32_t)rawSections[i]->charStream() + currByte);
                fprintf(stdout, "(0x%lx) 0x%lx:\t", (uint32_t)(rawSections[i]->charStream() + currByte), sectionHeaders[i]->GET(sh_addr) + currByte);

                instructionLength = print_insn(instructionAddress, &disInfo, is64Bit());

                fprintf(stdout, "\t(bytes -- ");
                uint8_t* bytePtr;
                for (uint32_t j = 0; j < instructionLength; j++){
                    bytePtr = (uint8_t*)rawSections[i]->charStream() + currByte + j;
                    fprintf(stdout, "%2.2lx ", *bytePtr);
                }


                fprintf(stdout, ")\n");
            }
            PRINT_INFOR("Found %d instructions (%d bytes) in section %d", instructionCount, currByte, i);

        }
    }

}

void ElfFile::disassemble(){
    struct disassemble_info disInfo;
    x86inst_set_disassemble_info(&disInfo,(uint32_t)is64Bit());

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint32_t instructionCount = 0;
    uint64_t instructionAddress;

    for (uint32_t i = 1; i < numberOfSections; i++){
        if (sectionHeaders[i]->hasExecInstrBit()){
            instructionCount = 0;

            for (currByte = 0; currByte < sectionHeaders[i]->GET(sh_size); currByte += instructionLength, instructionCount++){
                instructionAddress = (uint64_t)((uint32_t)rawSections[i]->charStream() + currByte);
                instructionLength = print_insn(instructionAddress, &disInfo, is64Bit());
                fprintf(stdout, "\n");
            }
            PRINT_INFOR("Found %d instructions (%d bytes) in section %d", instructionCount, currByte, i);

        }
    }
}

uint32_t ElfFile::findSectionNameInStrTab(char* name){
    if (!stringTables){
        return 0;
    }

    StringTable* st = stringTables[sectionNameStrTabIdx];

    for (uint32_t currByte = 0; currByte < st->getSizeInBytes(); currByte++){
        char* ptr = (char*)(st->getFilePointer() + currByte);
        if (strcmp(name,ptr) == 0){
            PRINT_INFOR("Found section name `%s` at offset %d in string table %d", name, currByte, sectionNameStrTabIdx);
            return currByte;
        }
    }

    return 0;
}

void ElfFile::addDataSection(uint64_t size, char* bytes){
    size = nextAlignAddress(size,getAddressAlignment());

    /* this extra section is located after all other existing sections */
    uint32_t extraSectionIdx = numberOfSections + 1;
    SectionHeader32* extraSectionHeader = new SectionHeader32(extraSectionIdx);
    Elf32_Shdr extraEntry;

    /* get the section offset for this extra section */
    Elf32_Addr extraSectionOffset = sectionHeaders[numberOfSections-1]->GET(sh_offset) +
            sectionHeaders[numberOfSections-1]->GET(sh_size);
    extraSectionOffset = (nextAlignAddress(extraSectionOffset,getAddressAlignment()));

    /* initialize the extra section's header */
    PRINT_INFOR("Adding data section -- idx=%d", extraSectionIdx);
    extraEntry.sh_name = findSectionNameInStrTab(".data");
    extraEntry.sh_type = SHT_PROGBITS;
    extraEntry.sh_flags = 0x3;
    extraEntry.sh_addr = 0x0;
    extraEntry.sh_offset = extraSectionOffset;
    extraEntry.sh_size = size;
    extraEntry.sh_link = 0;
    extraEntry.sh_info = 0;
    extraEntry.sh_addralign = getAddressAlignment();
    extraEntry.sh_entsize = 0;
    memcpy(extraSectionHeader->charStream(), (char*)&extraEntry, Size__32_bit_Section_Header);
    extraSectionHeader->setSectionNamePtr(extraEntry.sh_name + getStringTable(sectionNameStrTabIdx)->getFilePointer());
    
    RawSection* extraRawSection = new RawSection(ElfClassTypes_no_type, bytes, size, extraSectionIdx, this);

    /* update the file header to reflect the presence of the new section */
    Elf32_Ehdr extraFileHeader;
    FileHeader32* myFileHeader = (FileHeader32*)fileHeader;

    memcpy((char*)&extraFileHeader, myFileHeader->charStream(), Size__32_bit_File_Header);
    extraFileHeader.e_shoff += size;
    extraFileHeader.e_shnum += 1;
    memcpy(myFileHeader->charStream(), (char*)&extraFileHeader, Size__32_bit_File_Header);

    /* add the new section+header to the file */
    SectionHeader** newScnHdrs = new SectionHeader*[numberOfSections+1];
    RawSection** newRawScns = new RawSection*[numberOfSections+1];

    for (uint32_t i = 0; i < numberOfSections; i++){
        newScnHdrs[i] = sectionHeaders[i];
        newRawScns[i] = rawSections[i];
    }
    newScnHdrs[numberOfSections] = extraSectionHeader;
    newRawScns[numberOfSections] = extraRawSection;

    delete[] sectionHeaders;
    delete[] rawSections;

    sectionHeaders = newScnHdrs;
    rawSections = newRawScns; 
    numberOfSections++;
    //    briefPrint();
    PRINT_INFOR("new raw section: %s", rawSections[numberOfSections-1]->charStream());
}

void ElfFile::addTextSection(uint64_t size, char* bytes){
}

void ElfFile::addSharedLibrary(){
}


void ElfFile::dump(char* extension){
    uint32_t currentOffset;
    uint32_t elfFileAlignment = getAddressAlignment();

    char fileName[80] = "";
    sprintf(fileName,"%s.%s", elfFileName, extension);

    PRINT_INFOR("Output file is %s", fileName);

    binaryOutputFile.open(fileName);
    if(!binaryOutputFile){
        PRINT_ERROR("The output file can not be opened %s",fileName);
    }

    currentOffset = ELF_FILE_HEADER_OFFSET;
    fileHeader->dump(&binaryOutputFile,currentOffset);
    PRINT_INFOR("dumped file header");

    currentOffset = fileHeader->GET(e_phoff);
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        programHeaders[i]->dump(&binaryOutputFile,currentOffset);
        currentOffset += programHeaders[i]->getSizeInBytes();
    }
    PRINT_INFOR("dumped %d program headers", numberOfPrograms);

    currentOffset = fileHeader->GET(e_shoff);
    for (uint32_t i = 0; i < numberOfSections; i++){
        sectionHeaders[i]->dump(&binaryOutputFile,currentOffset);
        currentOffset += sectionHeaders[i]->getSizeInBytes();
	//        PRINT_INFOR("dumped section header[%d]", i);
    }
    PRINT_INFOR("dumped %d raw sections", numberOfSections);

    for (uint32_t i = 0; i < numberOfSections; i++){
        currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (sectionHeaders[i]->hasBitsInFile()){
            rawSections[i]->dump(&binaryOutputFile,currentOffset);
        }
	//        PRINT_INFOR("dumped raw section[%d]", i);
    }
    PRINT_INFOR("dumped %d section headers", numberOfSections);

    binaryOutputFile.close();

}

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
    uint32_t symtabSectionCount = 0;
    uint32_t dynsymSectionCount = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) == SHT_HASH){
            hashSectionCount++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_DYNAMIC){
            dynlinkSectionCount++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_SYMTAB){
            symtabSectionCount++;
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_DYNSYM){
            dynsymSectionCount++;
        }
    }
        // enforce that an elf file can have only one hash section
    if (hashSectionCount > MAX_SHT_HASH_COUNT){
        PRINT_ERROR("Elf file cannot have more than one hash section");
        return false;
    }
        // enforce that an elf file can have only one dynamic linking section
    if (dynlinkSectionCount > MAX_SHT_DYNAMIC_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d dynamic linking section", MAX_SHT_DYNAMIC_COUNT);
        return false;
    }
        // enforce that an elf file can have only one symbol table section
    if (dynlinkSectionCount > MAX_SHT_SYMTAB_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d symbol table section", MAX_SHT_SYMTAB_COUNT);
        return false;
    }
        // enforce that an elf file can have only one dynamic symtab section
    if (dynlinkSectionCount > MAX_SHT_DYNSYM_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d dynamic symtab section", MAX_SHT_DYNSYM_COUNT);
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
    readRawSections();
    initRawSectionFilePointers();

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

void ElfFile::readProgramHeaders(){

    PRINT_INFOR("Parsing the program header table");
    programHeaders = new ProgramHeader*[numberOfPrograms];
    ASSERT(programHeaders);

    binaryInputFile.setInPointer(fileHeader->getProgramHeaderTablePtr());
    //    PRINT_INFOR("Found %d program header entries, reading at location %#x\n", numberOfPrograms, binaryInputFile.currentOffset());

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

void ElfFile::readSectionHeaders(){
    PRINT_INFOR("Parsing the section header table");
    sectionHeaders = new SectionHeader*[numberOfSections];
    ASSERT(sectionHeaders);

    binaryInputFile.setInPointer(fileHeader->getSectionHeaderTablePtr());
    //PRINT_INFOR("Found %d section header entries, reading at location %#x\n", numberOfSections, binaryInputFile.currentOffset());

        // first read each section header
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

        // determine and set section type for each section header
    //PRINT_INFOR("Setting section types");
    for (uint32_t i = 0; i < numberOfSections; i++){
        ElfClassTypes typ = sectionHeaders[i]->setSectionType();
        switch(typ){
            case (ElfClassTypes_string_table) : numberOfStringTables++;
            case (ElfClassTypes_symbol_table) : numberOfSymbolTables++;
            case (ElfClassTypes_relocation_table) : numberOfRelocationTables++;
            case (ElfClassTypes_dwarf_section)  : numberOfDwarfSections++;
            default: ;
        }
    }

}

void ElfFile::initRawSectionFilePointers(){
        // find the string table for section names
    ASSERT(fileHeader->GET(e_shstrndx) && "No section name string table");
    for (uint32_t i = 0; i < numberOfStringTables; i++){
        if (stringTables[i]->getSectionIndex() == fileHeader->GET(e_shstrndx)){
            sectionNameStrTabIdx = i;
        }
    }

        // set section names
    PRINT_INFOR("Reading the scnhdr string table");
    ASSERT(sectionHeaders && "Section headers not present");
    ASSERT(sectionNameStrTabIdx && "Section header string table index must be defined");
    char* stringTablePtr = getStringTable(sectionNameStrTabIdx)->getFilePointer();
    PRINT_INFOR("String table is located at %#x", stringTablePtr);
    PRINT_INFOR("Setting section header names from string table");
    // skip first section header since it is reserved and its values are null
    for (uint32_t i = 1; i < numberOfSections; i++){
        ASSERT(sectionHeaders[i]->getSectionNamePtr() == NULL && "Section Header name shouldn't already be set");
        uint32_t sectionNameOffset = sectionHeaders[i]->GET(sh_name);
//        ASSERT(sectionNameOffset < stringTables[sectionNameStrTabIdx]->getSizeInBytes() && "Section header name idx 
//should be in string table");
        sectionHeaders[i]->setSectionNamePtr(stringTablePtr + sectionNameOffset);
    }

        // find the string table for each symbol table
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        getSymbolTable(i)->setStringTable();
    }

        // find the symbol table + relocation section for each relocation table
    for (uint32_t i = 0; i < numberOfRelocationTables; i++){
        getRelocationTable(i)->setSymbolTable();
        getRelocationTable(i)->setRelocationSection();
    }
}

void ElfFile::readRawSections(){
    ASSERT(sectionHeaders && "We should have read the section headers already");

    rawSections = new RawSection*[numberOfSections];

    stringTables = new StringTable*[numberOfStringTables];
    symbolTables = new SymbolTable*[numberOfSymbolTables];
    relocationTables = new RelocationTable*[numberOfRelocationTables];
    dwarfSections = new DwarfSection*[numberOfDwarfSections];

    numberOfStringTables = numberOfSymbolTables = numberOfRelocationTables = numberOfDwarfSections = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
        uint64_t sectionSize = (uint64_t)sectionHeaders[i]->GET(sh_size);
//        PRINT_INFOR("Using section type %d for section %d", sectionHeaders[i]->getSectionType(), i);

        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_string_table){
            rawSections[i] = new StringTable(sectionFilePtr, sectionSize, i, numberOfStringTables, this);
            stringTables[numberOfStringTables] = (StringTable*)rawSections[i];
            numberOfStringTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_symbol_table){
            rawSections[i] = new SymbolTable(sectionFilePtr, sectionSize, i, numberOfSymbolTables, this);
            symbolTables[numberOfSymbolTables] = (SymbolTable*)rawSections[i];
            numberOfSymbolTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_relocation_table){
            rawSections[i] = new RelocationTable(sectionFilePtr, sectionSize, i, numberOfRelocationTables, this);
            relocationTables[numberOfRelocationTables] = (RelocationTable*)rawSections[i];
            numberOfRelocationTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_dwarf_section){
            rawSections[i] = new DwarfSection(sectionFilePtr, sectionSize, i, numberOfDwarfSections, this);
            dwarfSections[numberOfDwarfSections] = (DwarfSection*)rawSections[i];
            numberOfDwarfSections++;
        } else {
            rawSections[i] = new RawSection(ElfClassTypes_no_type, sectionFilePtr, sectionSize, i, this);
        }
        rawSections[i]->read(&binaryInputFile);
    }

    //    PRINT_INFOR("Found sections: %d %d %d %d", numberOfStringTables, numberOfSymbolTables, numberOfRelocationTables, numberOfDwarfSections);
}

void ElfFile::briefPrint() 
{ 

    if(fileHeader){
        fileHeader->print(); 
    }
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
}

void ElfFile::print(){
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


