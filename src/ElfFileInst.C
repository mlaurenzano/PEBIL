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

    uint64_t gotAddress = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset;    
    if (elfFile->is64Bit()){
        PRINT_INFOR("64 Relocation info %08x %08x ==> %016llx", symbolIndex, R_X86_64_JUMP_SLOT, ELF64_R_INFO(symbolIndex,R_X86_64_JUMP_SLOT));
        relocOffset = relocTable->addRelocation(gotAddress,ELF64_R_INFO(symbolIndex,R_X86_64_JUMP_SLOT));
    } else {
        PRINT_INFOR("32 Relocation info %08x %08x ==> %08x", symbolIndex, R_386_JMP_SLOT, ELF32_R_INFO(symbolIndex,R_386_JMP_SLOT));
        relocOffset = relocTable->addRelocation(gotAddress,ELF32_R_INFO(symbolIndex,R_386_JMP_SLOT));

        // in 32bit the linker uses an offset into the table instead of its index
        relocOffset *= relocTable->getRelocationSize();
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

    Instruction** replacementCalls = new Instruction*[1];
    replacementCalls[0] = Instruction::generateJumpRelative(replaceAddr,bootstrapAddress);
    uint32_t replacementLength = 0;
    for (uint32_t i = 0; i < 1; i++){
        replacementLength += replacementCalls[i]->getLength();
    }

    Instruction** replacedInstructions = NULL;
    uint32_t numberOfReplacedInstructions = textSection->replaceInstructions(textHeader->GET(sh_addr),replacementCalls,1,&replacedInstructions);
    delete[] replacementCalls;

    uint32_t numOfRegisterSaves;
    if (elfFile->is64Bit()){
        numOfRegisterSaves = X86_64BIT_GPRS;
    } else {
        numOfRegisterSaves = X86_32BIT_GPRS;
    }

    uint32_t trampSize = 0;
    numberOfTrampInstructions = numberOfReplacedInstructions + 1 + numOfRegisterSaves + 1;
    trampInstructions = new Instruction*[numberOfTrampInstructions];
    for (uint32_t i = 0; i < numOfRegisterSaves; i++){
        trampInstructions[i] = Instruction::generateStackPop64(numOfRegisterSaves-1-i);
        trampSize += trampInstructions[i]->getLength();
    }

    trampInstructions[numOfRegisterSaves] = Instruction::generatePopEflags();
    trampSize += trampInstructions[numOfRegisterSaves]->getLength();

    for (uint32_t i = 0; i < numberOfReplacedInstructions; i++){
        trampInstructions[i+numOfRegisterSaves+1] = replacedInstructions[i];
        trampSize += trampInstructions[i+numOfRegisterSaves+1]->getLength();
    }

    trampInstructions[numberOfTrampInstructions-1] = Instruction::generateJumpRelative(elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + extraTextOffset + trampSize,
                                                                                         textHeader->GET(sh_addr) + replacementLength);
    trampSize += trampInstructions[numberOfTrampInstructions-1]->getLength();

    ASSERT(trampSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);

    trampOffset = extraTextOffset;
    extraTextOffset += trampSize;

    delete[] replacedInstructions;
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
    uint32_t pltSize = 0;
    uint32_t gotAddress = (uint32_t)(elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset);

    numberOfPLTInstructions = 3;
    pltInstructions = new Instruction*[numberOfPLTInstructions];
    numberOfPLTInstructions = 0;

    uint32_t returnAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;
    pltInstructions[numberOfPLTInstructions] = Instruction::generateIndirectRelativeJump64(returnAddress,gotAddress);
    uint32_t pltReturnOffset = pltInstructions[numberOfPLTInstructions]->getLength();
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    uint32_t gotInfo = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;
    numberOfPLTInstructions++;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateStackPushImmediate(relocOffset);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;

    // find the plt section
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    
    uint16_t realPLTSectionIdx = elfFile->findSectionIdx(".plt");
    ASSERT(realPLTSectionIdx && "Cannot find a section named `.plt`");
    uint32_t realPLTAddress = elfFile->getSectionHeader(realPLTSectionIdx)->GET(sh_addr);
    returnAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateJumpRelative(returnAddress,realPLTAddress);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;

    ASSERT(pltSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += pltSize;

    bootstrapOffset = extraTextOffset;
    uint32_t bootstrapSize = 0;
    numberOfBootstrapInstructions = 8 + X86_64BIT_GPRS + 1;
    bootstrapInstructions = new Instruction*[numberOfBootstrapInstructions];
    numberOfBootstrapInstructions = 0;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush64(X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush64(X86_REG_DX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveImmToReg(gotInfo,X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveImmToReg(gotAddress,X86_REG_DX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveRegToRegaddr(X86_REG_CX,X86_REG_DX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;    

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPop64(X86_REG_DX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPop64(X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generatePushEflags();
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush64(i);
        bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
        numberOfBootstrapInstructions++;
    }


    uint64_t pltAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset;
    uint64_t currAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + bootstrapOffset + bootstrapSize;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateCallRelative(currAddress,pltAddress);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    ASSERT(bootstrapSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += bootstrapSize;


    PRINT_INFOR("Verifying elf structures after adding our PLT");

    elfFile->verify();

    return pltReturnOffset;
    PRINT_ERROR("ElfFileInst::generateProcedureLinkageTable has not been written for 64bit yet");
    ASSERT(0 && "Cannot continue");
}

uint32_t ElfFileInst::generateProcedureLinkageTable32(){
    uint32_t pltSize = 0;
    uint32_t gotAddress = (uint32_t)(elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset);

    numberOfPLTInstructions = 3;
    pltInstructions = new Instruction*[numberOfPLTInstructions];
    numberOfPLTInstructions = 0;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateJumpIndirect32(gotAddress);
    uint32_t pltReturnOffset = pltInstructions[numberOfPLTInstructions]->getLength();
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    uint32_t gotInfo = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;
    numberOfPLTInstructions++;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateStackPushImmediate(relocOffset);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;


    // find the plt section
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    
    uint16_t realPLTSectionIdx = elfFile->findSectionIdx(".plt");
    ASSERT(realPLTSectionIdx && "Cannot find a section named `.plt`");
    uint32_t realPLTAddress = elfFile->getSectionHeader(realPLTSectionIdx)->GET(sh_addr);
    uint32_t returnAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset + pltSize;

    pltInstructions[numberOfPLTInstructions] = Instruction::generateJumpRelative(returnAddress,realPLTAddress);
    pltSize += pltInstructions[numberOfPLTInstructions]->getLength();
    numberOfPLTInstructions++;

    ASSERT(pltSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += pltSize;

    bootstrapOffset = extraTextOffset;
    uint32_t bootstrapSize = 0;
    numberOfBootstrapInstructions = 5 + X86_32BIT_GPRS + 1;
    bootstrapInstructions = new Instruction*[numberOfBootstrapInstructions];
    numberOfBootstrapInstructions = 0;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush32(X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveImmToReg(gotInfo,X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveRegToMem(X86_REG_CX,gotAddress);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPop32(X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generatePushEflags();
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateStackPush32(i);
        bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
        numberOfBootstrapInstructions++;
    }


    uint64_t pltAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset;
    uint64_t currAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + bootstrapOffset + bootstrapSize;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateCallRelative(currAddress,pltAddress);
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
    if (trampInstructions){
        for (uint32_t i = 0; i < numberOfTrampInstructions; i++){
            delete trampInstructions[i];
        }
        delete[] trampInstructions;
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

    currentOffset = elfFile->getSectionHeader(extraTextIdx)->GET(sh_offset) + trampOffset;
    for (uint32_t i = 0; i < numberOfTrampInstructions; i++){
        trampInstructions[i]->dump(binaryOutputFile,currentOffset);
        trampInstructions[i]->print();
        currentOffset += trampInstructions[i]->getLength();
    }

    currentOffset = elfFile->getSectionHeader(extraDataIdx)->GET(sh_offset) + gotOffset;
    for (uint32_t i = 0; i < numberOfGOTEntries; i++){
        binaryOutputFile->copyBytes((char*)&gotEntries[i],sizeof(gotEntries[i]),currentOffset);
        PRINT_INFOR("Dumped our personal GOT entry[%d]=%x at address %x", i, gotEntries[i], currentOffset); 
        currentOffset += sizeof(gotEntries[i]);
    }

    

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

    trampOffset = 0;
    trampInstructions = NULL;
    numberOfTrampInstructions = 0;

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
