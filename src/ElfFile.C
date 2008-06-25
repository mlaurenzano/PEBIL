#include <FileHeader.h>
#include <ProgramHeader.h>
#include <SectionHeader.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <RawSection.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <RelocationTable.h>
#include <Disassembler.h>
#include <CStructuresX86.h>
#include <BitSet.h>
#include <GlobalOffsetTable.h>
#include <DynamicTable.h>
#include <HashTable.h>
#include <NoteSection.h>

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


uint64_t ElfFile::extendTextSection(uint64_t size){
    size = nextAlignAddress(size,DEFAULT_PAGE_ALIGNMENT);
    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ProgramHeader* textHeader = getProgramHeader(textSegmentIdx);
    ProgramHeader* dataHeader = getProgramHeader(dataSegmentIdx);

    // first we will find the address of the first text section. we will be moving all elf
    // control structures that occur prior to this address with the extension of the text
    // segment (the interp and note.ABI-tag sections must be in the first text page and it
    // will make certain things easier for all control sections together)
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            getSectionHeader(i)->hasAllocBit() && getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextAddress > getSectionHeader(i)->GET(sh_addr)){
                ASSERT(lowestTextAddress == -1 && "Text section addresses should appear in increasing order");
                lowestTextAddress = getSectionHeader(i)->GET(sh_addr);
                lowestTextSectionIdx = i;
            }
        }
    }


    // for each segment that is contained within the loadable text segment,
    // update its address to reflect the new base address of the text segment
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        ProgramHeader* subHeader = getProgramHeader(i);
        if (textHeader->inRange(subHeader->GET(p_vaddr)) && i != textSegmentIdx){
            PRINT_INFOR("Found a segment(%d) that is in the text segment", i);
            Elf32_Phdr subEntry;
            memcpy(&subEntry,((ProgramHeader32*)subHeader)->charStream(),sizeof(subEntry));
            subEntry.p_vaddr -= (uint32_t)size;
            subEntry.p_paddr -= (uint32_t)size;
            memcpy(((ProgramHeader32*)subHeader)->charStream(),&subEntry,sizeof(subEntry));
        } 
    }


    // for each segment that is (or is contained within) the data segment,
    // update its offset to reflect the the base address of the executable
    // (ie the base address of the text segment)
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        ProgramHeader* subHeader = getProgramHeader(i);
        if (dataHeader->inRange(subHeader->GET(p_vaddr))){
            Elf32_Phdr subEntry;
            memcpy(&subEntry,((ProgramHeader32*)subHeader)->charStream(),sizeof(subEntry));
            subEntry.p_offset += (uint32_t)size;
            memcpy(((ProgramHeader32*)subHeader)->charStream(),&subEntry,sizeof(subEntry));
        }
    }


    // update section symbols for the sections that were moved. technically the loader won't use them 
    // but we will try to keep the binary as consistent as possible
    SymbolTable* symTab = getSymbolTable(1);
    for (uint32_t i = 0; i < symTab->getNumberOfSymbols(); i++){
        Symbol32* sym = (Symbol32*)symTab->getSymbol(i);
        if (sym->getSymbolType() == STT_SECTION && sym->GET(st_value) < lowestTextAddress){
            sym->setValue(sym->GET(st_value)-size);
        }
    }
    
    // modify the base address of the text segment
    Elf32_Phdr textEntry;
    memcpy(&textEntry,((ProgramHeader32*)textHeader)->charStream(),sizeof(textEntry));
    textEntry.p_vaddr -= (uint32_t)size;
    textEntry.p_paddr -= (uint32_t)size;
    textEntry.p_memsz += (uint32_t)size;
    textEntry.p_filesz += (uint32_t)size;
    memcpy(((ProgramHeader32*)textHeader)->charStream(),&textEntry,sizeof(textEntry));


    // For any section that falls before the program's code, displace its address so that it is in the
    // same location relative to the base address.
    // Likewise, displace the offset of any section that falls during/after the program's code so that
    // the code will be put in the correct location within the text segment.
    for (uint32_t i = 1; i < numberOfSections; i++){
        SectionHeader32* sHdr = (SectionHeader32*)getSectionHeader(i);
        if (i < lowestTextSectionIdx){
            ASSERT(getSectionHeader(i)->GET(sh_addr) < lowestTextAddress && "No section that occurs before the first text section should have a larger address");
            // strictly speaking the loader doesn't use these, but for consistency we change these
            sHdr->setAddress(sHdr->GET(sh_addr)-size);
        } else {
            sHdr->setOffset(sHdr->GET(sh_offset)+size);            
        }
    }

    // since some sections were displaced in the file, displace the section header table also so
    // that it occurs after all of the sections in the file
    Elf32_Ehdr elfEntry;
    memcpy(&elfEntry,((FileHeader32*)getFileHeader())->charStream(),sizeof(elfEntry));
    elfEntry.e_shoff += (uint32_t)size;
    memcpy(((FileHeader32*)getFileHeader())->charStream(),&elfEntry,sizeof(elfEntry));


    // update the dynamic table to correctly point to the displaced elf control sections
    for (uint32_t i = 0; i < getDynamicTable()->getNumberOfDynamics(); i++){
        Dynamic32* dyn = (Dynamic32*)getDynamicTable()->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->setPointer(dyn->GET_A(d_ptr,d_un)-size);
        }
    }

    return size;
}

void ElfFile::relocateSection(uint16_t scnIdx){
    scnIdx = getSymbolTable(dynamicSymtabIdx)->getStringTable()->getSectionIndex();

    SectionHeader32* header = (SectionHeader32*)getSectionHeader(scnIdx);
    RawSection* raw = getRawSection(scnIdx);
    uint64_t maxUsedAddr = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (getSectionHeader(i)->GET(sh_addr) + getSectionHeader(i)->GET(sh_size) > maxUsedAddr){
            maxUsedAddr = getSectionHeader(i)->GET(sh_addr) + getSectionHeader(i)->GET(sh_size);
        }
    }

    maxUsedAddr = nextAlignAddress(maxUsedAddr,DEFAULT_PAGE_ALIGNMENT);

    Elf32_Shdr hdrEntry;
    memcpy(&hdrEntry,header->charStream(),sizeof(hdrEntry));
    hdrEntry.sh_addr = maxUsedAddr;
    memcpy(header->charStream(),&hdrEntry,sizeof(hdrEntry));

    getDynamicTable()->relocateStringTable(maxUsedAddr);

    uint64_t segmentBaseAddr = getProgramBaseAddress();

    verify();
}

// get the smallest virtual address of all loadable segments (ie, the base address for the program)
uint64_t ElfFile::getProgramBaseAddress(){
    uint64_t segmentBase = -1;

    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if (getProgramHeader(i)->GET(p_type) == PT_LOAD){
            if (getProgramHeader(i)->GET(p_vaddr) < segmentBase){
                segmentBase = getProgramHeader(i)->GET(p_vaddr);
            }
        }
    }

    ASSERT(segmentBase != -1 && "No loadable segments found (or their v_addr fields are incorrect)");
    return segmentBase;
}


void ElfFile::addExitFunction(const char* libname, const char* funcname){
    StringTable* dynStrings = getSymbolTable(dynamicSymtabIdx)->getStringTable();

    /* the addString function is not properly functional at the moment
    PRINT_INFOR("Adding shared library name to string table: %s", libname);
    dynStrings->addString(libname);
    PRINT_INFOR("Adding function name to string table: %s", funcname);
    dynStrings->addString(funcname);
    */

}


void ElfFile::initSectionFilePointers(){

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


    // find the dynamic symbol table
    dynamicSymtabIdx = numberOfSymbolTables;
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        if (getSymbolTable(i)->isDynamic()){
            ASSERT(dynamicSymtabIdx == numberOfSymbolTables && "Cannot have multiple dynamic symbol tables");
            dynamicSymtabIdx = i;
        }

    }
    ASSERT(dynamicSymtabIdx != numberOfSymbolTables && "Cannot analyze a file if it doesn't have a dynamic symbol table");
    PRINT_INFOR("Dynamic symbol table is symbol table %d (actual section is %d)", dynamicSymtabIdx, getSymbolTable(dynamicSymtabIdx)->getSectionIndex());


    // find the global offset table's address
    uint64_t gotBaseAddress = 0;
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        SymbolTable* currentSymtab = getSymbolTable(i);
        for (uint32_t j = 0; j < currentSymtab->getNumberOfSymbols(); j++){

            // yes, we actually have to look for this symbol's name to find it!
            char* symName = currentSymtab->getSymbolName(j);
            if (!strcmp(symName,GOT_SYM_NAME)){
                PRINT_INFOR("Found a GOT symbol at %d,%d", i, j);
                if (gotBaseAddress){
                    PRINT_WARN("Found mutiple symbols for Global Offset Table (symbols named %s), addresses are 0x%016llx, 0x%016llx",
                               GOT_SYM_NAME, gotBaseAddress, currentSymtab->getSymbol(j)->GET(st_value));
                    ASSERT(gotBaseAddress == currentSymtab->getSymbol(j)->GET(st_value) && "Conflicting addresses for Global Offset Table Found!");
                }
                gotBaseAddress = currentSymtab->getSymbol(j)->GET(st_value);
            }
        }
    }
    ASSERT(gotBaseAddress && "Cannot find a symbol for the global offset table");
    PRINT_INFOR("Global Offset Table found at address 0x%016llx", gotBaseAddress);

    // find the global offset table
    uint16_t gotSectionIdx = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->inRange(gotBaseAddress)){
            ASSERT(!gotSectionIdx && "Cannot have multiple global offset tables");
            gotSectionIdx = i;
        }
    }
    ASSERT(gotSectionIdx && "Cannot find a section for the global offset table");
    ASSERT(getSectionHeader(gotSectionIdx)->GET(sh_type) == SHT_PROGBITS && "Global Offset Table section header is wrong type");


    // The raw section for the global offset table should already have been initialized as a generic RawSection
    // we will destroy it and create it as a GlobalOffsetTable
    ASSERT(rawSections[gotSectionIdx] && "Global Offset Table not yet created");
    delete rawSections[gotSectionIdx];
    
    char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[gotSectionIdx]->GET(sh_offset));
    uint64_t sectionSize = (uint64_t)sectionHeaders[gotSectionIdx]->GET(sh_size);    
    rawSections[gotSectionIdx] = new GlobalOffsetTable(sectionFilePtr, sectionSize, gotSectionIdx, gotBaseAddress, this);
    ASSERT(!globalOffsetTable && "global offset table should not be initialized");
    globalOffsetTable = (GlobalOffsetTable*)rawSections[gotSectionIdx];
    globalOffsetTable->read(&binaryInputFile);
    

    // find the dynamic section's address
    uint64_t dynamicSectionAddress = 0;
 
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        SymbolTable* currentSymtab = getSymbolTable(i);
        for (uint32_t j = 0; j < currentSymtab->getNumberOfSymbols(); j++){

            // yes, we actually have to look for this symbol's name to find it!
            char* symName = currentSymtab->getSymbolName(j);
            if (!strcmp(symName,DYN_SYM_NAME)){
                if (dynamicSectionAddress){
                    PRINT_WARN("Found mutiple symbols for Dynamic Section (symbols named %s), addresses are 0x%016llx, 0x%016llx",
                               DYN_SYM_NAME, dynamicSectionAddress, currentSymtab->getSymbol(j)->GET(st_value));
                    ASSERT(dynamicSectionAddress == currentSymtab->getSymbol(j)->GET(st_value) && "Two different addresses for Dynamic Section Found!");
                }
                dynamicSectionAddress = currentSymtab->getSymbol(j)->GET(st_value);
            }
        }
    }
    ASSERT(dynamicSectionAddress && "Cannot find a symbol for the dynamic section");
    PRINT_INFOR("Dynamic section found at address 0x%016llx", dynamicSectionAddress);

    // find the dynamic table
    uint16_t dynamicTableSectionIdx = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_addr) == dynamicSectionAddress){
            ASSERT(!dynamicTableSectionIdx && "Cannot have multiple dynamic sections");
            dynamicTableSectionIdx = i;
        }
    }
    ASSERT(dynamicTableSectionIdx && "Cannot find a section for the dynamic table");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->GET(sh_type) == SHT_DYNAMIC && "Dynamic Section section header is wrong type");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->hasAllocBit() && "Dynamic Section section header missing an attribute");

    PRINT_INFOR("Dynamic Section is in section %d", dynamicTableSectionIdx);


    uint16_t dynamicSegmentIdx = 0;
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if (programHeaders[i]->GET(p_type) == PT_DYNAMIC){
            ASSERT(!dynamicSegmentIdx && "Cannot have multiple segments for the dynamic section");
            dynamicSegmentIdx = i;
        }
    }
    PRINT_INFOR("Dynamic segment is %d", dynamicSegmentIdx);
    ASSERT(dynamicSegmentIdx && "Cannot find a segment for the dynamic table");
    ASSERT(getProgramHeader(dynamicSegmentIdx)->GET(p_vaddr) == dynamicSectionAddress && "Dynamic segment address from symbol and programHeader don't match");

    // The raw section for the dynamic table should already have been initialized as a generic RawSection
    // we will destroy it and create it as a DynamicTable
    ASSERT(rawSections[dynamicTableSectionIdx] && "Dynamic Table raw section not yet created");
    delete rawSections[dynamicTableSectionIdx];

    sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[dynamicTableSectionIdx]->GET(sh_offset));
    sectionSize = (uint64_t)sectionHeaders[dynamicTableSectionIdx]->GET(sh_size);

    rawSections[dynamicTableSectionIdx] = new DynamicTable(sectionFilePtr, sectionSize, dynamicTableSectionIdx, dynamicSegmentIdx, this);
    ASSERT(!dynamicTable && "dynamic table should not be initialized");
    dynamicTable = (DynamicTable*)rawSections[dynamicTableSectionIdx];
    dynamicTable->read(&binaryInputFile);
    dynamicTable->verify();

}


void ElfFile::print() 
{ 

    if(fileHeader){
        fileHeader->print(); 
    }

    if (programHeaders){
        PRINT_INFOR("");
        PRINT_INFOR("Program Header Table");
        PRINT_INFOR("\t\t\tType\t\tSize in File\t\tVirtual Address\t\tPhysical Address");
        PRINT_INFOR("\t\t\tOffset\t\tSize in Memory\t\tFlags\t\t\tAlignment");
        for (uint32_t i = 0; i < numberOfPrograms; i++){
            if (programHeaders[i]){
                programHeaders[i]->print();
            }
        }
    }
    
    if (sectionHeaders){
        PRINT_INFOR("");
        PRINT_INFOR("Section Header Table");
        PRINT_INFOR("\t\t\t\tName\t\tType\t\t\tFlags\t\t\tAddress\t\t\tOffset");
        PRINT_INFOR("\t\t\t\tLink\t\tInfo\t\t\tSize\t\t\tAlignment\t\tEntry Size");
        for (uint32_t i = 0; i < numberOfSections; i++){
            if (sectionHeaders[i]){
                sectionHeaders[i]->print();
            }
        }
    }

    if (stringTables){
        PRINT_INFOR("");
        for (uint32_t i = 0; i < numberOfStringTables; i++){       
            if (stringTables[i]){
                stringTables[i]->print();
            }
        }
    }

    if (symbolTables){
        PRINT_INFOR("");
        for (uint32_t i = 0; i < numberOfSymbolTables; i++){
            if (symbolTables[i])
                symbolTables[i]->print();
        }
    }


    if (relocationTables){
        PRINT_INFOR("");
        for (uint32_t i = 0; i < numberOfRelocationTables; i++){
            if (relocationTables[i]){
                relocationTables[i]->print();
            }
        }
    }

    if (globalOffsetTable){
        PRINT_INFOR("");
        globalOffsetTable->print();
    }

    if (dynamicTable){
        PRINT_INFOR("");
        dynamicTable->print();
    }

    if (dynamicTable){
        PRINT_INFOR("");
        dynamicTable->printSharedLibraries(&binaryInputFile);
    }

    if (hashTable){
        PRINT_INFOR("");
        hashTable->print();
    }

    if (noteSections){
        PRINT_INFOR("");
        for (uint32_t i = 0; i < numberOfNoteSections; i++){
            noteSections[i]->print();
        }
    }
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


uint32_t ElfFile::disassemble(){
    uint32_t numInstrs = 0;

    if (!disassembler){
        disassembler = new Disassembler(is64Bit());
    }

    for (uint32_t i = 0; i < numberOfTextSections; i++){
        numInstrs += textSections[i]->disassemble();
    }
    return numInstrs;
}

uint32_t ElfFile::printDisassembledCode(){
    uint32_t numInstrs = 0;

    if (!disassembler){
        disassembler = new Disassembler(is64Bit());
    }

    for (uint32_t i = 0; i < numberOfTextSections; i++){
        numInstrs += textSections[i]->printDisassembledCode();
    }
    return numInstrs;
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
    uint64_t extraSectionOffset = sectionHeaders[numberOfSections-1]->GET(sh_offset) +
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

    verify();
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
    PRINT_INFOR("dumped %d section headers", numberOfSections);

    for (uint32_t i = 0; i < numberOfSections; i++){
        currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (sectionHeaders[i]->hasBitsInFile()){
            rawSections[i]->dump(&binaryOutputFile,currentOffset);
        }
    }
    PRINT_INFOR("dumped %d raw sections", numberOfSections);

    binaryOutputFile.close();

}

bool ElfFile::verify(){

    if (numberOfPrograms != getFileHeader()->GET(e_phnum)){
        PRINT_ERROR("Number of segments in file header is inconsistent with our internal count");
    }
    if (numberOfSections != getFileHeader()->GET(e_shnum)){
        PRINT_ERROR("Number of sections in file header is inconsistent with our internal count");
    }
    
    // verify that there is only 1 text and 1 data segment
    uint32_t textSegCount = 0;
    uint32_t dataSegCount = 0;
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        ProgramHeader* phdr = getProgramHeader(i);
        if (phdr->GET(p_type) == PT_LOAD){
            if (phdr->isReadable() && phdr->isExecutable()){
                textSegmentIdx = i;
                textSegCount++;
            } else if (phdr->isReadable() && phdr->isWritable()){
                dataSegmentIdx = i;
                dataSegCount++;
            } else {
                PRINT_ERROR("Segment(%d) with type PT_LOAD has attributes that are not consistent with text or data");
            }
        }
    }
    if (textSegCount != 1){
        PRINT_ERROR("Exactly 1 text segment allowed, %d found", textSegCount);
    }
    if (dataSegCount != 1){
        PRINT_ERROR("Exactly 1 data segment allowed, %d found", dataSegCount);
    }


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
    }
    for (uint32_t i = 0; i < numberOfPrograms; i++){    
        if (programHeaders[i]->GET(p_type) == PT_LOAD){
            if (i < ptInterpIdx){
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
        if (sectionHeaders[i]->GET(sh_type) == SHT_HASH || sectionHeaders[i]->GET(sh_type) == SHT_GNU_HASH){
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
        PRINT_ERROR("Elf file cannot have more than one hash sections");
        return false;
    }
        // enforce that an elf file can have only one dynamic linking section
    if (dynlinkSectionCount > MAX_SHT_DYNAMIC_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d dynamic linking sections", MAX_SHT_DYNAMIC_COUNT);
        return false;
    }
        // enforce that an elf file can have only one symbol table section
    if (dynlinkSectionCount > MAX_SHT_SYMTAB_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d symbol table sections", MAX_SHT_SYMTAB_COUNT);
        return false;
    }
        // enforce that an elf file can have only one dynamic symtab section
    if (dynlinkSectionCount > MAX_SHT_DYNSYM_COUNT){
        PRINT_ERROR("Elf file cannot have more than %d dynamic symtab sections", MAX_SHT_DYNSYM_COUNT);
        return false;
    }

    for (uint32_t i = 0; i < numberOfSections; i++){
        rawSections[i]->verify();
    }

    for (uint32_t i = 0; i < numberOfPrograms; i++){
        getProgramHeader(i)->verify();
    }
    for (uint32_t i = 0; i < numberOfSections; i++){
        getSectionHeader(i)->verify();
        getRawSection(i)->verify();
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
    initSectionFilePointers();

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
    PRINT_INFOR("Found %d program header entries, reading at location %#x\n", numberOfPrograms, binaryInputFile.currentOffset());
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        ProgramHeader* phdr = getProgramHeader(i);
    }

    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if(is64Bit()){
            programHeaders[i] = new ProgramHeader64(i);
        } else {
            programHeaders[i] = new ProgramHeader32(i);
        }
        ASSERT(programHeaders[i]);
        programHeaders[i]->read(&binaryInputFile);
        programHeaders[i]->verify();

        if (programHeaders[i]->GET(p_type) == PT_LOAD){
            if (programHeaders[i]->isReadable() && programHeaders[i]->isExecutable()){
                textSegmentIdx = i;
            } else if (programHeaders[i]->isReadable() && programHeaders[i]->isWritable()){
                dataSegmentIdx = i;
            }
        }
    }
}

void ElfFile::readSectionHeaders(){
    PRINT_INFOR("Parsing the section header table");
    sectionHeaders = new SectionHeader*[numberOfSections];
    ASSERT(sectionHeaders);

    binaryInputFile.setInPointer(fileHeader->getSectionHeaderTablePtr());
    PRINT_INFOR("Found %d section header entries, reading at location %#x\n", numberOfSections, binaryInputFile.currentOffset());

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
    PRINT_INFOR("Setting section types");
    for (uint32_t i = 0; i < numberOfSections; i++){
        ElfClassTypes typ = sectionHeaders[i]->setSectionType();
        switch(typ){
        case (ElfClassTypes_string_table) : numberOfStringTables++;
        case (ElfClassTypes_symbol_table) : numberOfSymbolTables++;
        case (ElfClassTypes_relocation_table) : numberOfRelocationTables++;
        case (ElfClassTypes_dwarf_section)  : numberOfDwarfSections++;
        case (ElfClassTypes_text_section) : numberOfTextSections++;
        case (ElfClassTypes_note_section) : numberOfNoteSections++;
        default: ;
        }
    }

}

void ElfFile::readRawSections(){
    ASSERT(sectionHeaders && "We should have read the section headers already");

    rawSections = new RawSection*[numberOfSections];

    stringTables = new StringTable*[numberOfStringTables];
    symbolTables = new SymbolTable*[numberOfSymbolTables];
    relocationTables = new RelocationTable*[numberOfRelocationTables];
    dwarfSections = new DwarfSection*[numberOfDwarfSections];
    textSections = new TextSection*[numberOfTextSections];
    noteSections = new NoteSection*[numberOfNoteSections];

    numberOfStringTables = numberOfSymbolTables = numberOfRelocationTables = 
    numberOfDwarfSections = numberOfTextSections = numberOfNoteSections = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
        uint64_t sectionSize = (uint64_t)sectionHeaders[i]->GET(sh_size);

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
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_text_section){
            rawSections[i] = new TextSection(sectionFilePtr, sectionSize, i, numberOfTextSections, this);
            textSections[numberOfTextSections] = (TextSection*)rawSections[i];
            numberOfTextSections++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_hash_table){
            ASSERT(!hashTable && "Cannot have multiple hash table sections");
            rawSections[i] = new HashTable(sectionFilePtr, sectionSize, i, this);
            hashTable = (HashTable*)rawSections[i];
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_note_section){
            rawSections[i] = new NoteSection(sectionFilePtr, sectionSize, i, numberOfNoteSections, this);
            noteSections[numberOfNoteSections] = (NoteSection*)rawSections[i];
            numberOfNoteSections++;
        } else {
            rawSections[i] = new RawSection(ElfClassTypes_no_type, sectionFilePtr, sectionSize, i, this);
        }
        rawSections[i]->read(&binaryInputFile);
        PRINT_INFOR("Finished reading section %d", i);
    }
}


void ElfFile::briefPrint(){
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


