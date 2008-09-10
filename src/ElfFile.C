#include <ElfFile.h>
#include <FileHeader.h>
#include <ProgramHeader.h>
#include <SectionHeader.h>
#include <BinaryFile.h>
#include <RawSection.h>
#include <TextSection.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <RelocationTable.h>
#include <Disassembler.h>
#include <CStructuresX86.h>
#include <GlobalOffsetTable.h>
#include <DynamicTable.h>
#include <HashTable.h>
#include <NoteSection.h>
#include <Instruction.h>
#include <PriorityQueue.h>
#include <GnuVerneedTable.h>
#include <GnuVersymTable.h>

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

uint16_t ElfFile::findSectionIdx(char* name){
    for (uint16_t i = 1; i < numberOfSections; i++){
        if (name && sectionHeaders[i]->getSectionNamePtr()){
            if (!strcmp(sectionHeaders[i]->getSectionNamePtr(),name)){
                return i;
            }
        }
    }
    return 0;
}

uint16_t ElfFile::findSectionIdx(uint64_t addr){
    for (uint16_t i = 1; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_addr) == addr){
            return i;
        }
    }
    return 0;
}


void ElfFile::initTextSections(){
    uint32_t numberOfFunctions = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_TextSection){
            TextSection* textSection = (TextSection*)rawSections[i];
            numberOfFunctions += textSection->findFunctions();
        }
    }
}

bool ElfFile::verify(){

    if (!getFileHeader()){
        PRINT_ERROR("File Header should exist");
        return false;
    }

    if (numberOfPrograms != getFileHeader()->GET(e_phnum)){
        PRINT_ERROR("Number of segments in file header is inconsistent with our internal count");
        return false;
    }
    if (numberOfSections != getFileHeader()->GET(e_shnum)){
        PRINT_ERROR("Number of sections in file header is inconsistent with our internal count");
        return false;
    }
    
    // verify that there is only 1 text and 1 data segment
    uint32_t textSegCount = 0;
    uint32_t dataSegCount = 0;
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        ProgramHeader* phdr = getProgramHeader(i);
        if (!phdr){
            PRINT_ERROR("Program header %d should exist", i)
        }
        if (phdr->GET(p_type) == PT_LOAD){
            if (phdr->isReadable() && phdr->isExecutable()){
                textSegmentIdx = i;
                textSegCount++;
            } else if (phdr->isReadable() && phdr->isWritable()){
                dataSegmentIdx = i;
                dataSegCount++;
            } else {
                PRINT_ERROR("Segment(%d) with type PT_LOAD has attributes that are not consistent with text or data");
                return false;
            }
        }
    }
    if (textSegCount != 1){
        PRINT_ERROR("Exactly 1 loadable text segment must be present, %d found", textSegCount);
        return false;
    }
    if (dataSegCount != 1){
        PRINT_ERROR("Exactly 1 loadable data segment must be present, %d found", dataSegCount);
        return false;
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


    uint64_t hashSectionAddress_DT = dynamicTable->getDynamicByType(DT_HASH,0)->GET_A(d_val,d_un);
    uint64_t dynstrSectionAddress_DT = dynamicTable->getDynamicByType(DT_STRTAB,0)->GET_A(d_val,d_un);
    uint64_t dynsymSectionAddress_DT = dynamicTable->getDynamicByType(DT_SYMTAB,0)->GET_A(d_val,d_un);
    if (dynamicTable->countDynamics(DT_REL) + dynamicTable->countDynamics(DT_RELA) != 1){
        PRINT_ERROR("Can only have one relocation table referenced by the dynamic table");
        return false;
    }
    uint64_t relocationSectionAddress_DT;
    if (dynamicTable->countDynamics(DT_REL)){
        relocationSectionAddress_DT = dynamicTable->getDynamicByType(DT_REL,0)->GET_A(d_val,d_un);
    } else {
        relocationSectionAddress_DT = dynamicTable->getDynamicByType(DT_RELA,0)->GET_A(d_val,d_un);
    }


    // here we will enforce an ordering on the sections in the file.
    // The file must start with note and interp sections
    uint64_t hashSectionAddress = 0;
    uint64_t dynamicSectionAddress = 0;
    uint64_t dynstrSectionAddress = 0;
    uint64_t dynsymSectionAddress = 0;
    uint64_t textSectionAddress = 0;
    uint64_t relocationSectionAddress = 0;
    uint64_t pltgotSectionAddress = 0;
    uint64_t versymSectionAddress = 0;
    uint64_t verneedSectionAddress = 0;

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (!sectionHeaders[i]){
            PRINT_ERROR("Section header %d should exist", i);
            return false;
        }
        if (sectionHeaders[i]->getIndex() != i){
            PRINT_ERROR("Section header indices should match their order in the elf file");
            return false;
        }

        if (!rawSections[i]){
            PRINT_ERROR("Raw section %d should exist", i);
            return false;
        }
        if (rawSections[i]->getSectionIndex() != i){
            PRINT_ERROR("Raw section index %d should match its order (%d) in the elf file", rawSections[i]->getSectionIndex(), i);
            return false;
        }

        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_HashTable){
            if (hashSectionAddress){
                PRINT_ERROR("Cannot have more than one hash section");
                return false;
            }
            if (dynstrSectionAddress){
                PRINT_ERROR("Hash table should come before dynamic string table");
                return false;
            }
            hashSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_SymbolTable &&
            sectionHeaders[i]->GET(sh_type) == SHT_DYNSYM){
            if (dynsymSectionAddress){
                PRINT_ERROR("Cannot have more than one dynamic symbol table -- already found one at 0x%016llx", dynsymSectionAddress);
                return false;
            }
            if (dynstrSectionAddress){
                PRINT_ERROR("Dynamic symbol table should come before dynamic string table");
                return false;
            }
            dynsymSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_StringTable &&
            sectionHeaders[i]->GET(sh_addr) == dynstrSectionAddress_DT){
            if (dynstrSectionAddress){
                PRINT_ERROR("Cannot have more than one dynamic string table");
                return false;
            }
            if (relocationSectionAddress){
                PRINT_ERROR("Dynamic string table should come before versym table");
                return false;
            }
            dynstrSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_GNU_versym){
            if (versymSectionAddress){
                PRINT_ERROR("Cannot have more than one versym section");
                return false;
            }
            if (verneedSectionAddress){
                PRINT_ERROR("Versym section should come before verneed section");
                return false;
            }
            versymSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->GET(sh_type) == SHT_GNU_verneed){
            if (verneedSectionAddress){
                PRINT_ERROR("Cannot have more than one verneed section");
                return false;
            }
            if (verneedSectionAddress){
                PRINT_ERROR("Verneed section should come before plt/got section");
                return false;
            }
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_RelocationTable &&
            sectionHeaders[i]->GET(sh_addr) == relocationSectionAddress_DT){
            if (relocationSectionAddress){
                PRINT_ERROR("Cannot have more than one relocation table");
                return false;
            }
            if (pltgotSectionAddress){
                PRINT_ERROR("Relocation table should come before plt/got section");
                return false;
            }
            relocationSectionAddress = sectionHeaders[i]->GET(sh_addr);
        }

    }

    PriorityQueue<uint64_t,uint64_t> addrs = PriorityQueue<uint64_t,uint64_t>(numberOfSections+3);
    addrs.insert(fileHeader->GET(e_ehsize),0);
    addrs.insert(fileHeader->GET(e_phentsize)*fileHeader->GET(e_phnum),fileHeader->GET(e_phoff));
    addrs.insert(fileHeader->GET(e_shentsize)*fileHeader->GET(e_shnum),fileHeader->GET(e_shoff));
    for (uint32_t i = 1; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_type) != SHT_NOBITS){
            addrs.insert(sectionHeaders[i]->GET(sh_size),sectionHeaders[i]->GET(sh_offset));
        }
    }
    ASSERT(addrs.size() && "This queue should not be empty");

    uint64_t prevBegin, prevSize;
    uint64_t currBegin, currSize;
    prevSize = addrs.deleteMin(&prevBegin);
    while (addrs.size()){
        currSize = addrs.deleteMin(&currBegin);
        //        PRINT_INFOR("Verifying address ranges [%llx,%llx],[%llx,%llx]", prevBegin, prevBegin+prevSize, currBegin, currBegin+currSize);
        if (prevBegin+prevSize > currBegin && currSize != 0){
            PRINT_ERROR("Address ranges [%llx,%llx],[%llx,%llx] should not intersect", prevBegin, prevBegin+prevSize, currBegin, currBegin+currSize);
            return false;
        }
        prevBegin = currBegin;
        prevSize = currSize;
    }


    for (uint32_t i = 0; i < numberOfPrograms; i++){
        getProgramHeader(i)->verify();
    }
    for (uint32_t i = 0; i < numberOfSections; i++){
        getSectionHeader(i)->verify();
        getRawSection(i)->verify();
    }

    return true;

}


uint64_t ElfFile::addSection(uint16_t idx, ElfClassTypes classtype, char* bytes, uint32_t name, uint32_t type, 
                             uint64_t flags, uint64_t addr, uint64_t offset, uint64_t size, uint32_t link, 
                             uint32_t info, uint64_t addralign, uint64_t entsize){

    ASSERT(sectionHeaders && "sectionHeaders should be initialized");
    ASSERT(rawSections && "rawSections should be initialized");

    SectionHeader** newSectionHeaders = new SectionHeader*[numberOfSections+1];
    RawSection** newRawSections = new RawSection*[numberOfSections+1];
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (i < idx){
            newSectionHeaders[i] = sectionHeaders[i];
            newRawSections[i] = rawSections[i];
        } else {
            newSectionHeaders[i+1] = sectionHeaders[i];
            newSectionHeaders[i+1]->setIndex(i+1);
            newRawSections[i+1] = rawSections[i];
            newRawSections[i+1]->setSectionIndex(newRawSections[i+1]->getSectionIndex()+1);
        }
    }

    if (is64Bit()){
        newSectionHeaders[idx] = new SectionHeader64(idx);
    } else {
        newSectionHeaders[idx] = new SectionHeader32(idx);
    }

    newSectionHeaders[idx]->SET(sh_name,name);
    newSectionHeaders[idx]->SET(sh_type,type);
    newSectionHeaders[idx]->SET(sh_flags,flags);
    newSectionHeaders[idx]->SET(sh_addr,addr);
    newSectionHeaders[idx]->SET(sh_offset,offset);
    newSectionHeaders[idx]->SET(sh_size,size);
    newSectionHeaders[idx]->SET(sh_link,link);
    newSectionHeaders[idx]->SET(sh_info,info);
    newSectionHeaders[idx]->SET(sh_addralign,addralign);
    newSectionHeaders[idx]->SET(sh_entsize,entsize);
    newSectionHeaders[idx]->setSectionType();

    if (classtype == ElfClassTypes_TextSection){
        TextSection** newTextSections = new TextSection*[numberOfTextSections+1];
        newTextSections[numberOfTextSections] = new TextSection(bytes,size,idx,numberOfTextSections,this);
        newRawSections[idx] = (RawSection*)newTextSections[numberOfTextSections];
        for (uint32_t i = 0; i < numberOfTextSections; i++){
            newTextSections[i] = textSections[i];
        }
        delete[] textSections;
        numberOfTextSections++;
        textSections = newTextSections;
    } else {
        newRawSections[idx] = new RawSection(classtype,bytes,0,idx,this);
    }

    delete[] sectionHeaders;
    sectionHeaders = newSectionHeaders;

    delete[] rawSections;
    rawSections = newRawSections;

    numberOfSections++;

    // increment the number of sections in the file header
    getFileHeader()->INCREMENT(e_shnum,1);

    // if the section header string table moved, increment the pointer to it
    if (idx < getFileHeader()->GET(e_shstrndx)){
        getFileHeader()->INCREMENT(e_shstrndx,1);
    }

    // update the section header links to section indices
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_link) >= idx && sectionHeaders[i]->GET(sh_link) < numberOfSections){
            sectionHeaders[i]->INCREMENT(sh_link,1);
        }
    }

    // update the symbol table links to section indices
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        SymbolTable* symtab = getSymbolTable(i);
        for (uint32_t j = 0; j < symtab->getNumberOfSymbols(); j++){
            Symbol* sym = symtab->getSymbol(j);
            if (sym->GET(st_shndx) >= idx && sym->GET(st_shndx) < numberOfSections){
                sym->INCREMENT(st_shndx,1);
            }
        }
    }

    // if any sections fall after the section header table, update their offset to give room for the new entry in the
    for (uint32_t i = 0; i < numberOfSections; i++){
        uint64_t currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (currentOffset > fileHeader->GET(e_shoff)){
            uint64_t newOffset = nextAlignAddress(currentOffset+fileHeader->GET(e_shentsize),sectionHeaders[i]->GET(sh_addralign));
            sectionHeaders[i]->SET(sh_offset,newOffset);
        }
    }

    return sectionHeaders[idx]->GET(sh_addr);
}


// this will sort the sections header by their offsets in increasing order
void ElfFile::sortSectionHeaders(){
    SectionHeader* tmp;

    // we will just use a bubble sort since there shouldn't be very many sections
    for (uint32_t i = 0; i < numberOfSections; i++){
        for (uint32_t j = 0; j < i; j++){
            if (sectionHeaders[i]->GET(sh_offset) < sectionHeaders[j]->GET(sh_offset)){
                tmp = sectionHeaders[i];
                sectionHeaders[i] = sectionHeaders[j];
                sectionHeaders[j] = tmp;
            }
        }
    }
}


void ElfFile::initSectionFilePointers(){

    ASSERT(hashTable && "Hash Table should exist");
    hashTable->initFilePointers();
    
    // find the string table for section names
    ASSERT(fileHeader->GET(e_shstrndx) && "No section name string table");
    for (uint32_t i = 0; i < numberOfStringTables; i++){
        if (stringTables[i]->getSectionIndex() == fileHeader->GET(e_shstrndx)){
            sectionNameStrTabIdx = i;
        }
    }

    // set section names
    ASSERT(sectionHeaders && "Section headers not present");
    ASSERT(sectionNameStrTabIdx && "Section header string table index must be defined");

    char* stringTablePtr = getStringTable(sectionNameStrTabIdx)->getFilePointer();

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

    // find the global offset table's address
    uint64_t gotBaseAddress = 0;
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        SymbolTable* currentSymtab = getSymbolTable(i);
        for (uint32_t j = 0; j < currentSymtab->getNumberOfSymbols(); j++){

            // yes, we actually have to look for this symbol's name to find it!
            char* symName = currentSymtab->getSymbolName(j);
            if (!strcmp(symName,GOT_SYM_NAME)){
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
    dynamicSectionAddress = 0;
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

    // find the dynamic table
    dynamicTableSectionIdx = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_addr) == dynamicSectionAddress){
            ASSERT(!dynamicTableSectionIdx && "Cannot have multiple dynamic sections");
            dynamicTableSectionIdx = i;
        }
    }
    ASSERT(dynamicTableSectionIdx && "Cannot find a section for the dynamic table");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->GET(sh_type) == SHT_DYNAMIC && "Dynamic Section section header is wrong type");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->hasAllocBit() && "Dynamic Section section header missing an attribute");


    uint16_t dynamicSegmentIdx = 0;
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        if (programHeaders[i]->GET(p_type) == PT_DYNAMIC){
            ASSERT(!dynamicSegmentIdx && "Cannot have multiple segments for the dynamic section");
            dynamicSegmentIdx = i;
        }
    }
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

    // find certain sections whose addresses are in the dynamic table
    uint64_t strtabAddr = dynamicTable->getDynamicByType(DT_STRTAB,0)->GET_A(d_ptr,d_un);
    uint64_t symtabAddr = dynamicTable->getDynamicByType(DT_SYMTAB,0)->GET_A(d_ptr,d_un);
    uint64_t pltreltabAddr = dynamicTable->getDynamicByType(DT_JMPREL,0)->GET_A(d_ptr,d_un);
    uint32_t reltype;
    if (dynamicTable->countDynamics(DT_REL) > 0){
        reltype = DT_REL;
    } else {
        reltype = DT_RELA;
    }
    uint64_t reltabAddr = dynamicTable->getDynamicByType(reltype,0)->GET_A(d_ptr,d_un);

    for (uint32_t i = 0; i < numberOfSections; i++){
        if (rawSections[i]->getType() == ElfClassTypes_StringTable &&
            sectionHeaders[i]->GET(sh_addr) == strtabAddr){
            dynamicStringTable = (StringTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == ElfClassTypes_SymbolTable &&
            sectionHeaders[i]->GET(sh_addr) == symtabAddr){
            dynamicSymbolTable = (SymbolTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == ElfClassTypes_RelocationTable &&
            sectionHeaders[i]->GET(sh_addr) == pltreltabAddr){
            pltRelocationTable = (RelocationTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == ElfClassTypes_RelocationTable &&
            sectionHeaders[i]->GET(sh_addr) == reltabAddr){
            dynamicRelocationTable = (RelocationTable*)rawSections[i];
        }
    }
    ASSERT(dynamicStringTable && "Should be able to find the dynamic string table");
    ASSERT(dynamicSymbolTable && "Should be able to find the dynamic symbol table");
    ASSERT(pltRelocationTable && "Should be able to find the plt relocation table");
    ASSERT(dynamicRelocationTable && "Should be able to find the dynamic relocation table");


}


uint32_t ElfFile::findSymbol4Addr(uint64_t addr,Symbol** buffer,uint32_t bufCnt,char** namestr){
    uint32_t retValue = 0;
    if(namestr){
        *namestr = new char[__MAX_STRING_SIZE+2];
        **namestr = '\0';
    }

    if (symbolTables){
        for (uint32_t i = 0; i < numberOfSymbolTables; i++){
            if (symbolTables[i]){
                char* localnames = NULL;
                uint32_t cnt = symbolTables[i]->findSymbol4Addr(addr,buffer,bufCnt,&localnames);
                if(cnt){
                    if((__MAX_STRING_SIZE-strlen(*namestr)) > strlen(localnames)){
                        sprintf(*namestr+strlen(*namestr),"%s ",localnames);
                    }
                }
                delete[] localnames;
            }
        }
    }
    if(namestr && !strlen(*namestr)){
        sprintf(*namestr,"<__no_symbol_found>");
    }
    return retValue;
}

void ElfFile::print(){
    print(Print_Code_All);
}

void ElfFile::print(uint32_t printCodes) 
{ 

    if (HAS_PRINT_CODE(printCodes,Print_Code_FileHeader)){
        PRINT_INFOR("File Header");
        PRINT_INFOR("===========");
        if(fileHeader){
            fileHeader->print(); 
        } else {
            PRINT_WARN("\tNo File Header Found");
        }
    }
    
    
    if (HAS_PRINT_CODE(printCodes,Print_Code_SectionHeader)){
        PRINT_INFOR("Section Headers: %d",numberOfSections);
        PRINT_INFOR("=================");
        if (sectionHeaders){
            for (uint32_t i = 0; i < numberOfSections; i++){
                if (sectionHeaders[i]){
                    sectionHeaders[i]->print();
                }
            }
        } else {
            PRINT_WARN("\tNo Section Headers Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_ProgramHeader)){
        PRINT_INFOR("Program Headers: %d",numberOfPrograms);
        PRINT_INFOR("================");
        if (programHeaders){
            for (uint32_t i = 0; i < numberOfPrograms; i++){
                if (programHeaders[i]){
                    programHeaders[i]->print();
                }
            }
        } else {
            PRINT_WARN("\tNo Program Headers Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_NoteSection)){
        PRINT_INFOR("Note Sections: %d",numberOfNoteSections);
        PRINT_INFOR("=================");
        if (noteSections){
            for (uint32_t i = 0; i < numberOfNoteSections; i++){
                noteSections[i]->print();
            }
        } else {
            PRINT_WARN("\tNo Note Sections Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_SymbolTable)){
        PRINT_INFOR("Symbol Tables: %d",numberOfSymbolTables);
        PRINT_INFOR("==============");
        if (symbolTables){
            for (uint32_t i = 0; i < numberOfSymbolTables; i++){
                if (symbolTables[i])
                    symbolTables[i]->print();
            }
        } else {
            PRINT_WARN("\tNo Symbol Tables Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_RelocationTable)){
        PRINT_INFOR("Relocation Tables: %d",numberOfRelocationTables);
        PRINT_INFOR("==================");
        if (relocationTables){
            for (uint32_t i = 0; i < numberOfRelocationTables; i++){
                if (relocationTables[i]){
                    relocationTables[i]->print();
                }
            }
        } else {
            PRINT_WARN("\tNo Relocation Tables Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_StringTable)){
        PRINT_INFOR("String Tables: %d",numberOfStringTables);
        PRINT_INFOR("==============");
        if (stringTables){
            for (uint32_t i = 0; i < numberOfStringTables; i++){       
                if (stringTables[i]){
                    stringTables[i]->print();
                }
            }
        } else {
            PRINT_WARN("\tNo String Tables Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GlobalOffsetTable)){
        PRINT_INFOR("Global Offset Table");
        PRINT_INFOR("===================");
        if (globalOffsetTable){
            globalOffsetTable->print();
        } else {
            PRINT_WARN("\tNo Global Offset Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_HashTable)){
        PRINT_INFOR("Hash Table");
        PRINT_INFOR("=============");
        if (hashTable){
            hashTable->print();
        } else {
            PRINT_WARN("\tNo Hash Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_DynamicTable)){
        PRINT_INFOR("Dynamic Table");
        PRINT_INFOR("=============");
        if (dynamicTable){
            dynamicTable->print();
            dynamicTable->printSharedLibraries(&binaryInputFile);
        } else {
            PRINT_WARN("\tNo Dynamic Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GnuVerneedTable)){
        PRINT_INFOR("Gnu Verneed Table");
        PRINT_INFOR("=============");
        if (gnuVerneedTable){
            gnuVerneedTable->print();
        } else {
            PRINT_WARN("\tNo GNU Version Needs Table  Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GnuVersymTable)){
        PRINT_INFOR("Gnu Versym Table");
        PRINT_INFOR("=============");
        if (gnuVersymTable){
            gnuVersymTable->print();
        } else {
            PRINT_WARN("\tNo GNU Version Symbol Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_Disassemble)){
        PRINT_INFOR("Text Disassembly");
        PRINT_INFOR("=============");
        if (HAS_PRINT_CODE(printCodes,Print_Code_Instruction)){
            printDisassembledCode(true);
        } else {
            printDisassembledCode(false);
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



uint32_t ElfFile::printDisassembledCode(bool instructionDetail){
    uint32_t numInstrs = 0;

    ASSERT(disassembler && "disassembler should be initialized");

    for (uint32_t i = 0; i < numberOfTextSections; i++){
        numInstrs += textSections[i]->printDisassembledCode(instructionDetail);
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
            return currByte;
        }
    }

    return 0;
}


void ElfFile::dump(char* extension){
    char fileName[80] = "";
    sprintf(fileName,"%s.%s", elfFileName, extension);

    PRINT_INFOR("Output file is %s", fileName);

    BinaryOutputFile binaryOutputFile;
    binaryOutputFile.open(fileName);
    if(!binaryOutputFile){
        PRINT_ERROR("The output file can not be opened %s",fileName);
    }

    dump(&binaryOutputFile,ELF_FILE_HEADER_OFFSET);

    binaryOutputFile.close();
}

void ElfFile::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(offset == ELF_FILE_HEADER_OFFSET && "Instrumentation must be dumped at the begining of the output file");
    uint32_t currentOffset = offset;

    fileHeader->dump(binaryOutputFile,currentOffset);

    currentOffset = fileHeader->GET(e_phoff);
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        programHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += programHeaders[i]->getSizeInBytes();
    }

    currentOffset = fileHeader->GET(e_shoff);
    for (uint32_t i = 0; i < numberOfSections; i++){
        sectionHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += sectionHeaders[i]->getSizeInBytes();
    }

    for (uint32_t i = 0; i < numberOfSections; i++){
        currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (sectionHeaders[i]->hasBitsInFile()){
            rawSections[i]->dump(binaryOutputFile,currentOffset);
        }
    }
}


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
        PRINT_ERROR("The magic number [%02hhx%02hhx%02hhx%02hhx] is not a valid one",e_ident[EI_MAG0],e_ident[EI_MAG1],e_ident[EI_MAG2],e_ident[EI_MAG3]);
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
    disassembler = new Disassembler(is64Bit());
    readRawSections();
    initSectionFilePointers();
    initTextSections();

    verify();

}

void ElfFile::readFileHeader() {
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
    ASSERT(numberOfSections && "FATAL : This file has no segments!!!!!");
}

void ElfFile::readProgramHeaders(){

    programHeaders = new ProgramHeader*[numberOfPrograms];
    ASSERT(programHeaders);

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_phoff)));

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

    ASSERT(textSegmentIdx && "This file must contain a text segment");
    ASSERT(dataSegmentIdx && "This file must contain a data segment");
}

void ElfFile::readSectionHeaders(){

    sectionHeaders = new SectionHeader*[numberOfSections];
    ASSERT(sectionHeaders);

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_shoff)));

    // first read each section header
    for (uint32_t i = 0; i < numberOfSections; i++){
        if(is64Bit()){
            sectionHeaders[i] = new SectionHeader64(i);
        } else {
            sectionHeaders[i] = new SectionHeader32(i);
        }
        ASSERT(sectionHeaders[i]);
        sectionHeaders[i]->read(&binaryInputFile);
    }

    // determine and set section type for each section header
    for (uint32_t i = 0; i < numberOfSections; i++){
        uint32_t typ = sectionHeaders[i]->getSectionType();
        switch(typ){
        case (ElfClassTypes_StringTable) : numberOfStringTables++;
        case (ElfClassTypes_SymbolTable) : numberOfSymbolTables++;
        case (ElfClassTypes_RelocationTable) : numberOfRelocationTables++;
        case (ElfClassTypes_DwarfSection)  : numberOfDwarfSections++;
        case (ElfClassTypes_TextSection) : numberOfTextSections++;
        case (ElfClassTypes_NoteSection) : numberOfNoteSections++;
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

        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_StringTable){
            rawSections[i] = new StringTable(sectionFilePtr, sectionSize, i, numberOfStringTables, this);
            stringTables[numberOfStringTables] = (StringTable*)rawSections[i];
            numberOfStringTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_SymbolTable){
            rawSections[i] = new SymbolTable(sectionFilePtr, sectionSize, i, numberOfSymbolTables, this);
            symbolTables[numberOfSymbolTables] = (SymbolTable*)rawSections[i];
            numberOfSymbolTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_RelocationTable){
            rawSections[i] = new RelocationTable(sectionFilePtr, sectionSize, i, numberOfRelocationTables, this);
            relocationTables[numberOfRelocationTables] = (RelocationTable*)rawSections[i];
            numberOfRelocationTables++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_DwarfSection){
            rawSections[i] = new DwarfSection(sectionFilePtr, sectionSize, i, numberOfDwarfSections, this);
            dwarfSections[numberOfDwarfSections] = (DwarfSection*)rawSections[i];
            numberOfDwarfSections++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_TextSection){
            rawSections[i] = new TextSection(sectionFilePtr, sectionSize, i, numberOfTextSections, this);
            textSections[numberOfTextSections] = (TextSection*)rawSections[i];
            numberOfTextSections++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_HashTable){
            ASSERT(!hashTable && "Cannot have multiple hash table sections");
            rawSections[i] = new HashTable(sectionFilePtr, sectionSize, i, this);
            hashTable = (HashTable*)rawSections[i];
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_NoteSection){
            rawSections[i] = new NoteSection(sectionFilePtr, sectionSize, i, numberOfNoteSections, this);
            noteSections[numberOfNoteSections] = (NoteSection*)rawSections[i];
            numberOfNoteSections++;
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_GnuVerneedTable){
            ASSERT(!gnuVerneedTable && "Cannot have more than one GNU_verneed section");
            rawSections[i] = new GnuVerneedTable(sectionFilePtr, sectionSize, i, this);
            gnuVerneedTable = (GnuVerneedTable*)rawSections[i];
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_GnuVersymTable){
            ASSERT(!gnuVersymTable && "Cannot have more than one GNU_versym section");
            rawSections[i] = new GnuVersymTable(sectionFilePtr, sectionSize, i, this);
            gnuVersymTable = (GnuVersymTable*)rawSections[i];
        } else {
            rawSections[i] = new RawSection(ElfClassTypes_no_type, sectionFilePtr, sectionSize, i, this);
        }
        rawSections[i]->read(&binaryInputFile);
    }
}

ElfFile::~ElfFile(){
    if (disassembler){
        delete disassembler;
    }
    if (fileHeader){
        delete fileHeader;
    }
    if (programHeaders){
        for (uint32_t i = 0; i < numberOfPrograms; i++){
            if (programHeaders[i]){
                delete programHeaders[i];
            }
        }
        delete[] programHeaders;
    }
    if (sectionHeaders){
        for (uint32_t i = 0; i < numberOfSections; i++){
            if (sectionHeaders[i]){
                delete sectionHeaders[i];
            }
        }
        delete[] sectionHeaders;
    }
    if (rawSections){
        for (uint32_t i = 0; i < numberOfSections; i++){
            if (rawSections[i]){
                delete rawSections[i];
            }
        }
        delete[] rawSections;
    }

    // just delete the tables, the actual section pointers were deleted as rawSection pointers above
    if (stringTables){
        delete[] stringTables;
    }
    if (symbolTables){
        delete[] symbolTables;
    }
    if (relocationTables){
        delete[] relocationTables;
    }
    if (dwarfSections){
        delete[] dwarfSections;
    }
    if (textSections){
        delete[] textSections;
    }
    if (noteSections){
        delete[] noteSections;
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


