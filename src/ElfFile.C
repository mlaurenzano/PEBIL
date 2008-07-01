#include <FileHeader.h>
#include <ProgramHeader.h>
#include <SectionHeader.h>
#include <ElfFile.h>
#include <BinaryFile.h>
#include <RawSection.h>
#include <TextSection.h>
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

uint32_t ElfFileInst::fillPLTBytes(char* bytes, uint64_t gotAddress, uint64_t pltBase){
    uint32_t size;
    uint32_t plt[16];
    if (elfFile->is64Bit()){
        PRINT_ERROR("Must implement plt entry for 64 bit");
        size = 16;
    } else {
        size = 16;
    }

    /*
    0x8048590:     jmp    *0x8049eb4       (bytes -- ff 25 b4 9e 04 08 
    0x8048596:     push   $0x28    (bytes -- 68 28 00 00 00 
    0x804859b:     jmp    0xffffff95       (bytes -- e9 90 ff ff ff 
    */

    return size;
}

void ElfFileInst::addInstrumentationFunction(const char* funcname){
    PRINT_INFOR("Adding instrumentation function -- %s", funcname);
    uint32_t strOffset = addStringToDynamicStringTable(funcname);
    addSymbolToDynamicSymbolTable(strOffset, 0, 0, STB_WEAK, STT_FUNC, 0, 0);

    ASSERT(pltAddress && "You must reserve a PLT section before calling this function");

    // find our plt section
    uint16_t pltSectionIndex = elfFile->getNumberOfSections();
    for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_addr) == pltAddress){
            pltSectionIndex = i;
        }
    }
    ASSERT(pltSectionIndex != elfFile->getNumberOfSections() && "Could not find our reserved PLT section, are you sure it is initialized");

    SectionHeader* pltHeader = elfFile->getSectionHeader(pltSectionIndex);
    TextSection* pltTextSection = NULL;
    
    for (uint32_t i = 0; i < elfFile->getNumberOfTextSections(); i++){
        if (pltSectionIndex == elfFile->getTextSection(i)->getSectionIndex()){
            pltTextSection = elfFile->getTextSection(i);
        }
    }
    ASSERT(pltTextSection && "Could not find the text section for our PLT table");

    char* pltBytes;
    uint32_t pltEntrySize = fillPLTBytes(pltBytes, 0, 0);
    //    pltTextSection->addInstruction(


    /*
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        if (dynamicTable->getDynamic(i)->GET(d_tag) == DT_FINI){
            dynamicTable->getDynamic(i)->setPointer(0x8048418);
        }
    }
    */
}


// this function does not maintain section order or offsets -- these should be
// done outside of this function since these requirements are only known by
// the caller
uint64_t ElfFile::addSection(uint16_t idx, ElfClassTypes classtype, char* bytes, uint32_t name, uint32_t type, uint64_t flags, uint64_t addr, uint64_t offset, 
                             uint64_t size, uint32_t link, uint32_t info, uint64_t addralign, uint64_t entsize){

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
        }
    }

    if (is64Bit()){
        newSectionHeaders[idx] = new SectionHeader64(idx);
        Elf64_Shdr entry;
        entry.sh_name = name;
        entry.sh_type = type;
        entry.sh_flags = flags;
        entry.sh_addr = addr;
        entry.sh_offset = offset; 
        entry.sh_size = size;
        entry.sh_link = link;
        entry.sh_info = info;
        entry.sh_addralign = addralign;
        entry.sh_entsize = entsize;
        memcpy(newSectionHeaders[idx]->charStream(),&entry,sizeof(entry));
    } else {
        newSectionHeaders[idx] = new SectionHeader32(idx);
        Elf32_Shdr entry;
        entry.sh_name = name;
        entry.sh_type = type;
        entry.sh_flags = flags;
        entry.sh_addr = addr;
        entry.sh_offset = offset; 
        entry.sh_size = size;
        entry.sh_link = link;
        entry.sh_info = info;
        entry.sh_addralign = addralign;
        entry.sh_entsize = entsize;
        memcpy(newSectionHeaders[idx]->charStream(),&entry,sizeof(entry));        
    }

    newRawSections[idx] = new RawSection(classtype,bytes,size,idx,this);
    delete[] sectionHeaders;
    sectionHeaders = newSectionHeaders;

    delete[] rawSections;
    rawSections = newRawSections;

    numberOfSections++;


    // increment the number of sections in the file header
    getFileHeader()->setSectionCount(getFileHeader()->GET(e_shnum)+1);

    // if the section header string table moved, increment the pointer to it
    if (idx < getFileHeader()->GET(e_shstrndx)){
        getFileHeader()->setStringTableIndex(getFileHeader()->GET(e_shstrndx)+1);
    }

    // update the section header links to section indices
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->GET(sh_link) >= idx && sectionHeaders[i]->GET(sh_link) < numberOfSections){
            sectionHeaders[i]->setLink(sectionHeaders[i]->GET(sh_link)+1);
        }
    }

    // update the symbol table links to section indices
    for (uint32_t i = 0; i < numberOfSymbolTables; i++){
        SymbolTable* symtab = getSymbolTable(i);
        for (uint32_t j = 0; j < symtab->getNumberOfSymbols(); j++){
            Symbol* sym = symtab->getSymbol(j);
            if (sym->GET(st_shndx) >= idx && sym->GET(st_shndx) < numberOfSections){
                sym->setSectionLink(sym->GET(st_shndx)+1);
            }
        }
    }


    return sectionHeaders[idx]->GET(sh_addr);
}

void ElfFileInst::print(){
    elfFile->print();
    PRINT_INFOR("");
    PRINT_INFOR("Instrumentation Reservations:");
    if (extraTextSize){
        PRINT_INFOR("\tTEXT offsets reserved --\t[0x%016llx,0x%016llx] (%d bytes)", extraTextOffset, extraTextOffset + extraTextSize, extraTextSize);
    }
    if (pltSize){
        PRINT_INFOR("\tPLT  offsets reserved --\t[0x%016llx,0x%016llx] (%d bytes)", pltAddress, pltAddress + pltSize, pltSize);
    }
    if (extraDataSize){
        PRINT_INFOR("\tDATA offsets reserved --\t[0x%016llx,0x%016llx] (%d bytes)", extraDataOffset, extraDataOffset + extraDataSize, extraDataSize); 
        PRINT_INFOR("\tDATA memory  reserved --\t[0x%016llx,0x%016llx] (%d bytes)", extraDataAddress, extraDataAddress + extraDataSize, extraDataSize);
    }
    if (gotSize){
        PRINT_INFOR("\tGOT  memory  reserved --\t[0x%016llx,0x%016llx] (%d bytes)", gotAddress, gotAddress + gotSize, gotSize);
    }
}


// reserve space just after the bss section for our GOT
uint64_t ElfFileInst::reserveGlobalOffsetTable(uint32_t size){

    uint16_t lowestDataIdx = elfFile->getNumberOfSections();
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_NOBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasWriteBit()){
            if (lowestDataIdx == elfFile->getNumberOfSections()){
                lowestDataIdx = i;
            }
        }
    }
    ASSERT(lowestDataIdx != elfFile->getNumberOfSections() && "Could not find the BSS section in the file");

    SectionHeader* dataHdr = elfFile->getSectionHeader(lowestDataIdx);
    gotAddress = elfFile->addSection(lowestDataIdx+1, ElfClassTypes_global_offset_table, elfFile->getElfFileName(), dataHdr->GET(sh_name), SHT_PROGBITS,
                                     dataHdr->GET(sh_flags), extraDataAddress, extraDataOffset, size, dataHdr->GET(sh_link), 
                                     dataHdr->GET(sh_info), 0, dataHdr->GET(sh_entsize));


    // move all later sections' offsets so that they don't conflict with this one
    for (uint32_t i = lowestDataIdx+2; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* scn = elfFile->getSectionHeader(i);
        scn->setOffset(scn->GET(sh_offset) + size);        
    }

    gotSize = size;
    gotAddress = extraDataAddress;
    gotOffset = extraDataOffset;
    extraDataSize -= size;
    extraDataAddress += size;
    extraDataOffset += size;

    elfFile->verify();

    return gotAddress;
}


// reserve space immediately preceeding the rest of the code for our PLT
uint64_t ElfFileInst::reserveProcedureLinkageTable(uint32_t size){

    uint16_t lowestTextIdx = elfFile->getNumberOfSections();
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextIdx == elfFile->getNumberOfSections()){
                lowestTextIdx = i;
            }
        }
    }
    ASSERT(lowestTextIdx != elfFile->getNumberOfSections() && "Could not find any text sections in the file");

    SectionHeader* textHdr = elfFile->getSectionHeader(lowestTextIdx);
    pltAddress = elfFile->addSection(lowestTextIdx, ElfClassTypes_text_section, elfFile->getElfFileName(), textHdr->GET(sh_name), textHdr->GET(sh_type),
                                     textHdr->GET(sh_flags), textHdr->GET(sh_addr)-size, textHdr->GET(sh_offset)-size, size, textHdr->GET(sh_link), 
                                     textHdr->GET(sh_info), textHdr->GET(sh_addralign), textHdr->GET(sh_entsize));
    pltSize = size;
    extraTextSize -= size;

    SectionHeader* pltSection = elfFile->getSectionHeader(lowestTextIdx);

    // this section offset shouldn't conflict with any other sections
    for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
        if (i != lowestTextIdx){
            SectionHeader* scn = elfFile->getSectionHeader(i);
            ASSERT(scn && "Section Header should exist");
            if (scn->GET(sh_offset) >= pltSection->GET(sh_offset) && scn->GET(sh_offset) < pltSection->GET(sh_offset) + pltSection->GET(sh_size)){
                pltSection->print();
                scn->print();
                PRINT_ERROR("Section %d should not begin in the middle of the new PLT section", i);
            } else if (scn->GET(sh_offset) + scn->GET(sh_size) > pltSection->GET(sh_offset) && scn->GET(sh_offset) + scn->GET(sh_size) <= pltSection->GET(sh_offset) + pltSection->GET(sh_size)){
                pltSection->print();
                scn->print();
                PRINT_ERROR("Section %d should not end in the middle of the new PLT sections", i);
            } else if (pltSection->GET(sh_offset) >= scn->GET(sh_offset) && pltSection->GET(sh_offset) < scn->GET(sh_offset) + scn->GET(sh_size)){
                pltSection->print();
                scn->print();
                PRINT_ERROR("The new PLT section should not be contained by section %d", i);
            }
        }
    }

    elfFile->verify();

    return pltAddress;
}


ElfFileInst::ElfFileInst(ElfFile* elf){
    elfFile = elf;
    extraTextOffset = 0;
    extraTextSize = 0;
    extraDataOffset = 0;
    extraDataAddress = 0;
    extraDataSize = 0;

    pltAddress = 0;
    pltSize = 0;
    gotOffset = 0;
    gotAddress = 0;
    gotSize = 0;
}

uint32_t ElfFileInst::addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx){
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t entrySize;
    if (elfFile->is64Bit()){
        entrySize = Size__64_bit_Symbol;
    } else {
        entrySize = Size__32_bit_Symbol;
    }
    ASSERT(entrySize < extraTextSize && "There is not enough extra space in the text section to extend the symbol table");

    uint64_t symtabAddr = dynamicTable->getSymbolTableAddress();
    uint16_t symtabIdx = elfFile->getNumberOfSymbolTables();
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symTab = elfFile->getSymbolTable(i);
        SectionHeader* sHdr = elfFile->getSectionHeader(symTab->getSectionIndex());
        if (sHdr->GET(sh_addr) == symtabAddr){
            ASSERT(symtabIdx == elfFile->getNumberOfSymbolTables() && "Cannot have multiple symbol tables linked to the dynamic table");
            symtabIdx = i;
        }
    }
    ASSERT(symtabIdx != elfFile->getNumberOfSymbolTables() && "There must be a symbol table that is identifiable with the dynamic table");
    SymbolTable* dynamicSymbolTable = elfFile->getSymbolTable(symtabIdx);

    // add the symbol to the symbol table
    uint32_t symbolIndex = dynamicSymbolTable->addSymbol(name, value, size, bind, type, other, scnidx);

    SectionHeader* dynamicSymbolSection = elfFile->getSectionHeader(dynamicSymbolTable->getSectionIndex());
    dynamicSymbolSection->setSize(dynamicSymbolSection->GET(sh_size)+entrySize);

    uint16_t lowestTextIdx = elfFile->getNumberOfSections();
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextIdx == elfFile->getNumberOfSections()){
                lowestTextIdx = i;
            }
        }
    }
    ASSERT(lowestTextIdx != elfFile->getNumberOfSections() && "Could not find any text sections in the file");

    // displace every section in the text segment that comes after the dynamic symbol section and before the code
    for (uint32_t i = dynamicSymbolSection->getIndex()+1; i < lowestTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        sHdr->setOffset(sHdr->GET(sh_offset) + entrySize);
        sHdr->setAddress(sHdr->GET(sh_addr) + entrySize);
    }

    extraTextOffset += entrySize;
    extraTextSize -= entrySize;

    // adjust the dynamic table entries for the section shifts
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED || tag == DT_STRTAB ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            ASSERT(dyn->GET_A(d_ptr,d_un) > dynamicSymbolSection->GET(sh_addr) && "The gnu version tables and relocation tables should be after the dynamic symbol table");
            dyn->setPointer(dyn->GET_A(d_ptr,d_un) + entrySize);
        }

    }

    // add an entry to the hash table
    HashTable* hashTable = elfFile->getHashTable();
    hashTable->addChain();
    if (hashTable->getNumberOfBuckets() < hashTable->getNumberOfChains()/2){
        expandHashTable();
    }

    

    return symbolIndex;
}

uint32_t ElfFileInst::expandHashTable(){

    HashTable* hashTable = elfFile->getHashTable();
    uint32_t extraHashEntries = hashTable->expandSize(hashTable->getNumberOfChains()/2);

    PRINT_INFOR("Expanding the hash table by %d entries", extraHashEntries);

    SectionHeader* hashHeader = elfFile->getSectionHeader(hashTable->getSectionIndex());
    uint32_t extraSize = extraHashEntries * hashTable->getEntrySize();
    hashHeader->setSize(hashHeader->GET(sh_size) + extraSize);

    PRINT_INFOR("Expanding the hash table by %d bytes", extraSize);


    uint16_t lowestTextIdx = elfFile->getNumberOfSections();
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextIdx == elfFile->getNumberOfSections()){
                lowestTextIdx = i;
            }
        }
    }
    ASSERT(lowestTextIdx != elfFile->getNumberOfSections() && "Could not find any text sections in the file");

    for (uint32_t i = hashTable->getSectionIndex() + 1; i < lowestTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        sHdr->setOffset(sHdr->GET(sh_offset) + extraSize);
        sHdr->setAddress(sHdr->GET(sh_addr) + extraSize);
    }


    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->setPointer(dyn->GET_A(d_ptr,d_un) + extraSize);
        }

    }

    extraTextOffset += extraSize;
    extraTextSize -= extraSize;

    return extraSize;
}

uint32_t ElfFileInst::addStringToDynamicStringTable(const char* str){

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strSize = strlen(str) + 1;
    ASSERT(strSize < extraTextSize && "There is not enough extra space in the text section to extend the string table");

    // find the string table we will be adding to
    uint64_t stringTableAddr = dynamicTable->getStringTableAddress();
    uint16_t stringTableIdx = elfFile->getNumberOfStringTables();
    PRINT_INFOR("Looking for string table at %016llx", stringTableAddr);
    for (uint32_t i = 0; i < elfFile->getNumberOfStringTables(); i++){
        StringTable* strTab = elfFile->getStringTable(i);
        SectionHeader* sHdr = elfFile->getSectionHeader(strTab->getSectionIndex());
        PRINT_INFOR("\tString table is at %016llx", sHdr->GET(sh_addr));
        if (sHdr->GET(sh_addr) == stringTableAddr){
            ASSERT(stringTableIdx == elfFile->getNumberOfStringTables() && "Cannot have multiple string tables linked to the dynamic table");
            stringTableIdx = i;
        }
    }
    ASSERT(stringTableIdx != elfFile->getNumberOfStringTables() && "There must be a string table that is identifiable with the dynamic table");

    PRINT_INFOR("Found string table for dynamic table at section %d", stringTableIdx);
    StringTable* dynamicStringTable = elfFile->getStringTable(stringTableIdx);

    // add the string to the string table
    uint32_t origSize = dynamicStringTable->getSizeInBytes(); 
    uint32_t stringOffset = dynamicStringTable->addString(str);
    uint32_t extraSize = nextAlignAddressDouble(strlen(str)+1);

    SectionHeader* dynamicStringSection = elfFile->getSectionHeader(dynamicStringTable->getSectionIndex());
    dynamicStringSection->setSize(dynamicStringSection->GET(sh_size)+extraSize);

    uint16_t lowestTextIdx = elfFile->getNumberOfSections();
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextIdx == elfFile->getNumberOfSections()){
                lowestTextIdx = i;
            }
        }
    }
    ASSERT(lowestTextIdx != elfFile->getNumberOfSections() && "Could not find any text sections in the file");

    // displace every section in the text segment that comes after the dynamic string section and before the code
    for (uint32_t i = dynamicStringSection->getIndex()+1; i < lowestTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        sHdr->setOffset(sHdr->GET(sh_offset) + extraSize);
        sHdr->setAddress(sHdr->GET(sh_addr) + extraSize);
    }

    extraTextOffset += extraSize;
    extraTextSize -= extraSize;


    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            ASSERT(dyn->GET_A(d_ptr,d_un) > dynamicStringSection->GET(sh_addr) && "The gnu version tables and relocation tables should be after the dynamic string table");
            dyn->setPointer(dyn->GET_A(d_ptr,d_un) + extraSize);
        }

    }

    return origSize;

}


void ElfFileInst::addSharedLibrary(const char* libname){

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strOffset = addStringToDynamicStringTable(libname);

    // add a DT_NEEDED entry to the dynamic table
    uint32_t emptyDynamicIdx = dynamicTable->findEmptyDynamic();

    ASSERT(emptyDynamicIdx < dynamicTable->getNumberOfDynamics() && "No free entries found in the dynamic table");

    dynamicTable->getDynamic(emptyDynamicIdx)->setTag(DT_NEEDED);
    dynamicTable->getDynamic(emptyDynamicIdx)->setPointer(strOffset);

}


uint64_t ElfFileInst::extendDataSection(uint32_t size){

    ASSERT(!extraDataOffset && !extraDataSize && !extraDataAddress && "Cannot extend the data segment more than once");

    ProgramHeader* dataHeader = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());

    // we first find the bss section. Since the bss has to be the last section in the data
    // segment, we will extend the size of the bss segment then place any extra data in this
    // extra space in the bss section.
    uint16_t bssSectionIdx = 0;
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_NOBITS){
            // for now we assume that any section of type NOBITS is a bss section
            ASSERT(!bssSectionIdx && "Cannot have more than 1 bss section (defined by having type SHT_NOBITS)");
            bssSectionIdx = i;
        }
    }
    SectionHeader* bssSection = elfFile->getSectionHeader(bssSectionIdx);

    // increase the memory size of the bss section (note: this section has no size in the file)
    extraDataSize = size;
    extraDataOffset = bssSection->GET(sh_offset);
    extraDataAddress = bssSection->GET(sh_addr) + bssSection->GET(sh_size);
    bssSection->setSize(bssSection->GET(sh_size)+size);

    // increase the memory size of the data segment
    dataHeader->setMemorySize(dataHeader->GET(p_memsz)+size);

    PRINT_INFOR("Extra data space available: [0x%016llx,0x%016llx) -- %d bytes", extraDataOffset, (extraDataOffset + extraDataSize), extraDataSize)

    elfFile->verify();

    return extraDataAddress;
}

uint64_t ElfFileInst::extendTextSection(uint32_t size){
    size = nextAlignAddress(size,DEFAULT_PAGE_ALIGNMENT);
    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ASSERT(!extraTextOffset && !extraTextSize && "Cannot extend the text segment more than once");
    extraTextSize = size;

    PRINT_INFOR("Trying to get segments %hd %hd", elfFile->getTextSegmentIdx(), elfFile->getDataSegmentIdx());
    ProgramHeader* textHeader = elfFile->getProgramHeader(elfFile->getTextSegmentIdx());
    ProgramHeader* dataHeader = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());

    // first we will find the address of the first text section. we will be moving all elf
    // control structures that occur prior to this address with the extension of the text
    // segment (the interp and note.ABI-tag sections must be in the first text page and it
    // will make certain things easier for all control sections together)
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_PROGBITS &&
            elfFile->getSectionHeader(i)->hasAllocBit() && elfFile->getSectionHeader(i)->hasExecInstrBit()){
            if (lowestTextAddress > elfFile->getSectionHeader(i)->GET(sh_addr)){
                ASSERT(lowestTextAddress == -1 && "Text section addresses should appear in increasing order");
                lowestTextAddress = elfFile->getSectionHeader(i)->GET(sh_addr);
                lowestTextSectionIdx = i;
            }
        }
    }


    // for each segment that is contained within the loadable text segment,
    // update its address to reflect the new base address of the text segment
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* subHeader = elfFile->getProgramHeader(i);
        if (textHeader->inRange(subHeader->GET(p_vaddr)) && i != elfFile->getTextSegmentIdx()){
            subHeader->setVirtualAddress(subHeader->GET(p_vaddr)-size);
            subHeader->setPhysicalAddress(subHeader->GET(p_paddr)-size);
        } 
    }


    // for each segment that is (or is contained within) the data segment,
    // update its offset to reflect the the base address of the executable
    // (ie the base address of the text segment)
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* subHeader = elfFile->getProgramHeader(i);
        if (dataHeader->inRange(subHeader->GET(p_vaddr))){
            subHeader->setOffset(subHeader->GET(p_offset)+size);
        }
    }


    // update section symbols for the sections that were moved. technically the loader won't use them 
    // but we will try to keep the binary as consistent as possible
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symTab = elfFile->getSymbolTable(i);
        for (uint32_t j = 0; j < symTab->getNumberOfSymbols(); j++){
            Symbol* sym = symTab->getSymbol(j);
            if (sym->getSymbolType() == STT_SECTION && sym->GET(st_value) < lowestTextAddress && sym->GET(st_value)){
                sym->setValue(sym->GET(st_value)-size);
            }
        }
    }
    
    // modify the base address of the text segment and increase its size so it ends at the same address
    textHeader->setVirtualAddress(textHeader->GET(p_vaddr)-size);
    textHeader->setPhysicalAddress(textHeader->GET(p_paddr)-size);
    textHeader->setMemorySize(textHeader->GET(p_memsz)+size);
    textHeader->setFileSize(textHeader->GET(p_filesz)+size);


    // For any section that falls before the program's code, displace its address so that it is in the
    // same location relative to the base address.
    // Likewise, displace the offset of any section that falls during/after the program's code so that
    // the code will be put in the correct location within the text segment.
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        if (i < lowestTextSectionIdx){
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) < lowestTextAddress && "No section that occurs before the first text section should have a larger address");
            // strictly speaking the loader doesn't use these, but for consistency we change these
            sHdr->setAddress(sHdr->GET(sh_addr)-size);
            // the extra text offset will be at the end of the last elf control section
            extraTextOffset = sHdr->GET(sh_offset)+sHdr->GET(sh_size);
        } else {
            sHdr->setOffset(sHdr->GET(sh_offset)+size);            
        }
    }

    // since some sections were displaced in the file, displace the section header table also so
    // that it occurs after all of the sections in the file
    elfFile->getFileHeader()->setSectionHeaderOffset(elfFile->getFileHeader()->GET(e_shoff)+size);


    // update the dynamic table to correctly point to the displaced elf control sections
    for (uint32_t i = 0; i < elfFile->getDynamicTable()->getNumberOfDynamics(); i++){
        Dynamic* dyn = elfFile->getDynamicTable()->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->setPointer(dyn->GET_A(d_ptr,d_un)-size);
        }
    }

    elfFile->verify();

    PRINT_INFOR("Extra text space available: [0x%016llx,0x%016llx) -- %d bytes", extraTextOffset, (extraTextOffset + extraTextSize), extraTextSize);

    return extraTextOffset;
}

uint64_t ElfFileInst::relocateDynamicSection(){
    
    ASSERT(0 && "This function should not be used, relocating the dynamic table is not easy and we won't do unless absolutely necessary");
    
    // move the dynamic section to the end of the elf control sections in the text segment
    ASSERT(elfFile->getDynamicSectionAddress() && "We should have found the dynamic table for this executable prior to this");
    ASSERT(elfFile->getDynamicTableSectionIdx() && "We should have found the dynamic table for this executable prior to this");

    SectionHeader* dynamicSection = elfFile->getSectionHeader(elfFile->getDynamicTableSectionIdx());
    ASSERT(dynamicSection->GET(sh_size) <= extraTextSize && "There is not enough extra space in the data section to move the dynamic table");
    dynamicSection->setOffset(extraTextOffset);
    dynamicSection->setAddress(getProgramBaseAddress() + extraTextOffset);
    extraTextOffset += dynamicSection->GET(sh_size);
    extraTextSize -= dynamicSection->GET(sh_size);

    elfFile->sortSectionHeaders();

    // point the _DYNAMIC symbol(s) to the new location of the dynamic section
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symTab = elfFile->getSymbolTable(i);
        for (uint32_t j = 0; j < symTab->getNumberOfSymbols(); j++){
            Symbol* sym = symTab->getSymbol(j);
            if (!strcmp(symTab->getSymbolName(j),DYN_SYM_NAME)){
                sym->setValue(dynamicSection->GET(sh_addr));
            }
        }
    }
    
    // move the dynamic segment to point to the new dynamic section
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* pHdr = elfFile->getProgramHeader(i);
        if (pHdr->GET(p_type) == PT_DYNAMIC){
            pHdr->setOffset(dynamicSection->GET(sh_offset));
            pHdr->setVirtualAddress(dynamicSection->GET(sh_addr));
            pHdr->setPhysicalAddress(dynamicSection->GET(sh_addr));
            pHdr->setFlags(7);
        }
    }

    elfFile->verify();

    return 0;
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


// get the smallest virtual address of all loadable segments (ie, the base address for the program)
uint64_t ElfFileInst::getProgramBaseAddress(){
    uint64_t segmentBase = -1;

    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        if (elfFile->getProgramHeader(i)->GET(p_type) == PT_LOAD){
            if (elfFile->getProgramHeader(i)->GET(p_vaddr) < segmentBase){
                segmentBase = elfFile->getProgramHeader(i)->GET(p_vaddr);
            }
        }
    }

    ASSERT(segmentBase != -1 && "No loadable segments found (or their v_addr fields are incorrect)");
    return segmentBase;
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
    PRINT_INFOR("Dynamic section found at address 0x%016llx", dynamicSectionAddress);

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

    if (!getFileHeader()){
        PRINT_ERROR("File Header should exist");
    }

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
            }
        }
    }
    if (textSegCount != 1){
        PRINT_ERROR("Exactly 1 loadable text segment must be present, %d found", textSegCount);
    }
    if (dataSegCount != 1){
        PRINT_ERROR("Exactly 1 loadable data segment must be present, %d found", dataSegCount);
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
        if (!sectionHeaders[i]){
            PRINT_ERROR("Section header %d should exist", i);
        }
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


