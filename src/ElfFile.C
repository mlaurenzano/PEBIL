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
#include <X86Instruction.h>
#include <Instrumentation.h>
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

void ElfFile::swapSections(uint32_t idx1, uint32_t idx2){
    ASSERT(idx1 < sectionHeaders.size() && idx2 < sectionHeaders.size());
    SectionHeader* tmpSectionHeader = sectionHeaders[idx1];
    sectionHeaders[idx1] = sectionHeaders[idx2];
    sectionHeaders[idx2] = tmpSectionHeader;

    RawSection* tmpRawSection = rawSections[idx1];
    rawSections[idx1] = rawSections[idx2];
    rawSections[idx2] = tmpRawSection;
}

TextSection* ElfFile::getDotTextSection(){
    uint16_t textIdx = findSectionIdx(".text");
    TextSection* textSection = (TextSection*)getRawSection(textIdx);
    return textSection;
}
TextSection* ElfFile::getDotFiniSection(){
    uint16_t textIdx = findSectionIdx(".fini");
    TextSection* textSection = (TextSection*)getRawSection(textIdx);
    return textSection;
}
TextSection* ElfFile::getDotInitSection(){
    uint16_t textIdx = findSectionIdx(".init");
    TextSection* textSection = (TextSection*)getRawSection(textIdx);
    return textSection;
}
TextSection* ElfFile::getDotPltSection(){
    uint16_t textIdx = findSectionIdx(".plt");
    TextSection* textSection = (TextSection*)getRawSection(textIdx);
    return textSection;
}


ElfFile::ElfFile(char* f) :
is64BitFlag(false), staticLinked(false), elfFileName(f), fileHeader(NULL), globalOffsetTable(NULL), dynamicTable(NULL),
gnuVerneedTable(NULL), gnuVersymTable(NULL), dynamicStringTable(NULL), dynamicSymbolTable(NULL),
pltRelocationTable(NULL), dynamicRelocationTable(NULL), lineInfoSection(NULL), dynamicSymtabIdx(0),
dynamicSectionAddress(0), dynamicTableSectionIdx(0), textSegmentIdx(0), dataSegmentIdx(0), numberOfFunctions(0),
numberOfBlocks(0), numberOfMemoryOps(0), numberOfFloatPOps(0)
{
    uint32_t addrAlign;
    if (is64Bit()){
        addrAlign = sizeof(uint64_t);
    } else {
        addrAlign = sizeof(uint32_t);
    }

    DataReference* zeroAddrRef = new DataReference(0, NULL, addrAlign, 0);
    specialDataRefs.append(zeroAddrRef);

    addressAnchors = new Vector<AddressAnchor*>();
    anchorsAreSorted = false;
}


Symbol* ElfFile::lookupFunctionSymbol(uint64_t addr){
    GlobalOffsetTable* gotTable = getGlobalOffsetTable();
    RelocationTable* pltRelocTable = getPLTRelocationTable();
    SymbolTable* dynsymTable = getDynamicSymbolTable();
    if (isStaticLinked()){
        ASSERT(!gotTable && !pltRelocTable && !dynsymTable);
    } else {
        ASSERT(gotTable && pltRelocTable && dynsymTable);
    }

    char* namestr = NULL;
    uint32_t numSyms = 8;
    Symbol* foundSymbols[numSyms];
    bzero(&foundSymbols, sizeof(Symbol*) * numSyms);

    // first search directly for a symbol (non-PLT functions)
    findSymbol4Addr(addr, foundSymbols, numSyms, &namestr);
    if (namestr){
        delete[] namestr;
    }
    for (uint32_t i = 0; i < numSyms; i++){
        if (foundSymbols[i]){
            if (is64Bit()){
                if (ELF64_ST_TYPE(foundSymbols[i]->GET(st_info)) == STT_FUNC){
                    return foundSymbols[i];
                }
            } else {
                if (ELF32_ST_TYPE(foundSymbols[i]->GET(st_info)) == STT_FUNC){
                    return foundSymbols[i];
                }
            } 
        }
    }

    uint64_t val = addr + Size__ProcedureLink_Intermediate;
    uint64_t gotAddr = 0;
    int64_t symIndex = -1;

    if (isStaticLinked()){
        return NULL;
    }

    // search GOT to get relocation entry to get symbol index (PLT function)
    for (uint32_t i = 0; i < gotTable->getNumberOfEntries(); i++){
        if (gotTable->getEntry(gotTable->minIndex()+i) == val){
            gotAddr = gotTable->getEntryAddress(i);
            break;
        }
    }

    if (gotAddr){
        for (uint32_t i = 0; i < pltRelocTable->getNumberOfRelocations(); i++){
            if (pltRelocTable->getRelocation(i)->GET(r_offset) == gotAddr){
                symIndex = pltRelocTable->getRelocation(i)->getSymbol();
                break;
            }
        }
    }

    if (symIndex > 0){
        return dynamicSymbolTable->getSymbol(symIndex);
    }

    return NULL;
}

ProgramHeader* ElfFile::getProgramHeaderPHDR(){
    if (getProgramHeader(0)->GET(p_type) == PT_PHDR){
        return getProgramHeader(0);
    }
    return NULL;
}

DataSection* ElfFile::getDotDataSection(){
    uint16_t dataSectionIndex = 0;

    // pick the section prior to the .bss section
    dataSectionIndex = findSectionIdx(".data");
    if (dataSectionIndex){
        return (DataSection*)getRawSection(dataSectionIndex);
    }

    dataSectionIndex = findSectionIdx(".bss") - 1;
    ASSERT(dataSectionIndex && dataSectionIndex < getNumberOfSections());
    if (strstr(getSectionHeader(dataSectionIndex)->getSectionNamePtr(), ".data") != 
        getSectionHeader(dataSectionIndex)->getSectionNamePtr()){
        PRINT_ERROR("section prior to .bss should conform to name `.data*' -- actual name is %s", getSectionHeader(dataSectionIndex)->getSectionNamePtr());
        __SHOULD_NOT_ARRIVE;
    }

    return (DataSection*)getRawSection(dataSectionIndex);
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

    if (ptInterpIdx < getNumberOfPrograms()){
        for (uint32_t i = 0; i < getNumberOfPrograms(); i++){    
            if (programHeaders[i]->GET(p_type) == PT_LOAD){
                if (i < ptInterpIdx){
                    PRINT_ERROR("PT_INTERP segment must preceed any loadable segment");
                    return false;
                }
            }
        }
        if (isStaticLinked()){
            PRINT_ERROR("Static linked executables can't have a PT_INTERP segment");
            return false;
        }
    }

    PriorityQueue<uint64_t,uint64_t> addrs = PriorityQueue<uint64_t,uint64_t>(getNumberOfSections()+3);
    addrs.insert(fileHeader->GET(e_ehsize),0); 
    addrs.insert(fileHeader->GET(e_phentsize)*fileHeader->GET(e_phnum),fileHeader->GET(e_phoff));
    addrs.insert(fileHeader->GET(e_shentsize)*fileHeader->GET(e_shnum),fileHeader->GET(e_shoff));
    PRINT_DEBUG_ADDR_ALIGN("file header range (%#llx,%#llx)", fileHeader->GET(e_ehsize),0);
    PRINT_DEBUG_ADDR_ALIGN("pheader table range (%#llx,%#llx)", fileHeader->GET(e_phentsize)*fileHeader->GET(e_phnum),fileHeader->GET(e_phoff));
    PRINT_DEBUG_ADDR_ALIGN("sheader range (%#llx,%#llx)", fileHeader->GET(e_shentsize)*fileHeader->GET(e_shnum),fileHeader->GET(e_shoff));
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]->GET(sh_type) != SHT_NOBITS){
            addrs.insert(sectionHeaders[i]->GET(sh_size),sectionHeaders[i]->GET(sh_offset));
            PRINT_DEBUG_ADDR_ALIGN("section header %d range (%#llx,%#llx)", i, sectionHeaders[i]->GET(sh_size), sectionHeaders[i]->GET(sh_offset));
        }
    }
    ASSERT(addrs.size() && "This queue should not be empty");

    uint64_t prevBegin, prevSize;
    uint64_t currBegin, currSize;
    prevSize = addrs.deleteMin(&prevBegin);
    while (addrs.size()){
        currSize = addrs.deleteMin(&currBegin);
        PRINT_DEBUG_ADDR_ALIGN("Verifying address ranges [%llx,%llx],[%llx,%llx]", prevBegin, prevBegin+prevSize, currBegin, currBegin+currSize);
        if (prevBegin + prevSize > currBegin && currSize != 0){
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
    
    uint64_t gnuHashSectionAddress_DT = 0;
    uint64_t sysvHashSectionAddress_DT = 0;
    for (uint32_t i = 0; i < getNumberOfHashTables(); i++){
        if (hashTables[i]->isGnuStyleHash()){
            if (gnuHashSectionAddress_DT){
                PRINT_ERROR("Cannot have more than one (gnu) hash table");
                return false;
            }
            gnuHashSectionAddress_DT = dynamicTable->getDynamicByType(DT_GNU_HASH,0)->GET_A(d_val,d_un);        
        } else {
            if (sysvHashSectionAddress_DT){
                PRINT_ERROR("Cannot have more than one (sysv) hash table");
                return false;
            }
            sysvHashSectionAddress_DT = dynamicTable->getDynamicByType(DT_HASH,0)->GET_A(d_val,d_un);
        }
    }
    // if both types are present, we assume the sysv style version comes first
    if (getNumberOfHashTables() == 2){
        if (gnuHashSectionAddress_DT <= sysvHashSectionAddress_DT){
            PRINT_ERROR("Sysv hash table should come before gnu hash table");
            return false;
        }
    }

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
    uint64_t gnuHashSectionAddress = 0;
    uint64_t sysvHashSectionAddress = 0;
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

        if (sectionHeaders[i]->getSectionType() == PebilClassType_SysvHashTable){
            if (sysvHashSectionAddress){
                PRINT_ERROR("Cannot have more than one (sysv) hash section");
                return false;
            }
            if (dynstrSectionAddress){
                PRINT_ERROR("(Sysv) Hash table should come before dynamic string table");
                return false;
            }
            sysvHashSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->getSectionType() == PebilClassType_GnuHashTable){
            if (gnuHashSectionAddress){
                PRINT_ERROR("Cannot have more than one (gnu) hash section");
                return false;
            }
            if (dynstrSectionAddress){
                PRINT_ERROR("(Gnu) Hash table should come before dynamic string table");
                return false;
            }
            gnuHashSectionAddress = sectionHeaders[i]->GET(sh_addr);
        } else if (sectionHeaders[i]->getSectionType() == PebilClassType_SymbolTable &&
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
        } else if (sectionHeaders[i]->getSectionType() == PebilClassType_StringTable &&
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
        } else if (sectionHeaders[i]->getSectionType() == PebilClassType_RelocationTable &&
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

ProgramHeader* ElfFile::addSegment(uint16_t idx, uint32_t type, uint64_t offset, uint64_t vaddr, uint64_t paddr,
                             uint32_t memsz, uint32_t filesz, uint32_t flags, uint32_t align){
    if (is64Bit()){
        programHeaders.insert(new ProgramHeader64(idx), idx);
    } else {
        programHeaders.insert(new ProgramHeader32(idx), idx);
    }

    programHeaders[idx]->SET(p_type, type);
    programHeaders[idx]->SET(p_offset, offset);
    programHeaders[idx]->SET(p_vaddr, vaddr);
    programHeaders[idx]->SET(p_paddr, paddr);
    programHeaders[idx]->SET(p_memsz, memsz);
    programHeaders[idx]->SET(p_filesz, filesz);
    programHeaders[idx]->SET(p_flags, flags);
    programHeaders[idx]->SET(p_align, align);

    for (uint32_t i = 0; i < getNumberOfPrograms(); i++){
        programHeaders[i]->setIndex(i);
    }

    // increment the number of sections in the file header
    getFileHeader()->INCREMENT(e_phnum, 1);

    if (getProgramHeaderPHDR()){
        getProgramHeaderPHDR()->INCREMENT(p_memsz, fileHeader->GET(e_phentsize));
        getProgramHeaderPHDR()->INCREMENT(p_filesz, fileHeader->GET(e_phentsize));
    }

    return programHeaders[idx];
}

uint64_t ElfFile::addSection(uint16_t idx, PebilClassTypes classtype, char* bytes, uint32_t name, uint32_t type, 
                             uint64_t flags, uint64_t addr, uint64_t offset, uint64_t size, uint32_t link, 
                             uint32_t info, uint64_t addralign, uint64_t entsize){

    if (is64Bit()){
        sectionHeaders.insert(new SectionHeader64(idx), idx);
    } else {
        sectionHeaders.insert(new SectionHeader32(idx), idx);
    }

    sectionHeaders[idx]->SET(sh_name, name);
    sectionHeaders[idx]->SET(sh_type, type);
    sectionHeaders[idx]->SET(sh_flags, flags);
    sectionHeaders[idx]->SET(sh_addr, addr);
    sectionHeaders[idx]->SET(sh_offset, offset);
    sectionHeaders[idx]->SET(sh_size, size);
    sectionHeaders[idx]->SET(sh_link, link);
    sectionHeaders[idx]->SET(sh_info, info);
    sectionHeaders[idx]->SET(sh_addralign, addralign);
    sectionHeaders[idx]->SET(sh_entsize, entsize);
    sectionHeaders[idx]->setSectionType();

    if (classtype == PebilClassType_TextSection){
        textSections.append(new TextSection(bytes, size, idx, getNumberOfTextSections(), this, ByteSource_Instrumentation));
        rawSections.insert((RawSection*)textSections.back(), idx);
    } else if (classtype == PebilClassType_DataSection){
        dataSections.append(new DataSection(bytes, size, idx, this));
        rawSections.insert((RawSection*)dataSections.back(), idx);
    }
    else {
        __SHOULD_NOT_ARRIVE;
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
                sym->INCREMENT(st_shndx, 1);
            }
        }
    }

    return sectionHeaders[idx]->GET(sh_offset);
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

        ASSERT(sectionHeaders[lineInfoIdx]->getSectionType() == PebilClassType_DwarfSection);
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
    }

    /*
    uint32_t* instBins;
    instBins = new uint32_t[MAX_X86_INSTRUCTION_LENGTH+1];
    bzero(instBins, sizeof(uint32_t) * (MAX_X86_INSTRUCTION_LENGTH+1));
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        for (uint32_t j = 0; j < textSections[i]->getNumberOfTextObjects(); j++){
            if (textSections[i]->getTextObject(j)->isFunction()){
                Function* f = (Function*)textSections[i]->getTextObject(j) ;
                for (uint32_t k = 0; k < f->getNumberOfBasicBlocks(); k++){
                    for (uint32_t l = 0; l < f->getBasicBlock(k)->getNumberOfInstructions(); l++){
                        instBins[f->getBasicBlock(k)->getInstruction(l)->getSizeInBytes()]++;
                    }
                }
            }
        }
    }
    for (uint32_t i = 0; i < MAX_X86_INSTRUCTION_LENGTH+1; i++){

        PRINT_INFOR("Instruction bins %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                    instBins[0],
                    instBins[1],
                    instBins[2],
                    instBins[3],
                    instBins[4],
                    instBins[5],
                    instBins[6],
                    instBins[7],
                    instBins[8],
                    instBins[9],
                    instBins[10],
                    instBins[11],
                    instBins[12],
                    instBins[13],
                    instBins[14],
                    instBins[15],
                    instBins[16],
                    instBins[17],
                    instBins[18],
                    instBins[19],
                    instBins[20]);
    }
    */

}


void ElfFile::initDynamicFilePointers(){

    for (uint32_t i = 0; i < hashTables.size(); i++){
        ASSERT(hashTables[i] && "Hash Table should exist");
        hashTables[i]->initFilePointers();
    }

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
    uint16_t gotSectionIdx = findSectionIdx(gotBaseAddress);
    /*
    for (uint32_t i = 0; i < getNumberOfSections(); i++){
        if (sectionHeaders[i]->inRange(gotBaseAddress)){
            ASSERT(!gotSectionIdx && "Cannot have multiple global offset tables");
            gotSectionIdx = i;
        }
    }
    */
    ASSERT(gotSectionIdx && "Cannot find a section for the global offset table");
    ASSERT(getSectionHeader(gotSectionIdx)->GET(sh_type) == SHT_PROGBITS && "Global Offset Table section header is wrong type");


    // The raw section for the global offset table should already have been initialized as a generic DataSection
    // we will destroy it and create it as a GlobalOffsetTable
    ASSERT(rawSections[gotSectionIdx] && "Global Offset Table not yet created");
    ASSERT(sectionHeaders[gotSectionIdx]->getSectionType() == PebilClassType_DataSection);
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
    ASSERT(sectionHeaders[dynamicTableSectionIdx]->getSectionType() == PebilClassType_DynamicTable);
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
        if (rawSections[i]->getType() == PebilClassType_StringTable &&
            sectionHeaders[i]->GET(sh_addr) == strtabAddr){
            dynamicStringTable = (StringTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == PebilClassType_SymbolTable &&
            sectionHeaders[i]->GET(sh_addr) == symtabAddr){
            dynamicSymbolTable = (SymbolTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == PebilClassType_RelocationTable &&
            sectionHeaders[i]->GET(sh_addr) == pltreltabAddr){
            pltRelocationTable = (RelocationTable*)rawSections[i];
        }
        else if (rawSections[i]->getType() == PebilClassType_RelocationTable &&
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
    if (!addr){
        return retValue;
    }

    if(namestr){
        *namestr = new char[__MAX_STRING_SIZE];
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
        for (uint32_t i = 0; i < hashTables.size(); i++){
            hashTables[i]->print();
        }

        if (!hashTables.size()){
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

void ElfFile::dump(char* extension){
    char fileName[__MAX_STRING_SIZE] = "";
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
            if (i == 21){
                rawSections[37]->dump(binaryOutputFile,currentOffset);
            } else {
                rawSections[i]->dump(binaryOutputFile,currentOffset);
            }
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
        PRINT_ERROR("The file magic number [%02hhx%02hhx%02hhx%02hhx] is not a valid one",e_ident[EI_MAG0],e_ident[EI_MAG1],e_ident[EI_MAG2],e_ident[EI_MAG3]);
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

    if (hashTables.size()){
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

        switch(sectionHeaders[i]->getSectionType()){
        case PebilClassType_StringTable:
            rawSections.append(new StringTable(sectionFilePtr, sectionSize, i, getNumberOfStringTables(), this));
            stringTables.append((StringTable*)rawSections.back());
            break;
        case PebilClassType_SymbolTable:
            rawSections.append(new SymbolTable(sectionFilePtr, sectionSize, i, getNumberOfSymbolTables(), this));
            symbolTables.append((SymbolTable*)rawSections.back());
            break;
        case PebilClassType_RelocationTable:
            rawSections.append(new RelocationTable(sectionFilePtr, sectionSize, i, getNumberOfRelocationTables(), this));
            relocationTables.append((RelocationTable*)rawSections.back());
            break;
        case PebilClassType_DwarfSection:
            rawSections.append(new DwarfSection(sectionFilePtr, sectionSize, i, getNumberOfDwarfSections(), this));
            dwarfSections.append((DwarfSection*)rawSections.back());
            break;
        case PebilClassType_TextSection:
            rawSections.append(new TextSection(sectionFilePtr, sectionSize, i, getNumberOfTextSections(), this, ByteSource_Application));
            textSections.append((TextSection*)rawSections.back());
            break;
        case PebilClassType_SysvHashTable:
            rawSections.append(new SysvHashTable(sectionFilePtr, sectionSize, i, this));
            hashTables.append((HashTable*)rawSections.back());
            break;
        case PebilClassType_GnuHashTable:
            rawSections.append(new GnuHashTable(sectionFilePtr, sectionSize, i, this));
            hashTables.append((HashTable*)rawSections.back());
            break;
        case PebilClassType_NoteSection:
            rawSections.append(new NoteSection(sectionFilePtr, sectionSize, i, getNumberOfNoteSections(), this));
            noteSections.append((NoteSection*)rawSections.back());
            break;
        case PebilClassType_GnuVerneedTable:
            ASSERT(!gnuVerneedTable && "Cannot have more than one GNU_verneed section");
            rawSections.append(new GnuVerneedTable(sectionFilePtr, sectionSize, i, this));
            gnuVerneedTable = (GnuVerneedTable*)rawSections.back();
            break;
        case PebilClassType_GnuVersymTable:
            ASSERT(!gnuVersymTable && "Cannot have more than one GNU_versym section");
            rawSections.append(new GnuVersymTable(sectionFilePtr, sectionSize, i, this));
            gnuVersymTable = (GnuVersymTable*)rawSections.back();
            break;
        case PebilClassType_DataSection:
            rawSections.append(new DataSection(sectionFilePtr, sectionSize, i, this));
            dataSections.append((DataSection*)rawSections.back());
            break;
        default:
            rawSections.append(new RawSection(PebilClassType_RawSection, sectionFilePtr, sectionSize, i, this));
            break;
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
    for (uint32_t i = 0; i < specialDataRefs.size(); i++){
        delete specialDataRefs[i];
    }
    if (addressAnchors){
        delete addressAnchors;
    }
}

void ElfFile::briefPrint(){
}

void ElfFile::displaySymbols(){
}


void ElfFile::generateCFGs(){
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        textSections[i]->generateCFGs(addressAnchors);
    }
}

void ElfFile::findMemoryFloatOps(){
}

uint32_t ElfFile::getFileSize() { 
    return binaryInputFile.getSize(); 
}

void ElfFile::setLineInfoFinder(){
}

void ElfFile::findLoops(){
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        textSections[i]->buildLoops();
    }
}


Vector<AddressAnchor*>* ElfFile::searchAddressAnchors(uint64_t addr){
    Vector<AddressAnchor*>* linearUpdate = new Vector<AddressAnchor*>();
    Vector<AddressAnchor*>* binaryUpdate = new Vector<AddressAnchor*>();
    uint64_t binIdx = 0, linIdx = 0;

#if defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)
    if (!anchorsAreSorted){
        (*addressAnchors).sort(compareLinkBaseAddress);
        anchorsAreSorted = true;
    }
    AddressAnchor** allAnchors = &(*addressAnchors);

    PRINT_DEBUG_ANCHOR("Array is:");
    for (uint32_t i = 0; i < (*addressAnchors).size(); i++){
        PRINT_DEBUG_ANCHOR("%#llx", allAnchors[i]->linkBaseAddress);
    }

    void* anchor = bsearch(&addr, allAnchors, (*addressAnchors).size(), sizeof(AddressAnchor*), searchLinkBaseAddressExact);
    if (anchor){
        // get the FIRST occurrence of addr in the anchor array
        uint64_t idx = ((uint64_t)anchor-(uint64_t)allAnchors)/sizeof(AddressAnchor*);
        while (idx > 0 && (*addressAnchors)[idx-1]->linkBaseAddress == (*addressAnchors)[idx]->linkBaseAddress){
            idx--;
        }
        binIdx = idx;
        while (idx < (*addressAnchors).size() &&
                (*addressAnchors)[idx]->linkBaseAddress <= addr &&
               addr < (*addressAnchors)[idx]->linkBaseAddress + (*addressAnchors)[idx]->getLink()->getSizeInBytes()){
            PRINT_DEBUG_ANCHOR("found %#llx <= %#llx < %#llx + %d", (*addressAnchors)[idx]->linkBaseAddress, addr, (*addressAnchors)[idx]->linkBaseAddress, (*addressAnchors)[idx]->getLink()->getSizeInBytes());
            (*binaryUpdate).append(allAnchors[idx++]);
        }
    }
#endif //defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)

#if !defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)
    for (uint32_t i = 0; i < (*addressAnchors).size(); i++){
        if (addr == (*addressAnchors)[i]->linkBaseAddress){
            PRINT_DEBUG_ANCHOR("%#llx <= %#llx < %#llx", (*addressAnchors)[i]->linkBaseAddress, addr, (*addressAnchors)[i]->linkBaseAddress + (*addressAnchors)[i]->getLink()->getSizeInBytes());
            PRINT_DEBUG_ANCHOR("%#llx <= %#llx < %#llx", (*addressAnchors)[i]->linkBaseAddress, addr, (*addressAnchors)[i]->linkBaseAddress + (*addressAnchors)[i]->getLink()->getSizeInBytes());
            if (!(*linearUpdate).size()){
                linIdx = i;
            }
            linearUpdate->append((*addressAnchors)[i]);
        }
    }
#endif //!defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)

#ifdef VALIDATE_ANCHOR_SEARCH
    if ((*binaryUpdate).size() != (*linearUpdate).size()){
        PRINT_DEBUG_ANCHOR("Mismatch in binary/linear anchor search results for %#llx...", addr);
        PRINT_DEBUG_ANCHOR("Binary search yields %d hits -- see entry %d", (*binaryUpdate).size(), binIdx);
        for (uint32_t i = 0; i < (*binaryUpdate).size(); i++){
            PRINT_DEBUG_ANCHOR("\tbinary[%d] = %#llx", i, (*binaryUpdate)[i]->linkBaseAddress);
        }
        PRINT_DEBUG_ANCHOR("Linear search yields %d hits -- see entry %d", (*linearUpdate).size(), linIdx);
        for (uint32_t i = 0; i < (*linearUpdate).size(); i++){
            PRINT_DEBUG_ANCHOR("\tlinear[%d] = %#llx", i, (*linearUpdate)[i]->linkBaseAddress);
        }
        PRINT_DEBUG_ANCHOR("Array is:");
        for (uint32_t i = 0; i < (*addressAnchors).size(); i++){
            PRINT_DEBUG_ANCHOR("anchors[%d]: %#llx", i, allAnchors[i]->linkBaseAddress);
        }

    }
    ASSERT(0 && (*addressAnchors).isSorted(compareLinkBaseAddress));
    ASSERT((*binaryUpdate).size() == (*linearUpdate).size());
#endif //VALIDATE_ANCHOR_SEARCH

    PRINT_DEBUG_ANCHOR("search done... %#llx", addr);

#ifdef ANCHOR_SEARCH_BINARY
    delete linearUpdate;
    return binaryUpdate;
#else
    delete binaryUpdate;
    return linearUpdate;
#endif //ANCHOR_SEARCH_BINARY
    __SHOULD_NOT_ARRIVE;
}


uint32_t ElfFile::anchorProgramElements(){

    uint32_t instructionCount = 0;
    PRINT_DEBUG_ANCHOR("Found %d text sections", getNumberOfTextSections());
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        instructionCount += getTextSection(i)->getNumberOfInstructions();
        PRINT_DEBUG_ANCHOR("\tTextSection %d is section %d with %d instructions", i, getTextSection(i)->getSectionIndex(), getTextSection(i)->getNumberOfInstructions());
    }
    PRINT_DEBUG_ANCHOR("Found %d instructions in all sections", instructionCount);

    X86Instruction** allInstructions = new X86Instruction*[instructionCount];
    instructionCount = 0;
    PRINT_DEBUG_ANCHOR("allinst address %lx", allInstructions);
    for (uint32_t i = 0; i < getNumberOfTextSections(); i++){
        instructionCount += getTextSection(i)->getAllInstructions(allInstructions, instructionCount);
    }
    qsort(allInstructions, instructionCount, sizeof(X86Instruction*), compareBaseAddress);

    DEBUG_ANCHOR(
    for (uint32_t i = 0; i < instructionCount; i++){
        allInstructions[i]->print();
    }
    )

    for (uint32_t i = 0; i < instructionCount; i++){
        if (!allInstructions[i]->getBaseAddress()){
            allInstructions[i]->print();
            __SHOULD_NOT_ARRIVE;
        }
    }

    DEBUG_ANCHOR(
    for (uint32_t i = 0; i < instructionCount-1; i++){
        if (allInstructions[i+1]->getBaseAddress() <= allInstructions[i]->getBaseAddress()){
            allInstructions[i]->print();
            allInstructions[i+1]->print();
        }
        ASSERT(allInstructions[i]->getBaseAddress() < allInstructions[i+1]->getBaseAddress() && "Problem with qsort");
    }
    )

    uint32_t addrAlign;
    if (is64Bit()){
        //        addrAlign = sizeof(uint64_t);
        addrAlign = sizeof(uint32_t);
    } else {
        addrAlign = sizeof(uint32_t);
    }

    for (uint32_t i = 0; i < instructionCount; i++){
        X86Instruction* currentInstruction = allInstructions[i];
        ASSERT(!currentInstruction->getAddressAnchor());
        if (currentInstruction->usesRelativeAddress()){
            uint64_t relativeAddress = currentInstruction->getRelativeValue() + currentInstruction->getBaseAddress() + currentInstruction->getSizeInBytes();

            if (!is64Bit()){
                if (!currentInstruction->isControl() || currentInstruction->usesIndirectAddress()){
                    relativeAddress += currentInstruction->getSizeInBytes();
                }
            }
            if (!currentInstruction->getRelativeValue()){
                PRINT_WARN(4,"An Instruction links to null address %#llx", currentInstruction->getRelativeValue());
            }
            PRINT_DEBUG_ANCHOR("Searching for relative address %llx", relativeAddress);

            // search other instructions
            void* link = bsearch(&relativeAddress, allInstructions, instructionCount, sizeof(X86Instruction*), searchBaseAddressExact);
            if (link != NULL){
                X86Instruction* linkedInstruction = *(X86Instruction**)link;
                PRINT_DEBUG_ANCHOR("Found inst -> inst link: %#llx -> %#llx", currentInstruction->getBaseAddress(), relativeAddress);

                currentInstruction->initializeAnchor(linkedInstruction);

                ASSERT(currentInstruction->getAddressAnchor());
                (*addressAnchors).append(currentInstruction->getAddressAnchor());
                currentInstruction->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
            }

            // search special data references
            if (!currentInstruction->getAddressAnchor()){
                for (uint32_t i = 0; i < specialDataRefs.size(); i++){
                    if (specialDataRefs[i]->getBaseAddress() == relativeAddress){
                        PRINT_DEBUG_ANCHOR("Found inst -> sdata link: %#llx -> %#llx", currentInstruction->getBaseAddress(), relativeAddress);
                        currentInstruction->initializeAnchor(specialDataRefs[i]);
                        (*addressAnchors).append(currentInstruction->getAddressAnchor());
                        currentInstruction->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
                    }
                }
            }

            // search non-text sections
            if (!currentInstruction->getAddressAnchor()){
                for (uint32_t i = 0; i < getNumberOfSections(); i++){
                    RawSection* dataRawSection = getRawSection(i);
                    SectionHeader* dataSectionHeader = getSectionHeader(i);

                    PRINT_DEBUG_ANCHOR("Checking section %d for inst->data link: [%#llx,%#llx)", i, dataSectionHeader->GET(sh_addr), dataSectionHeader->GET(sh_addr) + dataRawSection->getSizeInBytes());
                    if (dataRawSection->getType() != PebilClassType_TextSection){
                        PRINT_DEBUG_ANCHOR("\tFound nontext");
                        if (dataSectionHeader->inRange(relativeAddress)){
                            PRINT_DEBUG_ANCHOR("Found inst -> data link: %#llx -> %#llx", currentInstruction->getBaseAddress(), relativeAddress);
                            uint64_t sectionOffset = relativeAddress - dataSectionHeader->GET(sh_addr);
                            uint64_t extendedData = 0;
                            if (dataSectionHeader->hasBitsInFile()){
                                if (addrAlign == sizeof(uint64_t)){
                                    uint64_t currentData = 0;
                                    if (sectionOffset >= dataRawSection->getSizeInBytes()){
                                        PRINT_ERROR("section %s: sectionOffset %d, sizeInBytes %d", dataSectionHeader->getSectionNamePtr(), sectionOffset, dataRawSection->getSizeInBytes());
                                    }
                                    ASSERT(sectionOffset < dataRawSection->getSizeInBytes());
                                    memcpy(&currentData, dataRawSection->getFilePointer() + sectionOffset, addrAlign);
                                    extendedData = currentData;
                                } else if (addrAlign == sizeof(uint32_t)){
                                    uint32_t currentData;
                                    memcpy(&currentData, dataRawSection->getFilePointer()+sectionOffset, addrAlign);
                                    extendedData = (uint64_t)currentData;
                                } else {
                                    __SHOULD_NOT_ARRIVE;
                                }
                            }
                            DataReference* dataRef = new DataReference(extendedData, dataRawSection, addrAlign, sectionOffset);

                            currentInstruction->initializeAnchor(dataRef);
                            (*addressAnchors).append(currentInstruction->getAddressAnchor());
                            currentInstruction->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
                            dataRawSection->addDataReference(dataRef);
                            break;
                        }
                    }
                }
            }

            if (!currentInstruction->getAddressAnchor()){
                PRINT_WARN(4, "Creating special AddressRelocation for %#llx at the behest of the instruction at %#llx since it wasn't an instruction or part of a data section",
                           relativeAddress, currentInstruction->getBaseAddress());
                DataReference* dataRef = new DataReference(0, NULL, addrAlign, relativeAddress);
                specialDataRefs.append(dataRef);
                currentInstruction->initializeAnchor(dataRef);
                (*addressAnchors).append(currentInstruction->getAddressAnchor());
                currentInstruction->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
            }

#if WARNING_SEVERITY <= 4
            if (!currentInstruction->getAddressAnchor()){
                PRINT_WARN(4,"Unable to link the instruction at %#llx (-> %#llx) to another object", currentInstruction->getBaseAddress(), relativeAddress);
                currentInstruction->print();
            }
#endif
        }
    }

    PRINT_DEBUG_ANCHOR("Found %d instructions that required anchoring", (*addressAnchors).size());
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");

    // find the data sections
    Vector<uint16_t> dataSections;
    for (uint32_t i = 1; i < getNumberOfSections(); i++){
        if (getSectionHeader(i)->getSectionNamePtr()){
            if (strstr(getSectionHeader(i)->getSectionNamePtr(),"data")){
                PRINT_DEBUG_ANCHOR("Found data section at %d", i);
                dataSections.append((uint16_t)i);
            }
        }
    }

    TextSection* text = getDotTextSection();

    /*
    uint64_t prevMax = 0;
    uint32_t openBytes = 0;
    uint32_t functionsCounted = 0;
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        if (text->getTextObject(i)->isFunction()){
            Function* f = (Function*)text->getTextObject(i);

            uint64_t maxAddr = 0;
            for (uint32_t j = 0; j < f->getFlowGraph()->getNumberOfBasicBlocks(); j++){
                BasicBlock* bb = f->getFlowGraph()->getBasicBlock(j);
                if (bb->getBaseAddress() + bb->getNumberOfBytes() > maxAddr){
                    maxAddr = bb->getBaseAddress() + bb->getNumberOfBytes();
                }
            }
            if (i > 0){
                uint32_t obytes = f->getBaseAddress() - prevMax;
                openBytes += obytes;
                functionsCounted++;
                PRINT_INFOR("OPENBYTES: %s %d", f->getName(), obytes);

            }
            if (i == text->getNumberOfTextObjects() - 1){
                uint64_t secEnd = text->getSectionHeader()->GET(sh_addr) + text->getSectionHeader()->GET(sh_size);
                uint32_t obytes = secEnd - maxAddr;
                openBytes += obytes;
                functionsCounted++;
                PRINT_INFOR("OPENBYTES: SECTEND %d", obytes);
            } 
            prevMax = maxAddr;
        }
    }
    PRINT_INFOR("Open bytes on functions %d, functions %d", openBytes, functionsCounted);
    */

    for (uint32_t i = 0; i < dataSections.size(); i++){
        RawSection* dataRawSection = getRawSection(dataSections[i]);
        SectionHeader* dataSectionHeader = getSectionHeader(dataSections[i]);

        uint32_t sectionSize = 0;
        if (dataRawSection->getSizeInBytes() > addrAlign){
            sectionSize = dataRawSection->getSizeInBytes() - addrAlign;
        }

        // since there are no constraints on the alignment of stuff in the data sections we must check starting at EVERY byte
        // ^^^NO TO THE ABOVE STATEMENT^^^: we will check just word-aligned addresses since we were getting false positives
        for (int32_t currByte = 0; currByte < sectionSize; currByte += sizeof(uint32_t)){
            char* dataPtr = (char*)(dataRawSection->getFilePointer()+currByte);
            uint64_t extendedData;
            if (addrAlign == sizeof(uint64_t)){
                uint64_t currentData;
                memcpy(&currentData,dataPtr,addrAlign);
                extendedData = currentData;
            } else if (addrAlign == sizeof(uint32_t)){
                uint32_t currentData;
                memcpy(&currentData,dataPtr,addrAlign);
                extendedData = (uint64_t)currentData;
            } else {
                __SHOULD_NOT_ARRIVE;
            }
            PRINT_DEBUG_ANCHOR("data section %d(%x): extendedData is %#016llx", i, dataPtr, extendedData);

            void* link = bsearch(&extendedData,allInstructions,instructionCount,sizeof(X86Instruction*),searchBaseAddressExact);
            if (link != NULL){
#ifndef FILL_RELOCATED_WITH_INTERRUPTS
                for (uint32_t j = 0; j < text->getNumberOfTextObjects(); j++){
                    if (extendedData >= text->getTextObject(j)->getBaseAddress() &&
                        extendedData < text->getTextObject(j)->getBaseAddress() + Size__uncond_jump){
                        //text->getTextObject(j)->print();
                        //PRINT_WARN(10, "Data value %#llx at %#llx refers to function entry at base %#llx", extendedData, dataSectionHeader->GET(sh_addr)+currByte, text->getTextObject(j)->getBaseAddress());
#endif // FILL_RELOCATED_WITH_INTERRUPTS
                if (!(dataSectionHeader->GET(sh_addr)+currByte % sizeof(uint64_t))){
                    PRINT_WARN(10, "unaligned data %#llx at %llx", extendedData, dataSectionHeader->GET(sh_addr)+currByte);
                }

                X86Instruction* linkedInstruction = *(X86Instruction**)link;
                PRINT_DEBUG_ANCHOR("Found data -> inst link: %#llx -> %#llx, offset %x", dataSectionHeader->GET(sh_addr)+currByte, extendedData, currByte);
                DataReference* dataRef = new DataReference(extendedData, dataRawSection, addrAlign, currByte);

                DEBUG_ANCHOR(dataRef->print();)

                dataRef->initializeAnchor(linkedInstruction);
                dataRawSection->addDataReference(dataRef);

                (*addressAnchors).append(dataRef->getAddressAnchor());
                dataRef->getAddressAnchor()->setIndex((*addressAnchors).size()-1);
#ifndef FILL_RELOCATED_WITH_INTERRUPTS
                    }
                }
#endif // FILL_RELOCATED_WITH_INTERRUPTS
            } else {
                //PRINT_DEBUG_ANCHOR("found data %#llx not linked to an instruction", extendedData);
            }
        }
    }

    PRINT_DEBUG_ANCHOR("Found %d anchors total", (*addressAnchors).size());
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");

    for (uint32_t i = 0; i < (*addressAnchors).size(); i++){
        PRINT_DEBUG_ANCHOR("");
        DEBUG_ANCHOR((*addressAnchors)[i]->print();)
    }

    delete[] allInstructions;
    return (*addressAnchors).size();
}
