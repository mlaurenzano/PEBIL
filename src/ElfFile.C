#include <ElfFile.h>

#include <Base.h>
#include <BasicBlock.h>
#include <BinaryFile.h>
#include <DwarfSection.h>
#include <DynamicTable.h>
#include <FileHeader.h>
#include <FlowGraph.h>
#include <Function.h>
#include <GlobalOffsetTable.h>
#include <GnuVersion.h>
#include <HashTable.h>
#include <Instruction.h>
#include <LineInformation.h>
#include <NoteSection.h>
#include <PriorityQueue.h>
#include <ProgramHeader.h>
#include <RawSection.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <TextSection.h>

DEBUG(
uint32_t readBytes = 0;
);

void ElfFile::gatherDisassemblyStats(){
    STATS(totalFunctions = 0);
    STATS(totalBlocks = 0);
    STATS(totalFunctionBytes = 0);
    STATS(totalBlockBytes = 0);

    STATS(functionsCovered = 0);
    STATS(functionBytesCovered = 0);
    STATS(blocksCovered = 0);
    STATS(blockBytesCovered = 0);

    uint32_t numberOfTextSections = getNumberOfTextSections();
    for (uint32_t i = 0; i < numberOfTextSections; i++){
        TextSection* ts = getTextSection(i);
        for (uint32_t j = 0; j < ts->getNumberOfTextObjects(); j++){
            if (ts->getTextObject(j)->getType() == ElfClassTypes_Function){
                Function* func = (Function*)ts->getTextObject(j);
                STATS(totalFunctions++);
                STATS(totalFunctionBytes += func->getSizeInBytes());
                if (func->hasCompleteDisassembly()){
                    STATS(functionsCovered++);
                    STATS(functionBytesCovered += func->getSizeInBytes());
                }
                for (uint32_t k = 0; k < func->getFlowGraph()->getNumberOfBasicBlocks(); k++){
                    BasicBlock* bb = func->getFlowGraph()->getBasicBlock(k);
                    STATS(totalBlocks++);
                    STATS(totalBlockBytes += bb->getNumberOfBytes());
                    if (func->hasCompleteDisassembly()){
                        STATS(blocksCovered++);
                        STATS(blockBytesCovered += bb->getNumberOfBytes());
                    }
                }

            }
        }
    }

    STATS(float ratio1, ratio2);
    STATS(ratio1 = (float)functionsCovered / (float)totalFunctions * 100.0);
    STATS(ratio2 = (float)functionBytesCovered / (float)totalFunctionBytes * 100.0);
    STATS(PRINT_INFOR("___stats: Disasm comprehension: %d out of %d functions (%.2f\%); %d out of %d bytes (%.2f\%)", functionsCovered, totalFunctions, ratio1, functionBytesCovered, totalFunctionBytes, ratio2));

    STATS(ratio1 = (float)blocksCovered / (float)totalBlocks * 100.0);
    STATS(ratio2 = (float)blockBytesCovered / (float)totalBlockBytes * 100.0);
    STATS(PRINT_INFOR("___stats: Disasm comprehension: %d out of %d blocks (%.2f\%); %d out of %d bytes (%.2f\%)", blocksCovered, totalBlocks, ratio1, blockBytesCovered, totalBlockBytes, ratio2));

}


RawSection* ElfFile::findDataSectionAtAddr(uint64_t addr){
    RawSection* dataSection = NULL;
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
        if (getSectionHeader(i)->inRange(addr)){
            dataSection = getRawSection(i);
        }
    }
    return dataSection;
}

uint16_t ElfFile::findSectionIdx(char* name){
    for (uint16_t i = 1; i < getNumberOfSections(); i++){
        if (name && sectionHeaders[i]->getSectionNamePtr()){
            if (!strcmp(sectionHeaders[i]->getSectionNamePtr(),name)){
                return i;
            }
        }
    }
    return 0;
}

uint16_t ElfFile::findSectionIdx(uint64_t addr){
    for (uint16_t i = 1; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]->GET(sh_addr) == addr){
            return i;
        }
    }
    return 0;
}


bool ElfFile::verify(){

    if (!getFileHeader()){
        PRINT_ERROR("File Header should exist");
        return false;
    }

    if (getNumberOfPrograms() != getFileHeader()->GET(e_phnum)){
        PRINT_ERROR("Number of segments in file header is inconsistent with our internal count");
        return false;
    }
    if (getNumberOfSections() != getFileHeader()->GET(e_shnum)){
        PRINT_ERROR("Number of sections in file header is inconsistent with our internal count");
        return false;
    }
    
    // verify that there is only 1 text and 1 data segment
    uint32_t textSegCount = 0;
    uint32_t dataSegCount = 0;
    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        ProgramHeader* phdr = getProgramHeader(i);
        if (!phdr){
            PRINT_ERROR("Program header %d should exist", i)
                return false;
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
    uint32_t ptInterpIdx = getNumberOfPrograms();
    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        if (programHeaders[i]->GET(p_type) == PT_INTERP){
            if (ptInterpIdx < getNumberOfPrograms()){
                PRINT_ERROR("Cannot have multiple PT_INTERP segments");
                return false;
            }
            ptInterpIdx = i;
        }
    }

    if (isStaticLinked()){
        if (ptInterpIdx != getNumberOfPrograms()){
            PRINT_ERROR("Static linked executables shouldn't have a PT_INTERP segment");
            return false;
        }
    } else {
        for (uint32_t i = 0; i < getNumberOfPrograms(); i++){    
            if (programHeaders[i]->GET(p_type) == PT_LOAD){
                if (i < ptInterpIdx){
                    PRINT_ERROR("PT_INTERP segment must preceed any loadable segment");
                    return false;
                }
            }
        }
    }

    PriorityQueue<uint64_t,uint64_t> addrs = PriorityQueue<uint64_t,uint64_t>(getNumberOfSections()+3);
    addrs.insert(fileHeader->GET(e_ehsize),0);
    addrs.insert(fileHeader->GET(e_phentsize)*fileHeader->GET(e_phnum),fileHeader->GET(e_phoff));
    addrs.insert(fileHeader->GET(e_shentsize)*fileHeader->GET(e_shnum),fileHeader->GET(e_shoff));
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
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


    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        if (!getProgramHeader(i)->verify()){
            return false;
        }
    }
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (!getSectionHeader(i)->verify()){
            return false;
        }
        if (!getRawSection(i)->verify()){
            return false;
        }
    }

    if (!isStaticLinked()){
        if (!verifyDynamic()){
            return false;
        }
    }

    return true;
}

bool ElfFile::verifyDynamic(){
    
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

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
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

    return true;

}


uint64_t ElfFile::addSection(uint16_t idx, ElfClassTypes classtype, char* bytes, uint32_t name, uint32_t type, 
                             uint64_t flags, uint64_t addr, uint64_t offset, uint64_t size, uint32_t link, 
                             uint32_t info, uint64_t addralign, uint64_t entsize){

    if (is64Bit()){
        sectionHeaders.insert(new SectionHeader64(idx), idx);
    } else {
        sectionHeaders.insert(new SectionHeader32(idx), idx);
    }

    sectionHeaders[idx]->SET(sh_name,name);
    sectionHeaders[idx]->SET(sh_type,type);
    sectionHeaders[idx]->SET(sh_flags,flags);
    sectionHeaders[idx]->SET(sh_addr,addr);
    sectionHeaders[idx]->SET(sh_offset,offset);
    sectionHeaders[idx]->SET(sh_size,size);
    sectionHeaders[idx]->SET(sh_link,link);
    sectionHeaders[idx]->SET(sh_info,info);
    sectionHeaders[idx]->SET(sh_addralign,addralign);
    sectionHeaders[idx]->SET(sh_entsize,entsize);
    sectionHeaders[idx]->setSectionType();

    if (classtype == ElfClassTypes_TextSection){
        textSections.append(new TextSection(bytes, size, idx, getNumberOfTextSections(), this, ByteSource_Instrumentation));
        rawSections.insert((RawSection*)textSections.back(), idx);
    } else {
        rawSections.insert(new RawSection(classtype, bytes, 0, idx, this), idx);
    }

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        sectionHeaders[i]->setIndex(i);
        rawSections[i]->setSectionIndex(i);
    }

    // increment the number of sections in the file header
    getFileHeader()->INCREMENT(e_shnum,1);

    // if the section header string table moved, increment the pointer to it
    if (idx < getFileHeader()->GET(e_shstrndx)){
        getFileHeader()->INCREMENT(e_shstrndx,1);
    }

    // update the section header links to section indices
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]->GET(sh_link) >= idx && sectionHeaders[i]->GET(sh_link) < getNumberOfSections()){
            sectionHeaders[i]->INCREMENT(sh_link,1);
        }
    }

    // update the symbol table links to section indices
    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
        SymbolTable* symtab = getSymbolTable(i);
        for (uint32_t j = 0; j < symtab->getNumberOfSymbols(); j++){
            Symbol* sym = symtab->getSymbol(j);
            if (sym->GET(st_shndx) >= idx && sym->GET(st_shndx) < getNumberOfSections()){
                sym->INCREMENT(st_shndx,1);
            }
        }
    }

    // if any sections fall after the section header table, update their offset to give room for the new entry in the
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
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

    // we will just use a bubble sort (for now) since there shouldn't be very many sections
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
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

    /*
    // find the string table for section names
    ASSERT(fileHeader->GET(e_shstrndx) && "No section name string table");
    PRINT_INFOR("%d", fileHeader->GET(e_shstrndx));
    for (uint32_t i = 0; i < getNumberOfStringTables(); i++){
        PRINT_INFOR("%d %d", fileHeader->GET(e_shstrndx), stringTables[i]->getSectionIndex());
        if (stringTables[i]->getSectionIndex() == fileHeader->GET(e_shstrndx)){
            sectionNameStrTabIdx = i;
            PRINT_INFOR("yes");
        }
    }

    // set section names
    ASSERT(sectionNameStrTabIdx && "Section header string table index must be defined");
    ASSERT(
    */

    char* stringTablePtr = ((StringTable*)rawSections[fileHeader->GET(e_shstrndx)])->getFilePointer();

    // skip first section header since it is reserved and its values are null
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
        ASSERT(sectionHeaders[i]->getSectionNamePtr() == NULL && "Section Header name shouldn't already be set");
        uint32_t sectionNameOffset = sectionHeaders[i]->GET(sh_name);
        sectionHeaders[i]->setSectionNamePtr(stringTablePtr + sectionNameOffset);
    }

    // delineate the various dwarf sections
    uint32_t lineInfoIdx = 0;
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
        ASSERT(sectionHeaders[i]->getSectionNamePtr() && "Section header name should be set");
        if (!strcmp(sectionHeaders[i]->getSectionNamePtr(),DWARF_LINE_INFO_SCN_NAME)){
            ASSERT(!lineInfoIdx && "Cannot have multiple line information sections");
            lineInfoIdx = i;
        }
    }
    if (lineInfoIdx){
        char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[lineInfoIdx]->GET(sh_offset));
        uint64_t sectionSize = (uint64_t)sectionHeaders[lineInfoIdx]->GET(sh_size);

        ASSERT(sectionHeaders[lineInfoIdx]->getSectionType() == ElfClassTypes_DwarfSection);
        uint32_t dwarfIdx = ((DwarfSection*)rawSections[lineInfoIdx])->getIndex();
        delete rawSections[lineInfoIdx];

        lineInfoSection = new DwarfLineInfoSection(sectionFilePtr,sectionSize,lineInfoIdx,dwarfIdx,this);
        lineInfoSection->read(&binaryInputFile);
        rawSections[lineInfoIdx] = lineInfoSection;
    }


    // find the string table for each symbol table
    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
        getSymbolTable(i)->setStringTable();
    }

    // find the symbol table + relocation section for each relocation table
    for (uint32_t i = 0; i < getNumberOfRelocationTables(); i++){
        getRelocationTable(i)->setSymbolTable();
        getRelocationTable(i)->setRelocationSection();
    }


    if (!isStaticLinked()){
        initDynamicFilePointers();
    }

    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        textSections[i]->disassemble(&binaryInputFile);
        
        uint32_t binSizes[MAX_X86_INSTRUCTION_LENGTH+1];
        bzero(binSizes, sizeof(uint32_t)*(MAX_X86_INSTRUCTION_LENGTH+1));
        Instruction** allinsts = NULL;

#define BB_BINAMOUNT 32
        uint32_t bbSizes[BB_BINAMOUNT+1];
        bzero(bbSizes, sizeof(uint32_t)*(BB_BINAMOUNT+1));

        if (strstr(getSectionHeader(textSections[i]->getSectionIndex())->getSectionNamePtr(), ".text")){
            for (uint32_t j = 0; j < textSections[i]->getNumberOfTextObjects(); j++){
                allinsts = new Instruction*[textSections[i]->getTextObject(j)->getNumberOfInstructions()];
                textSections[i]->getTextObject(j)->getAllInstructions(allinsts, 0);
                for (uint32_t k = 0; k < textSections[i]->getTextObject(j)->getNumberOfInstructions(); k++){
                    binSizes[allinsts[k]->getSizeInBytes()]++;
                }
                delete[] allinsts;

                if (textSections[i]->getTextObject(j)->isFunction()){
                    Function* f = (Function*)textSections[i]->getTextObject(j);
                    for (uint32_t k = 0; k < f->getNumberOfBasicBlocks(); k++){
                        BasicBlock* bb = f->getBasicBlock(k);
                        if (bb->getSizeInBytes() > BB_BINAMOUNT){
                            bbSizes[BB_BINAMOUNT]++;
                        } else {
                            bbSizes[bb->getSizeInBytes()]++;
                        }
                    }
                }

            }
            
            PRINT_INFOR("Text Section %s instruction histogram", getSectionHeader(textSections[i]->getSectionIndex())->getSectionNamePtr());
            for (uint32_t j = 0; j < MAX_X86_INSTRUCTION_LENGTH+1; j++){
                fprintf(stdout, "%d\t", binSizes[j]);
            }
            fprintf(stdout, "\n");

            PRINT_INFOR("Text Section %s bb histogram", getSectionHeader(textSections[i]->getSectionIndex())->getSectionNamePtr());
            for (uint32_t j = 0; j < BB_BINAMOUNT+1; j++){
                fprintf(stdout, "%d\t", bbSizes[j]);
            }
            fprintf(stdout, "\n");

            fflush(stdout);
        }
    }

}


void ElfFile::initDynamicFilePointers(){

    if (hashTable){
        hashTable->initFilePointers();
    }

    ASSERT(hashTable && "Hash Table should exist");

    // find the dynamic symbol table
    dynamicSymtabIdx = getNumberOfSymbolTables();
    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
        if (getSymbolTable(i)->isDynamic()){
            ASSERT(dynamicSymtabIdx == getNumberOfSymbolTables() && "Cannot have multiple dynamic symbol tables");
            dynamicSymtabIdx = i;
        }

    }
    ASSERT(dynamicSymtabIdx != getNumberOfSymbolTables() && "Cannot analyze a file if it doesn't have a dynamic symbol table");

    // find the global offset table's address
    uint64_t gotBaseAddress = 0;
    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
        SymbolTable* currentSymtab = getSymbolTable(i);
        for (uint32_t j = 0; j < currentSymtab->getNumberOfSymbols(); j++){

            // yes, we actually have to look for this symbol's name to find it!
            char* symName = currentSymtab->getSymbolName(j);
            if (!strcmp(symName,GOT_SYM_NAME)){
                if (gotBaseAddress){
                    PRINT_WARN(4,"Found mutiple symbols for Global Offset Table (symbols named %s), addresses are 0x%016llx, 0x%016llx",
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
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
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
    ASSERT(sectionHeaders[gotSectionIdx]->getSectionType() == ElfClassTypes_RawSection);
    delete rawSections[gotSectionIdx];
    
    char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[gotSectionIdx]->GET(sh_offset));
    uint64_t sectionSize = (uint64_t)sectionHeaders[gotSectionIdx]->GET(sh_size);    
    rawSections[gotSectionIdx] = new GlobalOffsetTable(sectionFilePtr, sectionSize, gotSectionIdx, gotBaseAddress, this);
    ASSERT(!globalOffsetTable && "global offset table should not be initialized");
    globalOffsetTable = (GlobalOffsetTable*)rawSections[gotSectionIdx];
    globalOffsetTable->read(&binaryInputFile);
    

    // find the dynamic section's address
    dynamicSectionAddress = 0;
    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
        SymbolTable* currentSymtab = getSymbolTable(i);
        for (uint32_t j = 0; j < currentSymtab->getNumberOfSymbols(); j++){

            // yes, we actually have to look for this symbol's name to find it!
            char* symName = currentSymtab->getSymbolName(j);
            if (!strcmp(symName,DYN_SYM_NAME)){
                if (dynamicSectionAddress){
                    PRINT_WARN(4,"Found mutiple symbols for Dynamic Section (symbols named %s), addresses are 0x%016llx, 0x%016llx",
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
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]->GET(sh_addr) == dynamicSectionAddress){
            ASSERT(!dynamicTableSectionIdx && "Cannot have multiple dynamic sections");
            dynamicTableSectionIdx = i;
        }
    }
    ASSERT(dynamicTableSectionIdx && "Cannot find a section for the dynamic table");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->GET(sh_type) == SHT_DYNAMIC && "Dynamic Section section header is wrong type");
    ASSERT(getSectionHeader(dynamicTableSectionIdx)->hasAllocBit() && "Dynamic Section section header missing an attribute");


    uint16_t dynamicSegmentIdx = 0;
    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
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
    ASSERT(sectionHeaders[dynamicTableSectionIdx]->getSectionType() == ElfClassTypes_DynamicTable);
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

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
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

    for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
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
            PRINT_WARN(4,"\tNo File Header Found");
        }
    }
    
    
    if (HAS_PRINT_CODE(printCodes,Print_Code_SectionHeader)){
        PRINT_INFOR("Section Headers: %d",getNumberOfSections());
        PRINT_INFOR("=================");
        for (uint32_t i = 0; i < getNumberOfSections(); i++){
            if (sectionHeaders[i]){
                sectionHeaders[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_ProgramHeader)){
        PRINT_INFOR("Program Headers: %d",getNumberOfPrograms());
        PRINT_INFOR("================");
        for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
            if (programHeaders[i]){
                programHeaders[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_NoteSection)){
        PRINT_INFOR("Note Sections: %d",getNumberOfNoteSections());
        PRINT_INFOR("=================");
        for (uint32_t i = 0; i < getNumberOfNoteSections(); i++){
            if (noteSections[i]){
                noteSections[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_SymbolTable)){
        PRINT_INFOR("Symbol Tables: %d",getNumberOfSymbolTables());
        PRINT_INFOR("==============");
        for (uint32_t i = 0; i < getNumberOfSymbolTables(); i++){
            if (symbolTables[i]){
                symbolTables[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_RelocationTable)){
        PRINT_INFOR("Relocation Tables: %d",getNumberOfRelocationTables());
        PRINT_INFOR("==================");
        for (uint32_t i = 0; i < getNumberOfRelocationTables(); i++){
            if (relocationTables[i]){
                relocationTables[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_StringTable)){
        PRINT_INFOR("String Tables: %d",getNumberOfStringTables());
        PRINT_INFOR("==============");
        for (uint32_t i = 0; i < getNumberOfStringTables(); i++){       
            if (stringTables[i]){
                stringTables[i]->print();
            }
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GlobalOffsetTable)){
        PRINT_INFOR("Global Offset Table");
        PRINT_INFOR("===================");
        if (globalOffsetTable){
            globalOffsetTable->print();
        } else {
            PRINT_WARN(4,"\tNo Global Offset Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_HashTable)){
        PRINT_INFOR("Hash Table");
        PRINT_INFOR("=============");
        if (hashTable){
            hashTable->print();
        } else {
            PRINT_WARN(4,"\tNo Hash Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_DynamicTable)){
        PRINT_INFOR("Dynamic Table");
        PRINT_INFOR("=============");
        if (dynamicTable){
            dynamicTable->print();
            dynamicTable->printSharedLibraries(&binaryInputFile);
        } else {
            PRINT_WARN(4,"\tNo Dynamic Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GnuVerneedTable)){
        PRINT_INFOR("Gnu Verneed Table");
        PRINT_INFOR("=============");
        if (gnuVerneedTable){
            gnuVerneedTable->print();
        } else {
            PRINT_WARN(4,"\tNo GNU Version Needs Table  Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_GnuVersymTable)){
        PRINT_INFOR("Gnu Versym Table");
        PRINT_INFOR("=============");
        if (gnuVersymTable){
            gnuVersymTable->print();
        } else {
            PRINT_WARN(4,"\tNo GNU Version Symbol Table Found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_DwarfSection)){
        PRINT_INFOR("DWARF Debug Sections");
        PRINT_INFOR("=============");
        if (lineInfoSection){
            lineInfoSection->print();
        } else {
            PRINT_INFOR("No LineInfo Section found");
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_Disassemble)){
        PRINT_INFOR("Text Disassembly");
        PRINT_INFOR("=============");
        if (HAS_PRINT_CODE(printCodes,Print_Code_Instruction)){
            printDisassembly(true);
        } else {
            printDisassembly(false);
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_Loops)){
        PRINT_INFOR("Loop Summary");
        PRINT_INFOR("=============");
        for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
            textSections[i]->printLoops();
        }
    }

}


void ElfFile::findFunctions(){
/*
    ASSERT(symbolTable && "FATAL : Symbol table is missing");
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=getNumberOfSections();i++){
        rawSections[i]->findFunctions();
    }
*/
}



uint32_t ElfFile::printDisassembly(bool instructionDetail){
    uint32_t numInstrs = 0;

    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        if (textSections[i]){
            if (textSections[i]->getByteSource() != ByteSource_Instrumentation){
                numInstrs += textSections[i]->printDisassembly(instructionDetail);
            } else {
                PRINT_INFOR("Skipping print of section %hd because it is instrumentation code", textSections[i]->getSectionIndex());
            }
        }
    }
    return numInstrs;
}

/*
uint32_t ElfFile::findSectionNameInStrTab(char* name){
    if (!stringTables.size()){
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
*/

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
    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        programHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += programHeaders[i]->getSizeInBytes();
    }

    currentOffset = fileHeader->GET(e_shoff);
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        sectionHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += sectionHeaders[i]->getSizeInBytes();
    }

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (sectionHeaders[i]->hasBitsInFile()){
            rawSections[i]->dump(binaryOutputFile,currentOffset);
        }
    }
}


void ElfFile::parse(){

    TIMER(double t1 = timer());	

    char* endianCheck = "elfs\0";
    uint32_t endianValue = getUInt32(endianCheck);
    if (endianValue != 0x73666c65){
        PRINT_ERROR("Platform must be little endian");
    }
   
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
    readRawSections();

    if (hashTable){
        setStaticLinked(false);
        PRINT_INFOR("The executable is dynamically linked");
    } else {
        setStaticLinked(true);
        PRINT_INFOR("The executable is statically linked");
    }
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

    ASSERT(fileHeader->GET(e_flags) == EFINSTSTATUS_NON && "This executable appears to already be instrumented");
}

void ElfFile::readProgramHeaders(){

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_phoff)));

    bool foundText = false, foundData = false;
    for (uint32_t i = 0; i < fileHeader->GET(e_phnum); i++){
        if(is64Bit()){
            programHeaders.append(new ProgramHeader64(i));
        } else {
            programHeaders.append(new ProgramHeader32(i));
        }
        programHeaders[i]->read(&binaryInputFile);
        programHeaders[i]->verify();

        if (programHeaders[i]->GET(p_type) == PT_LOAD){
            if (programHeaders[i]->isReadable() && programHeaders[i]->isExecutable()){
                foundText = true;
                textSegmentIdx = i;
            } else if (programHeaders[i]->isReadable() && programHeaders[i]->isWritable()){
                foundData = true;
                dataSegmentIdx = i;
            }
        }
    }

    ASSERT(foundText && "This file must contain a text segment");
    ASSERT(foundData && "This file must contain a data segment");
}

void ElfFile::readSectionHeaders(){

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_shoff)));

    // first read each section header
    for (uint32_t i = 0; i < fileHeader->GET(e_shnum); i++){
        if(is64Bit()){
            sectionHeaders.append(new SectionHeader64(i));
        } else {
            sectionHeaders.append(new SectionHeader32(i));
        }
        ASSERT(sectionHeaders[i]);
        sectionHeaders[i]->read(&binaryInputFile);
    }

}

void ElfFile::readRawSections(){
    ASSERT(sectionHeaders.size() && "We should have read the section headers already");

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        char* sectionFilePtr = binaryInputFile.fileOffsetToPointer(sectionHeaders[i]->GET(sh_offset));
        uint64_t sectionSize = (uint64_t)sectionHeaders[i]->GET(sh_size);

        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_StringTable){
            rawSections.append(new StringTable(sectionFilePtr, sectionSize, i, getNumberOfStringTables(), this));
            stringTables.append((StringTable*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_SymbolTable){
            rawSections.append(new SymbolTable(sectionFilePtr, sectionSize, i, getNumberOfSymbolTables(), this));
            symbolTables.append((SymbolTable*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_RelocationTable){
            rawSections.append(new RelocationTable(sectionFilePtr, sectionSize, i, getNumberOfRelocationTables(), this));
            relocationTables.append((RelocationTable*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_DwarfSection){
            rawSections.append(new DwarfSection(sectionFilePtr, sectionSize, i, getNumberOfDwarfSections(), this));
            dwarfSections.append((DwarfSection*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_TextSection){
            rawSections.append(new TextSection(sectionFilePtr, sectionSize, i, getNumberOfTextSections(), this, ByteSource_Application));
            textSections.append((TextSection*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_HashTable){
            ASSERT(!hashTable && "Cannot have multiple hash table sections");
            rawSections.append(new HashTable(sectionFilePtr, sectionSize, i, this));
            hashTable = (HashTable*)rawSections.back();
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_NoteSection){
            rawSections.append(new NoteSection(sectionFilePtr, sectionSize, i, getNumberOfNoteSections(), this));
            noteSections.append((NoteSection*)rawSections.back());
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_GnuVerneedTable){
            ASSERT(!gnuVerneedTable && "Cannot have more than one GNU_verneed section");
            rawSections.append(new GnuVerneedTable(sectionFilePtr, sectionSize, i, this));
            gnuVerneedTable = (GnuVerneedTable*)rawSections.back();
        } else if (sectionHeaders[i]->getSectionType() == ElfClassTypes_GnuVersymTable){
            ASSERT(!gnuVersymTable && "Cannot have more than one GNU_versym section");
            rawSections.append(new GnuVersymTable(sectionFilePtr, sectionSize, i, this));
            gnuVersymTable = (GnuVersymTable*)rawSections.back();
        } else {
            rawSections.append(new RawSection(ElfClassTypes_no_type, sectionFilePtr, sectionSize, i, this));
        }
    }

    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        rawSections[i]->read(&binaryInputFile);
    }
}

ElfFile::~ElfFile(){
    if (fileHeader){
        delete fileHeader;
    }
    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        if (programHeaders[i]){
            delete programHeaders[i];
        }
    }
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]){
            delete sectionHeaders[i];
        }
    }
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (rawSections[i]){
            delete rawSections[i];
        }
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
        for(uint32_t i=1;i<=getNumberOfSections();i++){
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
    for(uint32_t i=1;i<=getNumberOfSections();i++){
        rawSections[i]->generateCFGs();
    }
*/
}

void ElfFile::findMemoryFloatOps(){
/*
    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=getNumberOfSections();i++){
        rawSections[i]->findMemoryFloatOps();
    }
*/
}

/*
RawSection* ElfFile::findRawSection(uint64_t addr){

    ASSERT(rawSections && "FATAL : Raw data is not read");
    for(uint32_t i=1;i<=getNumberOfSections();i++){
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

    if(sectionNo >= getNumberOfSections())
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
    for(uint32_t i=1;i<=getNumberOfSections();i++){
        uint32_t n = rawSections[i]->getAllBlocks(arr);
        arr += n;
        ret += n;
    }
    return ret;
}
*/
uint32_t ElfFile::getFileSize() { 
    return binaryInputFile.getSize(); 
}

/*
RelocationTable* ElfFile::getRelocationTable(uint32_t idx){ 
    return sectionHeaders[idx]->getRelocationTable(); 
}
LineInfoTable* ElfFile::getLineInfoTable(uint32_t idx){ 
    return sectionHeaders[idx]->getLineInfoTable(); 
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
    for (uint32_t i = 1; i <= getNumberOfSections(); i++){
        rawSections[i]->buildLineInfoFinder();
    }
*/
}

void ElfFile::findLoops(){
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        textSections[i]->buildLoops();
    }
}


