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

void ElfFileInst::addPLTRelocationEntry(uint32_t symbolIndex){
    ASSERT(extraDataIdx && "Must reserve space for extra data prior to calling this function");
    ASSERT(extraTextIdx && "Must reserve space for extra text prior to calling this function");
    ASSERT(!pltInstructions && !numberOfPLTInstructions && "We should not have already created a PLT");
    ASSERT(!gotEntries && !numberOfGOTEntries && "We should not have already created a GOT");

    DynamicTable* dynTable = elfFile->getDynamicTable();

    ASSERT(dynTable->countDynamics(DT_JMPREL) == 1 && "Cannot find a unique Relocation table for this file");
    
    uint64_t relocTableAddr = dynTable->getDynamicByType(DT_JMPREL,0)->GET_A(d_val,d_un);
    ASSERT(relocTableAddr && "Count not find a relocation table address in the dynamic table");

    RelocationTable* relocTable = (RelocationTable*)elfFile->getRawSection(elfFile->findSectionIdx(relocTableAddr));
    ASSERT(relocTable->getType() == ElfClassTypes_RelocationTable && "Found wrong section type when searching for relocation table");

    PRINT_INFOR("Adding PLT entry to relocation table at section %hd", relocTable->getSectionIndex());

    if (elfFile->is64Bit()){
        uint64_t gotAddress = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset;    
        PRINT_INFOR("64 Relocation info %08x %08x ==> %016llx", symbolIndex, R_X86_64_JUMP_SLOT, ELF64_R_INFO(symbolIndex,R_X86_64_JUMP_SLOT));
        relocOffset = relocTable->addRelocation((uint32_t)gotAddress,ELF64_R_INFO(symbolIndex,R_X86_64_JUMP_SLOT));
    } else {
        uint32_t gotAddress = (uint32_t)(elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset);    
        PRINT_INFOR("32 Relocation info %08x %08x ==> %016llx", symbolIndex, R_386_JMP_SLOT, ELF32_R_INFO(symbolIndex,R_386_JMP_SLOT));
        relocOffset = relocTable->addRelocation(gotAddress,ELF32_R_INFO(symbolIndex,R_386_JMP_SLOT));
    }
    ASSERT(relocOffset && "Should set the relocation offset to a non-trival value");


    SectionHeader* relocationSection = elfFile->getSectionHeader(relocTable->getSectionIndex());
    uint32_t extraSize = relocTable->getRelocationSize();
    relocationSection->INCREMENT(sh_size,extraSize);

    // displace every section in the text segment that comes after the dynamic string section and before the initial text section
    for (uint32_t i = relocationSection->getIndex()+1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        PRINT_INFOR("Adding relocation table: modifying section offset from %lld to %lld", sHdr->GET(sh_offset), sHdr->GET(sh_offset) + extraSize);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }

    // shrink the size of the extra text section to accomodate the increase of the control sections
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);

    for (uint32_t i = 0; i < dynTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_PLTRELSZ){
            dyn->INCREMENT_A(d_val,d_un,relocTable->getRelocationSize());
            ASSERT(dyn->GET_A(d_val,d_un) == relocTable->getNumberOfRelocations()*relocTable->getRelocationSize() && 
                   "The number of entries in the relocation table does not match the amount given in the dynamic table");
        }

        if (tag == DT_REL || tag == DT_RELA){
            if (dyn->GET_A(d_ptr,d_un) > relocationSection->GET(sh_addr)){
                dyn->INCREMENT_A(d_ptr,d_un,extraSize);
            }
        }
    }
    
    elfFile->verify();
}

// the order of the functions called here matters
void ElfFileInst::instrument(){
    PRINT_INFOR("Beginning Instrumentation Of elf file");

    extendTextSection(0x1000);
    PRINT_INFOR("Successfully extended text section/segment");
    extendDataSection(0x100);
    PRINT_INFOR("Successfully extended data section/segment");

    uint32_t symbolIndex = addSharedLibrary("libtest.so", "smalltest");
    PRINT_INFOR("Successfully added shared library");
    
    addPLTRelocationEntry(symbolIndex);
    PRINT_INFOR("Successfully added PLT relocation entries");

    uint32_t pltReturnOffset = generateProcedureLinkageTable();
    PRINT_INFOR("Successfully generated PLT");
    generateGlobalOffsetTable(pltReturnOffset);
    PRINT_INFOR("Successfully generated GOT");

    generateFunctionCall();
}

void ElfFileInst::generateFunctionCall(){
    uint16_t textIdx = elfFile->findSectionIdx(".text");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    SectionHeader* textHeader = elfFile->getSectionHeader(textIdx);
    ASSERT(textSection->getType() == ElfClassTypes_TextSection && ".text section has the wrong type");

    uint64_t replaceAddr = textHeader->GET(sh_addr);
    uint64_t bootstrapAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + bootstrapOffset;

    Instruction* replacementCall = Instruction::generateCallPLT32(replaceAddr,bootstrapAddress);
    textSection->replaceInstructions(textHeader->GET(sh_addr),replacementCall);
}


void ElfFileInst::extendDataSection(uint64_t size){

    ASSERT(!extraDataIdx && !extraDataOffset && "Cannot extend the data segment more than once");

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
    ASSERT(bssSectionIdx != elfFile->getNumberOfSections() && "Could not find the BSS section in the file");
    SectionHeader* bssSection = elfFile->getSectionHeader(bssSectionIdx);
    extraDataIdx = bssSectionIdx + 1;
    SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);

    // increase the memory size of the bss section (note: this section has no size in the file)
    extraDataOffset = 0;
    bssSection->INCREMENT(sh_size,size);

    // increase the memory size of the data segment
    dataHeader->INCREMENT(p_memsz,size);

    elfFile->addSection(extraDataIdx, ElfClassTypes_no_type, NULL, bssSection->GET(sh_name), SHT_PROGBITS,
                        bssSection->GET(sh_flags), bssSection->GET(sh_addr) + bssSection->GET(sh_size), 
                        bssSection->GET(sh_offset), size, bssSection->GET(sh_link), 
                        bssSection->GET(sh_info), 0, bssSection->GET(sh_entsize));
    gotOffset = extraTextOffset;


    // move all later sections' offsets so that they don't conflict with this one
    for (uint32_t i = bssSectionIdx+2; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* scn = elfFile->getSectionHeader(i);
        scn->SET(sh_offset,nextAlignAddress(scn->GET(sh_offset) + size, scn->GET(sh_addralign)));        
    }

    // move the section header table offset if it is after the data section
    if (extendedData->GET(sh_offset) < elfFile->getFileHeader()->GET(e_shoff)){
        elfFile->getFileHeader()->INCREMENT(e_shoff,size);
    }

    PRINT_INFOR("Extra data space available: address [0x%016llx,0x%016llx] -- %d bytes", extendedData->GET(sh_addr), 
                extendedData->GET(sh_addr) + extendedData->GET(sh_size), extendedData->GET(sh_size));
    PRINT_INFOR("Extra data space available: offset   0x%016llx + %lld", extendedData->GET(sh_offset), extraDataOffset);

    elfFile->verify();
}

// reserve space just after the bss section for our GOT
void ElfFileInst::generateGlobalOffsetTable(uint32_t pltReturnOffset){

    ASSERT(extraDataIdx && "Must reserve space for extra data prior to calling this function");
    ASSERT(extraTextIdx && "Must reserve space for extra text prior to calling this function");
    ASSERT(!gotEntries && !numberOfGOTEntries && "Should not have already created a GOT");

    uint32_t pltReturnAddress = (uint32_t)(elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltReturnOffset);

    numberOfGOTEntries = 1;
    ASSERT(numberOfGOTEntries * sizeof(uint32_t) < elfFile->getSectionHeader(extraDataIdx)->GET(sh_size) - extraDataOffset &&
           "Not enough space available for extra GOT entries");

    gotEntries = new uint32_t[numberOfGOTEntries];
    numberOfGOTEntries = 0;
    gotEntries[numberOfGOTEntries] = pltReturnAddress;
    numberOfGOTEntries++;

    PRINT_INFOR("Verifying elf structures after generating GOT");

    elfFile->verify();
}


void ElfFileInst::extendTextSection(uint64_t size){
    PRINT_INFOR("Attempting to extend text segment by %llx ALIGN %llx = %llx", size, elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align), nextAlignAddress(size,elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align)));
    size = nextAlignAddress(size,elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align));
    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ASSERT(!extraTextOffset && !extraTextIdx && "Cannot extend the text segment more than once");

    PRINT_INFOR("Trying to get segments %hd %hd", elfFile->getTextSegmentIdx(), elfFile->getDataSegmentIdx());
    ProgramHeader* textHeader = elfFile->getProgramHeader(elfFile->getTextSegmentIdx());
   ProgramHeader* dataHeader = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());

    // first we will find the address of the first text section. we will be moving all elf
    // control structures that occur prior to this address when we extend the text
    // segment (the interp and note.ABI-tag sections must be in the first text page and it
    // will make certain things easier for all control sections to be together)
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
    ASSERT(lowestTextSectionIdx != elfFile->getNumberOfSections() && "Could not find any text sections in the file");

    // for each segment that is contained within the loadable text segment,
    // update its address to reflect the new base address of the text segment
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* subHeader = elfFile->getProgramHeader(i);
        if (textHeader->inRange(subHeader->GET(p_vaddr)) && i != elfFile->getTextSegmentIdx()){
            PRINT_INFOR("Moving text sub-segment at idx %d by %d bytes", i, size);

            subHeader->SET(p_vaddr,subHeader->GET(p_vaddr)-size);
            subHeader->SET(p_paddr,subHeader->GET(p_paddr)-size);
        } 
    }


    // for each segment that is (or is contained within) the data segment,
    // update its offset to reflect the the base address of the executable
    // (ie the base address of the text segment)
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* subHeader = elfFile->getProgramHeader(i);
        if (dataHeader->inRange(subHeader->GET(p_vaddr))){
            subHeader->INCREMENT(p_offset,size);
        }
    }


    // update section symbols for the sections that were moved. technically the loader won't use them 
    // but we will try to keep the binary as consistent as possible
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symTab = elfFile->getSymbolTable(i);
        for (uint32_t j = 0; j < symTab->getNumberOfSymbols(); j++){
            Symbol* sym = symTab->getSymbol(j);
            if (sym->getSymbolType() == STT_SECTION && sym->GET(st_value) < lowestTextAddress && sym->GET(st_value)){
                sym->SET(st_value,sym->GET(st_value)-size);
            }
        }
    }
    
    // modify the base address of the text segment and increase its size so it ends at the same address
    textHeader->print();
    PRINT_INFOR("Extending this segment by %x bytes", size);
    textHeader->SET(p_vaddr,textHeader->GET(p_vaddr)-size);
    textHeader->SET(p_paddr,textHeader->GET(p_paddr)-size);
    textHeader->INCREMENT(p_memsz,size);
    textHeader->INCREMENT(p_filesz,size);
    textHeader->print();

    // For any section that falls before the program's code, displace its address so that it is in the
    // same location relative to the base address.
    // Likewise, displace the offset of any section that falls during/after the program's code so that
    // the code will be put in the correct location within the text segment.
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        if (i < lowestTextSectionIdx){
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) < lowestTextAddress && "No section that occurs before the first text section should have a larger address");
            // strictly speaking the loader doesn't use these, but for consistency we change them anyway
            sHdr->SET(sh_addr,sHdr->GET(sh_addr)-size);
        } else {
            sHdr->INCREMENT(sh_offset,size);            
        }
    }

    // since some sections were displaced in the file, displace the section header table also so
    // that it occurs after all of the sections in the file
    elfFile->getFileHeader()->INCREMENT(e_shoff,size);


    // update the dynamic table to correctly point to the displaced elf control sections
    for (uint32_t i = 0; i < elfFile->getDynamicTable()->getNumberOfDynamics(); i++){
        Dynamic* dyn = elfFile->getDynamicTable()->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->SET_A(d_ptr,d_un,dyn->GET_A(d_ptr,d_un)-size);
        }
    }


    SectionHeader* textHdr = elfFile->getSectionHeader(lowestTextSectionIdx);
    elfFile->addSection(lowestTextSectionIdx, ElfClassTypes_TextSection, elfFile->getElfFileName(), textHdr->GET(sh_name), textHdr->GET(sh_type),
                        textHdr->GET(sh_flags), textHdr->GET(sh_addr)-size, textHdr->GET(sh_offset)-size, size, textHdr->GET(sh_link), 
                        textHdr->GET(sh_info), textHdr->GET(sh_addralign), textHdr->GET(sh_entsize));
    pltOffset = extraTextOffset;


    extraTextIdx = lowestTextSectionIdx;
    SectionHeader* extendedText = elfFile->getSectionHeader(extraTextIdx);
    
    // this section offset shouldn't conflict with any other sections
    for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
        if (i != lowestTextSectionIdx){
            SectionHeader* scn = elfFile->getSectionHeader(i);
            ASSERT(scn && "Section Header should exist");
            if (scn->GET(sh_offset) >= extendedText->GET(sh_offset) && scn->GET(sh_offset) < extendedText->GET(sh_offset) + extendedText->GET(sh_size)){
                extendedText->print();
                scn->print();
                PRINT_ERROR("Section %d should not begin in the middle of the new PLT section", i);
            } else if (scn->GET(sh_offset) + scn->GET(sh_size) > extendedText->GET(sh_offset) && scn->GET(sh_offset) + scn->GET(sh_size) <= extendedText->GET(sh_offset) + extendedText->GET(sh_size)){
                extendedText->print();
                scn->print();
                PRINT_ERROR("Section %d should not end in the middle of the new PLT sections", i);
                            } else if (extendedText->GET(sh_offset) >= scn->GET(sh_offset) && extendedText->GET(sh_offset) < scn->GET(sh_offset) + scn->GET(sh_size)){
                extendedText->print();
                scn->print();
                PRINT_ERROR("The new PLT section should not be contained by section %d", i);
            }
        }
    }


    elfFile->verify();

    PRINT_INFOR("Extra text space available: address [0x%016llx,0x%016llx] -- %d bytes", extendedText->GET(sh_addr), extendedText->GET(sh_addr) + extendedText->GET(sh_size), extendedText->GET(sh_size));
    PRINT_INFOR("Extra text space available: offset   0x%016llx + %lld", extendedText->GET(sh_offset), extraTextOffset);
}


// reserve space immediately preceeding the rest of the code for our PLT
uint32_t ElfFileInst::generateProcedureLinkageTable(){

    ASSERT(extraDataIdx && "Must reserve space for extra data prior to calling this function");
    ASSERT(extraTextIdx && "Must reserve space for extra text prior to calling this function");
    ASSERT(!pltInstructions && !numberOfPLTInstructions && "We should not have already created a PLT");
    ASSERT(!gotEntries && !numberOfGOTEntries && "We should not have already created a GOT");

    if (elfFile->is64Bit()){
        return generateProcedureLinkageTable64();
    } else {
        return generateProcedureLinkageTable32();
    }
}

uint32_t ElfFileInst::generateProcedureLinkageTable64(){
    PRINT_ERROR("ElfFileInst::generateProcedureLinkageTable has not been written for 64bit yet");
    ASSERT(0 && "Cannot continue");
    return 0;
}

uint32_t ElfFileInst::generateProcedureLinkageTable32(){
    uint32_t pltSize = 0;
    uint32_t gotAddress = (uint32_t)(elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset);

    numberOfPLTInstructions = 3;
    pltInstructions = new Instruction*[numberOfPLTInstructions];
    numberOfPLTInstructions = 0;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateJumpDirect32(gotAddress);
    uint32_t pltReturnOffset = pltInstructions[numberOfPLTInstructions]->getLength();
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    uint32_t gotInfo = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;
    numberOfPLTInstructions++;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateStackPushImmediate32(relocOffset);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;


    // find the plt section
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    
    uint16_t realPLTSectionIdx = elfFile->findSectionIdx(".plt");
    ASSERT(realPLTSectionIdx && "Cannot find a section named `.plt`");
    uint32_t realPLTAddress = elfFile->getSectionHeader(realPLTSectionIdx)->GET(sh_addr);
    uint32_t returnAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateJumpRelative32(returnAddress,realPLTAddress);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;

    ASSERT(pltSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += pltSize;

    bootstrapOffset = extraTextOffset;
    uint32_t bootstrapSize = 0;
    numberOfBootstrapInstructions = 5;
    bootstrapInstructions = new Instruction*[numberOfBootstrapInstructions];
    numberOfBootstrapInstructions = 0;
    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush32(1);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveImmToReg32(gotInfo,1);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveRegToMem32(1,gotAddress);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPop32(1);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;


    uint64_t pltAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset;
    uint64_t currAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + bootstrapOffset + bootstrapSize;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateCallPLT32(currAddress,pltAddress);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    ASSERT(bootstrapSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += bootstrapSize;


    PRINT_INFOR("Verifying elf structures after adding our PLT");

    elfFile->verify();

    return pltReturnOffset;
}




ElfFileInst::~ElfFileInst(){
    if (pltInstructions){
        for (uint32_t i = 0; i < numberOfPLTInstructions; i++){
            delete pltInstructions[i];
        }
        delete[] pltInstructions;
    }
    if (bootstrapInstructions){
        for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
            delete bootstrapInstructions[i];
        }
        delete[] bootstrapInstructions;
    }
    if (gotEntries){
        delete[] gotEntries;
    }

}

void ElfFileInst::dump(char* extension){
    char fileName[80] = "";
    sprintf(fileName,"%s.%s", elfFile->getElfFileName(), extension);

    PRINT_INFOR("Output file is %s", fileName);

    BinaryOutputFile binaryOutputFile;
    binaryOutputFile.open(fileName);
    if(!binaryOutputFile){
        PRINT_ERROR("The output file can not be opened %s",fileName);
    }

    elfFile->dump(&binaryOutputFile,ELF_FILE_HEADER_OFFSET);
    dump(&binaryOutputFile,ELF_FILE_HEADER_OFFSET);

    binaryOutputFile.close();
}

void ElfFileInst::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(offset == ELF_FILE_HEADER_OFFSET && "Instrumentation must be dumped at the begining of the output file");

    uint32_t currentOffset = (uint32_t)elfFile->getSectionHeader(extraTextIdx)->GET(sh_offset) + pltOffset;
    for (uint32_t i = 0; i < numberOfPLTInstructions; i++){
        pltInstructions[i]->dump(binaryOutputFile,currentOffset);
        PRINT_INFOR("Dumped our personal PLT instruction at address %x", currentOffset);
        pltInstructions[i]->print();
        currentOffset += pltInstructions[i]->getLength();
    }

    currentOffset = elfFile->getSectionHeader(extraTextIdx)->GET(sh_offset) + bootstrapOffset;
    for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
        bootstrapInstructions[i]->dump(binaryOutputFile,currentOffset);
        bootstrapInstructions[i]->print();
        currentOffset += bootstrapInstructions[i]->getLength();
    }

    currentOffset = elfFile->getSectionHeader(extraDataIdx)->GET(sh_offset) + gotOffset;
    for (uint32_t i = 0; i < numberOfGOTEntries; i++){
        binaryOutputFile->copyBytes((char*)&gotEntries[i],sizeof(gotEntries[i]),currentOffset);
        PRINT_INFOR("Dumped our personal GOT entry[%d]=%x at address %x", i, gotEntries[i], currentOffset); 
        currentOffset += sizeof(gotEntries[i]);
    }

}


void ElfFile::initTextSections(){
    uint32_t numberOfFunctions = 0;
    for (uint32_t i = 0; i < numberOfSections; i++){
        if (sectionHeaders[i]->getSectionType() == ElfClassTypes_TextSection){
            TextSection* textSection = (TextSection*)rawSections[i];
            numberOfFunctions += textSection->findFunctions();
        }
    }
    PRINT_INFOR("Found %d functions in all sections", numberOfFunctions);
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
        //        PRINT_INFOR("Verifying address ranges [%lld,%lld],[%lld,%lld]", prevBegin, prevBegin+prevSize, currBegin, currBegin+currSize);
        if (prevBegin+prevSize > currBegin && currSize != 0){
            PRINT_ERROR("Address ranges [%lld,%lld],[%lld,%lld] should not intersect", prevBegin, prevBegin+prevSize, currBegin, currBegin+currSize);
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


uint64_t ElfFile::addSection(uint16_t idx, ElfClassTypes classtype, char* bytes, uint32_t name, uint32_t type, uint64_t flags, uint64_t addr, uint64_t offset, 
                             uint64_t size, uint32_t link, uint32_t info, uint64_t addralign, uint64_t entsize){

    ASSERT(sectionHeaders && "sectionHeaders should be initialized");
    ASSERT(rawSections && "rawSections should be initialized");

    PRINT_INFOR("Adding section at address %llx(offset %llx) with size %lld bytes", addr, offset, size);


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
            PRINT_INFOR("Changing offset of section %d from %llx to %llx", i, currentOffset, newOffset);
            sectionHeaders[i]->SET(sh_offset,newOffset);
        }
    }

    return sectionHeaders[idx]->GET(sh_addr);
}

void ElfFileInst::print(){
    elfFile->print();
    PRINT_INFOR("");
    PRINT_INFOR("Instrumentation Reservations:");
    if (extraTextIdx){
        SectionHeader* extendedText = elfFile->getSectionHeader(extraTextIdx);
        PRINT_INFOR("Extended TEXT section is section %hd", extraTextIdx);
        PRINT_INFOR("\tTEXT addresses reserved -- \t[0x%016llx,0x%016llx] (%lld bytes)", extendedText->GET(sh_addr), 
                    extendedText->GET(sh_addr) + extendedText->GET(sh_size), extendedText->GET(sh_size));
        PRINT_INFOR("\tTEXT offsets   reserved -- \t0x%016llx (%lld of %lld bytes used)", extendedText->GET(sh_offset), extraTextOffset, extendedText->GET(sh_size));
    }
    if (extraDataIdx){
        SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);
        PRINT_INFOR("Extended DATA section is section %hd", extraDataIdx);
        PRINT_INFOR("\tDATA addresses reserved -- \t[0x%016llx,0x%016llx] (%lld bytes)", extendedData->GET(sh_addr), 
                    extendedData->GET(sh_addr) + extendedData->GET(sh_size), extendedData->GET(sh_size));
        PRINT_INFOR("\tDATA offsets   reserved -- \t0x%016llx (%lld of %lld bytes used)", extendedData->GET(sh_offset), extraDataOffset, extendedData->GET(sh_size));
    }
}


ElfFileInst::ElfFileInst(ElfFile* elf){
    elfFile = elf;

    extraTextIdx = 0;
    extraTextOffset = 0;
    extraDataIdx = 0;
    extraDataOffset = 0;

    relocOffset = 0;

    pltOffset = 0;
    pltInstructions = NULL;
    numberOfPLTInstructions = 0;

    bootstrapOffset = 0;
    bootstrapInstructions = NULL;
    numberOfBootstrapInstructions = 0;

    gotOffset = 0;
    gotEntries = NULL;
    numberOfGOTEntries = 0;
}

uint32_t ElfFileInst::addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx){
    ASSERT(!numberOfGOTEntries && !gotEntries && "Cannot add to sections after generating GOT");
    ASSERT(!numberOfPLTInstructions && !pltInstructions && "Cannot add to sections after generating PLT");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t entrySize;
    if (elfFile->is64Bit()){
        entrySize = Size__64_bit_Symbol;
    } else {
        entrySize = Size__32_bit_Symbol;
    }
    ASSERT(entrySize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_offset) - extraTextOffset && "There is not enough extra space in the text section to extend the symbol table");

    PRINT_INFOR("Adding symbol with size %d", entrySize);

    uint64_t symtabAddr = dynamicTable->getDynamicByType(DT_SYMTAB,0)->GET_A(d_val,d_un);
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
    dynamicSymbolSection->INCREMENT(sh_size,entrySize);

    uint32_t extraSize = entrySize;
    // displace every section that comes after the dynamic symbol section and before the code
    for (uint32_t i = dynamicSymbolSection->getIndex()+1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,entrySize);
        sHdr->INCREMENT(sh_addr,entrySize);
    }
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);


    // adjust the dynamic table entries for the section shifts
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED || tag == DT_STRTAB ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            ASSERT(dyn->GET_A(d_ptr,d_un) > dynamicSymbolSection->GET(sh_addr) && "The gnu version tables and relocation tables should be after the dynamic symbol table");
            dyn->INCREMENT_A(d_ptr,d_un,entrySize);
        }

    }

    // add an entry to the hash table
    HashTable* hashTable = elfFile->getHashTable();
    hashTable->addChain();
    if (hashTable->getNumberOfBuckets() < hashTable->getNumberOfChains()/2){
        expandHashTable();
    }    

    GnuVersymTable* versymTable = elfFile->getGnuVersymTable();
    SectionHeader* versymHeader = elfFile->getSectionHeader(versymTable->getSectionIndex());
    versymTable->addSymbol(VER_NDX_GLOBAL);
    if (elfFile->is64Bit()){
        entrySize = Size__64_bit_Gnu_Versym;
    } else {
        entrySize = Size__32_bit_Gnu_Versym;
    }
    extraSize = entrySize;
    versymHeader->INCREMENT(sh_size,entrySize);

    // displace every section that comes after the gnu versym section and before the code
    for (uint32_t i = versymHeader->getIndex()+1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        PRINT_INFOR("Adding %d bytes to offset of section %d to align %llx/%lld", extraSize, i, sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign));
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    } 
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);

    dynamicTable->getDynamicByType(DT_VERSYM,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getGnuVersymTable()->getSectionIndex())->GET(sh_addr));
    dynamicTable->getDynamicByType(DT_VERNEED,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getGnuVerneedTable()->getSectionIndex())->GET(sh_addr));
    dynamicTable->getDynamicByType(DT_STRTAB,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getDynamicStringTable()->getSectionIndex())->GET(sh_addr));
    if (dynamicTable->getDynamicByType(DT_REL,0)){
        dynamicTable->getDynamicByType(DT_REL,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getDynamicRelocationTable()->getSectionIndex())->GET(sh_addr));
    } else {
        dynamicTable->getDynamicByType(DT_RELA,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getDynamicRelocationTable()->getSectionIndex())->GET(sh_addr));
    }
    dynamicTable->getDynamicByType(DT_JMPREL,0)->SET_A(d_ptr,d_un,elfFile->getSectionHeader(elfFile->getPLTRelocationTable()->getSectionIndex())->GET(sh_addr));
   
    return symbolIndex;
}

uint32_t ElfFileInst::expandHashTable(){

    ASSERT(!numberOfGOTEntries && !gotEntries && "Cannot add to sections after generating GOT");
    ASSERT(!numberOfPLTInstructions && !pltInstructions && "Cannot add to sections after generating PLT");

    HashTable* hashTable = elfFile->getHashTable();
    uint32_t extraHashEntries = hashTable->expandSize(hashTable->getNumberOfChains()/2);

    PRINT_INFOR("Expanding the hash table by %d entries", extraHashEntries);

    SectionHeader* hashHeader = elfFile->getSectionHeader(hashTable->getSectionIndex());
    uint32_t extraSize = extraHashEntries * hashTable->getEntrySize();
    hashHeader->INCREMENT(sh_size,extraSize);

    PRINT_INFOR("Expanding the hash table by %d bytes", extraSize);

    for (uint32_t i = hashTable->getSectionIndex() + 1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);
    PRINT_INFOR("Expanding the hash table by %d bytes", extraSize);

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->INCREMENT_A(d_ptr,d_un,extraSize);
        }

    }

    return extraSize;
}

uint32_t ElfFileInst::addStringToDynamicStringTable(const char* str){

    ASSERT(!numberOfGOTEntries && !gotEntries && "Cannot add to sections after generating GOT");
    ASSERT(!numberOfPLTInstructions && !pltInstructions && "Cannot add to sections after generating PLT");

    ASSERT(extraTextIdx && "Should extend the text section before calling this function");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strSize = strlen(str) + 1;
    ASSERT(strSize < elfFile->getSectionHeader(extraDataIdx)->GET(sh_size) - extraTextOffset && "There is not enough extra space in the text section to extend the string table");

    // find the string table we will be adding to
    uint64_t stringTableAddr = dynamicTable->getDynamicByType(DT_STRTAB,0)->GET_A(d_val,d_un);
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
    dynamicStringSection->INCREMENT(sh_size,extraSize);

    // displace every section in the text segment that comes after the dynamic string section and before the initial text section
    for (uint32_t i = dynamicStringSection->getIndex()+1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        PRINT_INFOR("Adding string table: modifying section offset from %lld to %lld", sHdr->GET(sh_offset), sHdr->GET(sh_offset) + extraSize);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }

    // shrink the size of the extra text section to accomodate the increase of the control sections
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);
    elfFile->getSectionHeader(extraTextIdx)->print();

    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            ASSERT(dyn->GET_A(d_ptr,d_un) > dynamicStringSection->GET(sh_addr) && "The gnu version tables and relocation tables should be after the dynamic string table");
            dyn->INCREMENT_A(d_ptr,d_un,extraSize);
        }

    }

    return origSize;

}


uint32_t ElfFileInst::addSharedLibrary(const char* libname, const char* funcname){

    ASSERT(!numberOfGOTEntries && !gotEntries && "Cannot add to sections after generating GOT");
    ASSERT(!numberOfPLTInstructions && !pltInstructions && "Cannot add to sections after generating PLT");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strOffset = addStringToDynamicStringTable(libname);

    // add a DT_NEEDED entry to the dynamic table
    uint32_t emptyDynamicIdx = dynamicTable->findEmptyDynamic();

    ASSERT(emptyDynamicIdx < dynamicTable->getNumberOfDynamics() && "No free entries found in the dynamic table");

    dynamicTable->getDynamic(emptyDynamicIdx)->SET(d_tag,DT_NEEDED);
    dynamicTable->getDynamic(emptyDynamicIdx)->SET_A(d_ptr,d_un,strOffset);

    uint32_t funcNameOffset = addStringToDynamicStringTable(funcname);
    uint32_t symbolIndex = addSymbolToDynamicSymbolTable(funcNameOffset, 0, 0, STB_GLOBAL, STT_FUNC, 0, 0);

    PRINT_INFOR("Verifying elf file after adding shared library");

    elfFile->verify();

    return symbolIndex;
}


uint64_t ElfFileInst::relocateDynamicSection(){
    
    ASSERT(0 && "This function should not be used, relocating the dynamic table is not easy and we won't do unless absolutely necessary");
    ASSERT(!numberOfGOTEntries && !gotEntries && "Cannot add to sections after generating GOT");
    ASSERT(!numberOfPLTInstructions && !pltInstructions && "Cannot add to sections after generating PLT");

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

        PRINT_INFOR("shstrndx is %d, found %d", fileHeader->GET(e_shstrndx), stringTables[i]->getSectionIndex());

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

void ElfFile::print() 
{ 

    PRINT_INFOR("File Header");
    PRINT_INFOR("===========");
    if(fileHeader){
        fileHeader->print(); 
    }
    
    PRINT_INFOR("Section Headers: %d",numberOfSections);
    PRINT_INFOR("=================");
    if (sectionHeaders){
        for (uint32_t i = 0; i < numberOfSections; i++){
            if (sectionHeaders[i]){
                sectionHeaders[i]->print();
            }
        }
    }

    PRINT_INFOR("Program Headers: %d",numberOfPrograms);
    PRINT_INFOR("================");
    if (programHeaders){
        for (uint32_t i = 0; i < numberOfPrograms; i++){
            if (programHeaders[i]){
                programHeaders[i]->print();
            }
        }
    }

    PRINT_INFOR("Note Sections: %d",numberOfNoteSections);
    PRINT_INFOR("=================");
    if (noteSections){
        for (uint32_t i = 0; i < numberOfNoteSections; i++){
            noteSections[i]->print();
        }
    }

    PRINT_INFOR("Symbol Tables: %d",numberOfSymbolTables);
    PRINT_INFOR("==============");
    if (symbolTables){
        for (uint32_t i = 0; i < numberOfSymbolTables; i++){
            if (symbolTables[i])
                symbolTables[i]->print();
        }
    }

    PRINT_INFOR("Relocation Tables: %d",numberOfRelocationTables);
    PRINT_INFOR("==================");
    if (relocationTables){
        for (uint32_t i = 0; i < numberOfRelocationTables; i++){
            if (relocationTables[i]){
                relocationTables[i]->print();
            }
        }
    }

    PRINT_INFOR("String Tables: %d",numberOfStringTables);
    PRINT_INFOR("==============");
    if (stringTables){
        for (uint32_t i = 0; i < numberOfStringTables; i++){       
            if (stringTables[i]){
                stringTables[i]->print();
            }
        }
    }

    PRINT_INFOR("Global Offset Table");
    PRINT_INFOR("===================");
    if (globalOffsetTable){
        globalOffsetTable->print();
    }

    PRINT_INFOR("Hash Table");
    PRINT_INFOR("=============");
    if (hashTable){
        hashTable->print();
    }

    PRINT_INFOR("Dynamic Table");
    PRINT_INFOR("=============");
    if (dynamicTable){
        dynamicTable->print();
        dynamicTable->printSharedLibraries(&binaryInputFile);
    }

    PRINT_INFOR("Gnu Verneed Table");
    PRINT_INFOR("=============");
    if (gnuVerneedTable){
        gnuVerneedTable->print();
    }

    PRINT_INFOR("Gnu Versym Table");
    PRINT_INFOR("=============");
    if (gnuVersymTable){
        gnuVersymTable->print();
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



uint32_t ElfFile::printDisassembledCode(){
    uint32_t numInstrs = 0;

    ASSERT(disassembler && "disassembler should be initialized");

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
    PRINT_INFOR("dumped file header");

    currentOffset = fileHeader->GET(e_phoff);
    for (uint32_t i = 0; i < numberOfPrograms; i++){
        programHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += programHeaders[i]->getSizeInBytes();
    }
    PRINT_INFOR("dumped %d program headers", numberOfPrograms);

    currentOffset = fileHeader->GET(e_shoff);
    for (uint32_t i = 0; i < numberOfSections; i++){
        sectionHeaders[i]->dump(binaryOutputFile,currentOffset);
        currentOffset += sectionHeaders[i]->getSizeInBytes();
    }
    PRINT_INFOR("dumped %d section headers", numberOfSections);

    for (uint32_t i = 0; i < numberOfSections; i++){
        currentOffset = sectionHeaders[i]->GET(sh_offset);
        if (sectionHeaders[i]->hasBitsInFile()){
            rawSections[i]->dump(binaryOutputFile,currentOffset);
        }
    }
    PRINT_INFOR("dumped %d raw sections", numberOfSections);
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
    ASSERT(numberOfSections && "FATAL : This file has no segments!!!!!");

    fileHeader->print();

}

void ElfFile::readProgramHeaders(){

    PRINT_INFOR("Parsing the program header table");
    programHeaders = new ProgramHeader*[numberOfPrograms];
    ASSERT(programHeaders);

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_phoff)));
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

    binaryInputFile.setInPointer(binaryInputFile.fileOffsetToPointer(fileHeader->GET(e_shoff)));
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
    }

        // determine and set section type for each section header
    PRINT_INFOR("Setting section types");
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
            PRINT_INFOR("Section %d is gnu version symbol", i);
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


