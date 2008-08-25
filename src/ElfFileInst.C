#include <Base.h>
#include <ElfFileInst.h>
#include <ElfFile.h>
#include <DynamicTable.h>
#include <StringTable.h>
#include <SectionHeader.h>
#include <ProgramHeader.h>
#include <HashTable.h>
#include <RelocationTable.h>
#include <GnuVersymTable.h>
#include <GnuVerneedTable.h>
#include <Instruction.h>
#include <CStructuresX86.h>
#include <TextSection.h>
#include <FileHeader.h>
#include <Instrumentation.h>

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

void ElfFileInst::generateInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    uint16_t textIdx = elfFile->findSectionIdx(".text");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    SectionHeader* textHeader = elfFile->getSectionHeader(textIdx);
    ASSERT(textSection->getType() == ElfClassTypes_TextSection && ".text section has the wrong type");

    uint16_t pltIdx = elfFile->findSectionIdx(".plt");
    TextSection* pltSection = (TextSection*)elfFile->getRawSection(pltIdx);
    ASSERT(pltSection->getType() == ElfClassTypes_TextSection && ".plt section has the wrong type");

    InstrumentationSnippet* snip = new InstrumentationSnippet(IDX_INST_BOOTSTRAP_BEGIN);
    snip->addInstruction(Instruction::generateStackPush32(X86_REG_CX));
    snip->addInstruction(Instruction::generateStackPush32(X86_REG_DX));
    instrumentations[IDX_INST_BOOTSTRAP_BEGIN] = snip;

    snip = new InstrumentationSnippet(IDX_INST_BOOTSTRAP_END);
    snip->addInstruction(Instruction::generateStackPop32(X86_REG_DX));
    snip->addInstruction(Instruction::generateStackPop32(X86_REG_CX));
    snip->addInstruction(Instruction::generateReturn());
    instrumentations[IDX_INST_BOOTSTRAP_END] = snip;

    ASSERT(!instrumentationPoints[IDX_POINT_BOOTSTRAP] && "instrumentationPoint[IDX_POINT_BOOTSTRAP] is reserved");
    for (uint32_t i = 0; i < textSection->getNumberOfFunctions(); i++){
        if (!strcmp(textSection->getFunction(i)->getFunctionName(),"_start")){
            ASSERT(!instrumentationPoints[IDX_POINT_BOOTSTRAP] && "Found more than one start function");
            instrumentationPoints[IDX_POINT_BOOTSTRAP] = new InstrumentationPoint(IDX_POINT_BOOTSTRAP, (Base*)textSection->getFunction(i), instrumentations[IDX_INST_BOOTSTRAP_BEGIN]);
        }
    }

    // compute the size of the bootstrap code
    uint32_t bootstrapSize = 0;
    bootstrapSize += instrumentations[IDX_INST_BOOTSTRAP_BEGIN]->sizeNeeded();
    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        if(instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
            InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];
            bootstrapSize += func->bootstrapSize();
        }
    }
    bootstrapSize += instrumentations[IDX_INST_BOOTSTRAP_END]->sizeNeeded();
    PRINT_INFOR("Saving %d bytes for bootstrap code", bootstrapSize);

    uint32_t precodeOffset = 0;
    uint32_t codeOffset = bootstrapSize;
    uint32_t dataOffset = 0;

    ASSERT(instrumentations[IDX_INST_BOOTSTRAP_BEGIN]->getType() == ElfClassTypes_InstrumentationSnippet);
    snip = ((InstrumentationSnippet*)instrumentations[IDX_INST_BOOTSTRAP_BEGIN]);
    snip->setCodeOffsets(precodeOffset);
    precodeOffset += snip->sizeNeeded();

    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        ASSERT(instrumentations[i] && "Instrumentations should be initialized by now");
        if (instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
            InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];
            
            uint64_t bootstrapOffset = precodeOffset;
            precodeOffset += func->bootstrapSize();
            
            uint64_t procedureLinkOffset = codeOffset;
            codeOffset += func->procedureLinkSize();
            uint64_t wrapperOffset = codeOffset;
            codeOffset += func->wrapperSize();
            
            uint64_t globalDataOffset = dataOffset;
            PRINT_INFOR("Global data offset = %lld %lld", globalDataOffset, func->getGlobalDataOffset());
            ASSERT(globalDataOffset == func->getGlobalDataOffset());
            dataOffset += func->globalDataSize();
            
            func->setCodeOffsets(procedureLinkOffset, bootstrapOffset, wrapperOffset);
            func->print();
        } else {
            dataOffset += Size__32_bit_Global_Offset_Table_Entry;
        }
    }

    ASSERT(instrumentations[IDX_INST_BOOTSTRAP_END]->getType() == ElfClassTypes_InstrumentationSnippet);
    snip = ((InstrumentationSnippet*)instrumentations[IDX_INST_BOOTSTRAP_END]);
    snip->setCodeOffsets(precodeOffset);
    precodeOffset += snip->sizeNeeded();

    for (uint32_t i = 0; i < numberOfInstrumentationPoints; i++){
        InstrumentationPoint* pt = instrumentationPoints[i];
        if (!pt){
            PRINT_ERROR("Instrumentation point %d should exist", i);
        }
        PRINT_INFOR("Looking at point %d", i);
        Instruction** repl = new Instruction*[1];
        repl[0] = Instruction::generateJumpRelative(pt->getSourceAddress(), elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + codeOffset);
        Instruction** displaced = NULL;
        uint32_t numberOfDisplacedInstructions = textSection->replaceInstructions(pt->getSourceAddress(), repl, 1, &displaced);
        uint64_t returnOffset = pt->getSourceAddress() - elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + repl[0]->getLength();
        PRINT_INFOR("Computing return address %llx - %llx + %d = %llx", pt->getSourceAddress(), elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr), repl[0]->getLength(), returnOffset);
        pt->generateTrampoline(numberOfDisplacedInstructions,displaced,codeOffset,returnOffset);
        codeOffset += pt->sizeNeeded();

        delete[] repl;
    }
    


    ASSERT(precodeOffset == bootstrapSize && "Bootstrap code size does not match what we just calculated");
    ASSERT(codeOffset <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough space in the text section to accomodate the extra code");
    ASSERT(dataOffset <= elfFile->getSectionHeader(extraDataIdx)->GET(sh_size) && "Not enough space in the data section to accomodate the extra data");    

    uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);
    uint64_t dataBaseAddress = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr);

    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        ASSERT(instrumentations[i] && "Instrumentations should be initialized by now");
        if (instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
            InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];

            func->generateGlobalData(textBaseAddress);
            func->generateWrapperInstructions(textBaseAddress);
            func->generateBootstrapInstructions(textBaseAddress,dataBaseAddress);
            func->generateProcedureLinkInstructions(textBaseAddress,dataBaseAddress,pltSection->getAddress());
            func->print();
        }
    }

    /*
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
    */

}

InstrumentationFunction* ElfFileInst::getInstrumentationFunction(const char* funcName){
    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        if (instrumentations[i] && instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
            InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];
            if (!strcmp(func->getFunctionName(),funcName)){
                return func;
            }
        }
    }
    return NULL;
}


// the order of the operations in this function matters
void ElfFileInst::instrument(){
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    ASSERT(numberOfInstrumentations > 1);

    uint16_t textIdx = elfFile->findSectionIdx(".text");
    ASSERT(textIdx && "Cannot find the text section");
    TextSection* text = (TextSection*)elfFile->getRawSection(textIdx);
    ASSERT(text && text->getType() == ElfClassTypes_TextSection && "Cannot find the text section");
    
    for (uint32_t i = 0; i < text->getNumberOfFunctions(); i++){
        if (strcmp(text->getFunction(i)->getFunctionName(),"_start")){
            PRINT_INFOR("Found non-start function: %s", text->getFunction(i)->getFunctionName());
            addInstrumentationPoint(text->getFunction(i),getInstrumentationFunction("smalltest"));
        } 
    }

    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_reserve_space && "Instrumentation phase order must be observed");

    PRINT_INFOR("Beginning Instrumentation of ELF file");

    uint32_t totalText = 0;
    uint32_t totalData = 0;
    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        if (instrumentations[i]){ 
            if (instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
                InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];
                totalText += func->procedureLinkSize() + func->bootstrapSize() + func->wrapperSize();
                totalData += func->globalDataSize();
            } else if (instrumentations[i]->getType() == ElfClassTypes_InstrumentationSnippet){
                InstrumentationSnippet* snip = (InstrumentationSnippet*)instrumentations[i];
                totalText += snip->sizeNeeded();
            }
        }
    }

    extendTextSection(0x1000);
    PRINT_INFOR("Successfully extended text section/segment");
    extendDataSection(0x100);
    PRINT_INFOR("Successfully extended data section/segment");

    ASSERT(currentPhase == ElfInstPhase_reserve_space && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < numberOfInstrumentationLibraries; i++){
        addSharedLibrary(instrumentationLibraries[i]);
    }

    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        if (instrumentations[i]){ 
            if (instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
                addFunction((InstrumentationFunction*)instrumentations[i]);
            }
        }
    }

    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    generateInstrumentation();

    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_dump_file && "Instrumentation phase order must be observed");
}




void ElfFileInst::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(currentPhase == ElfInstPhase_dump_file && "Instrumentation phase order must be observed");
    ASSERT(offset == ELF_FILE_HEADER_OFFSET && "Instrumentation must be dumped at the begining of the output file");

    uint32_t extraTextOffset = elfFile->getSectionHeader(extraTextIdx)->GET(sh_offset);
    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        instrumentations[i]->dump(binaryOutputFile,extraTextOffset);
    }
    for (uint32_t i = 0; i < numberOfInstrumentationPoints; i++){
        instrumentationPoints[i]->dump(binaryOutputFile,extraTextOffset);
    }

}

void ElfFileInst::verify(){
    elfFile->verify();
}

uint64_t ElfFileInst::addInstrumentationPoint(Instruction* instpoint, Instrumentation* inst){
    return addInstrumentationPoint((Base*)instpoint, inst);
}
uint64_t ElfFileInst::addInstrumentationPoint(TextSection* instpoint, Instrumentation* inst){
    return addInstrumentationPoint((Base*)instpoint, inst);
}
uint64_t ElfFileInst::addInstrumentationPoint(Function* instpoint, Instrumentation* inst){
    return addInstrumentationPoint((Base*)instpoint, inst);
}

uint64_t ElfFileInst::addInstrumentationPoint(Base* instpoint, Instrumentation* inst){
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");    

    if (instpoint->getType() != ElfClassTypes_Instruction &&
        instpoint->getType() != ElfClassTypes_TextSection &&
        instpoint->getType() != ElfClassTypes_Function){
        PRINT_ERROR("Cannot use an object of type %d as an instrumentation point", instpoint->getType());
    }

    InstrumentationPoint** newPoints = new InstrumentationPoint*[numberOfInstrumentationPoints+1];
    for (uint32_t i = 0; i < numberOfInstrumentationPoints; i++){
        newPoints[i] = instrumentationPoints[i];
    }
    newPoints[numberOfInstrumentationPoints] = new InstrumentationPoint(numberOfInstrumentationPoints, instpoint, inst);
    newPoints[numberOfInstrumentationPoints]->print();

    delete[] instrumentationPoints;
    instrumentationPoints = newPoints;
    numberOfInstrumentationPoints++;

    return numberOfInstrumentationPoints;
}

uint32_t ElfFileInst::declareFunction(char* funcName){
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        if (instrumentations[i] && instrumentations[i]->getType() == ElfClassTypes_InstrumentationFunction){
            InstrumentationFunction* func = (InstrumentationFunction*)instrumentations[i];
            if (!strcmp(funcName,func->getFunctionName())){
                PRINT_ERROR("Trying to add a function that was already added -- %s", funcName);
                return numberOfInstrumentations;
            }
        }
    }

    Instrumentation** newFunctions = new Instrumentation*[numberOfInstrumentations+1];
    for (uint32_t i = 0; i < numberOfInstrumentations; i++){
        newFunctions[i] = instrumentations[i];
    }
    newFunctions[numberOfInstrumentations] = new InstrumentationFunction32(numberOfInstrumentations, funcName);

    delete[] instrumentations;
    instrumentations = newFunctions;
    numberOfInstrumentations++;

    return numberOfInstrumentations;
}

uint32_t ElfFileInst::declareLibrary(char* libName){
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < numberOfInstrumentationLibraries; i++){
        if (!strcmp(libName,instrumentationLibraries[i])){
            PRINT_ERROR("Trying to add a library that was already added -- %s", libName);
            return numberOfInstrumentationLibraries;
        }
    }

    char** newLibraries = new char*[numberOfInstrumentationLibraries+1];
    for (uint32_t i = 0; i < numberOfInstrumentationLibraries; i++){
        newLibraries[i] = instrumentationLibraries[i];
    }
    newLibraries[numberOfInstrumentationLibraries] = new char[strlen(libName)+1];
    strcpy(newLibraries[numberOfInstrumentationLibraries],libName);

    delete[] instrumentationLibraries;
    instrumentationLibraries = newLibraries;
    numberOfInstrumentationLibraries++;

    return numberOfInstrumentationLibraries;
}


uint64_t ElfFileInst::addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    DynamicTable* dynTable = elfFile->getDynamicTable();

    ASSERT(dynTable->countDynamics(DT_JMPREL) == 1 && "Cannot find a unique Relocation table for this file");
    
    uint64_t relocTableAddr = dynTable->getDynamicByType(DT_JMPREL,0)->GET_A(d_val,d_un);
    ASSERT(relocTableAddr && "Count not find a relocation table address in the dynamic table");

    RelocationTable* relocTable = (RelocationTable*)elfFile->getRawSection(elfFile->findSectionIdx(relocTableAddr));
    ASSERT(relocTable->getType() == ElfClassTypes_RelocationTable && "Found wrong section type when searching for relocation table");

    PRINT_INFOR("Adding PLT entry to relocation table at section %hd", relocTable->getSectionIndex());

    uint64_t gotAddress = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + gotOffset;    
    uint64_t relocOffset;
    if (elfFile->is64Bit()){
        relocOffset = relocTable->addRelocation(gotAddress,ELF64_R_INFO(symbolIndex,R_X86_64_JUMP_SLOT));
    } else {
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
    
    verify();

    return relocOffset;
}


void ElfFileInst::extendDataSection(uint64_t size){
    ASSERT(currentPhase == ElfInstPhase_reserve_space && "Instrumentation phase order must be observed");

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
    bssSection->INCREMENT(sh_size,size);

    // increase the memory size of the data segment
    dataHeader->INCREMENT(p_memsz,size);

    elfFile->addSection(extraDataIdx, ElfClassTypes_no_type, NULL, bssSection->GET(sh_name), SHT_PROGBITS,
                        bssSection->GET(sh_flags), bssSection->GET(sh_addr) + bssSection->GET(sh_size), 
                        bssSection->GET(sh_offset), size, bssSection->GET(sh_link), 
                        bssSection->GET(sh_info), 0, bssSection->GET(sh_entsize));

    // move all later sections' offsets so that they don't conflict with this one
    for (uint32_t i = bssSectionIdx+2; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* scn = elfFile->getSectionHeader(i);
        scn->SET(sh_offset,nextAlignAddress(scn->GET(sh_offset) + size, scn->GET(sh_addralign)));        
    }

    // move the section header table offset if it is after the data section
    if (extendedData->GET(sh_offset) < elfFile->getFileHeader()->GET(e_shoff)){
        elfFile->getFileHeader()->INCREMENT(e_shoff,size);
    }

    PRINT_INFOR("Extra data space available @address 0x%016llx + %d bytes", extendedData->GET(sh_addr), extendedData->GET(sh_size));
    PRINT_INFOR("Extra data space available @offset  0x%016llx + %d bytes", extendedData->GET(sh_offset), extendedData->GET(sh_size));

    verify();
}


void ElfFileInst::extendTextSection(uint64_t size){
    ASSERT(currentPhase == ElfInstPhase_reserve_space && "Instrumentation phase order must be observed");

    PRINT_INFOR("Attempting to extend text segment by %llx ALIGN %llx = %llx", size, elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align), nextAlignAddress(size,elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align)));
    size = nextAlignAddress(size,elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align));
    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ASSERT(!extraTextIdx && "Cannot extend the text segment more than once");

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


    verify();

    PRINT_INFOR("Extra text space available @address 0x%016llx + %d bytes", extendedText->GET(sh_addr), extendedText->GET(sh_size));
    PRINT_INFOR("Extra text space available @offset  0x%016llx + %d bytes", extendedText->GET(sh_offset), extendedText->GET(sh_size));
}


ElfFileInst::~ElfFileInst(){
    if (instrumentations){
        for (uint32_t i = 0; i < numberOfInstrumentations; i++){
            delete instrumentations[i];
        }
        delete[] instrumentations;
    }

    if (instrumentationPoints){
        for (uint32_t i = 0; i < numberOfInstrumentationPoints; i++){
            delete instrumentationPoints[i];
        }
        delete[] instrumentationPoints;
    }

    if (instrumentationLibraries){
        for (uint32_t i = 0; i < numberOfInstrumentationLibraries; i++){
            delete[] instrumentationLibraries[i];
        }
        delete[] instrumentationLibraries;
    }
}

void ElfFileInst::dump(char* extension){
    ASSERT(currentPhase == ElfInstPhase_dump_file && "Instrumentation phase order must be observed");

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

void ElfFileInst::print(){
    elfFile->print();
    PRINT_INFOR("");
    PRINT_INFOR("Instrumentation Reservations:");
    if (extraTextIdx){
        SectionHeader* extendedText = elfFile->getSectionHeader(extraTextIdx);
        PRINT_INFOR("Extended TEXT section is section %hd", extraTextIdx);
        PRINT_INFOR("\tExtra text space available @address 0x%016llx + %d bytes", extendedText->GET(sh_addr), extendedText->GET(sh_size));
        PRINT_INFOR("\tExtra text space available @offset  0x%016llx + %d bytes", extendedText->GET(sh_offset), extendedText->GET(sh_size));
    }
    if (extraDataIdx){
        SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);
        PRINT_INFOR("Extended DATA section is section %hd", extraDataIdx);
        PRINT_INFOR("Extra data space available @address 0x%016llx + %d bytes", extendedData->GET(sh_addr), extendedData->GET(sh_size));
        PRINT_INFOR("Extra data space available @offset  0x%016llx + %d bytes", extendedData->GET(sh_offset), extendedData->GET(sh_size));
    }
}


ElfFileInst::ElfFileInst(ElfFile* elf){
    currentPhase = ElfInstPhase_no_phase;
    elfFile = elf;

    numberOfInstrumentations = INST_CODES_RESERVED;
    instrumentations = new Instrumentation*[INST_CODES_RESERVED];
    instrumentations[IDX_INST_BOOTSTRAP_BEGIN] = NULL;
    instrumentations[IDX_INST_BOOTSTRAP_END] = NULL;

    // automatically set the 1st instrumentation point to go to the bootstrap code
    numberOfInstrumentationPoints = INST_POINTS_RESERVED;
    instrumentationPoints = new InstrumentationPoint*[INST_POINTS_RESERVED];
    instrumentationPoints[IDX_POINT_BOOTSTRAP] = NULL;

    numberOfInstrumentationLibraries = 0;
    instrumentationLibraries = NULL;

    extraTextIdx = 0;
    extraDataIdx = 0;
}

uint32_t ElfFileInst::addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t entrySize;
    if (elfFile->is64Bit()){
        entrySize = Size__64_bit_Symbol;
    } else {
        entrySize = Size__32_bit_Symbol;
    }

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
    ASSERT(extraSize <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough room to insert extra ELF control");
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
    ASSERT(extraSize <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough room to insert extra ELF control");
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
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

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
    ASSERT(extraSize <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough room to insert extra ELF control");
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
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strSize = strlen(str) + 1;

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
    ASSERT(extraSize <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough room to insert extra ELF control");
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);

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

uint64_t ElfFileInst::addFunction(InstrumentationFunction* func){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    uint32_t funcNameOffset = addStringToDynamicStringTable(func->getFunctionName());
    uint32_t symbolIndex = addSymbolToDynamicSymbolTable(funcNameOffset, 0, 0, STB_GLOBAL, STT_FUNC, 0, 0);

    uint64_t relocationOffset = addPLTRelocationEntry(symbolIndex,func->getGlobalDataOffset());
    func->setRelocationOffset(relocationOffset);
    PRINT_INFOR("Successfully added PLT relocation entries");

    verify();

    return relocationOffset;
}


uint32_t ElfFileInst::addSharedLibrary(const char* libname){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strOffset = addStringToDynamicStringTable(libname);

    // add a DT_NEEDED entry to the dynamic table
    uint32_t emptyDynamicIdx = dynamicTable->findEmptyDynamic();

    ASSERT(emptyDynamicIdx < dynamicTable->getNumberOfDynamics() && "No free entries found in the dynamic table");

    dynamicTable->getDynamic(emptyDynamicIdx)->SET(d_tag,DT_NEEDED);
    dynamicTable->getDynamic(emptyDynamicIdx)->SET_A(d_ptr,d_un,strOffset);

    PRINT_INFOR("Verifying elf file after adding shared library");

    verify();

    return strOffset;
}


uint64_t ElfFileInst::relocateDynamicSection(){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    
    __SHOULD_NOT_ARRIVE;

    verify();
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
