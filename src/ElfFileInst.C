#include <ElfFileInst.h>

#include <AddressAnchor.h>
#include <Base.h>
#include <CStructuresX86.h>
#include <Disassembler.h>
#include <DynamicTable.h>
#include <ElfFile.h>
#include <FileHeader.h>
#include <Function.h>
#include <GnuVerneedTable.h>
#include <GnuVersymTable.h>
#include <HashTable.h>
#include <Instruction.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <ProgramHeader.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <TextSection.h>

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

// some common macros you can use to help debug
//#define TURNOFF_FUNCTION_RELOCATION
//#define TURNOFF_INSTRUCTION_SWAP
#define ANCHOR_SEARCH_BINARY

uint32_t ElfFileInst::initializeReservedData(uint64_t address, uint32_t size, void* data){
    InstrumentationSnippet* snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END];
    uint8_t* bytes = (uint8_t*)data;

    bool setLoc = false;
    for (uint32_t i = 0; i < size; i++){
        uint8_t d = bytes[i];
        if (d){
            snip->addSnippetInstruction(Instruction::generateMoveImmByteToReg(d,X86_REG_AX));
            /*
            if (i == 0){
                snip->addSnippetInstruction(Instruction::generateMoveImmToReg(address+i,X86_REG_DI));
            } else {
                snip->addSnippetInstruction(Instruction::generateRegIncrement(X86_REG_DI));
            }
            */
            snip->addSnippetInstruction(Instruction::generateMoveImmToReg(address+i,X86_REG_DI));
            snip->addSnippetInstruction(Instruction::generateSTOSByte(false));
        }
    }

    return size;
}

bool ElfFileInst::isEligibleFunction(Function* func){
    if (!canRelocateFunction(func)){
        return false;
    }
    return true;

    char* name = func->getName();
    if (strstr(name,"intel")
        || strstr(name,"pgi")
        || strstr(name,"pgCC")
        || strstr(name,"hpf")
        || strstr(name,"fmth")
        ){
        return false;
    }

    return true;
}

Vector<AddressAnchor*>* ElfFileInst::searchAddressAnchors(uint64_t addr){
    Vector<AddressAnchor*>* needToUpdate = new Vector<AddressAnchor*>();

#ifdef ANCHOR_SEARCH_BINARY
    if (!anchorsAreSorted){
        addressAnchors.sort(compareLinkBaseAddress);
        anchorsAreSorted = true;
    }
    //    qsort(allAnchors, addressAnchors.size(), sizeof(AddressAnchor*), compareLinkBaseAddress);
    AddressAnchor** allAnchors = &addressAnchors;
#ifdef DEBUG_ANCHOR
    PRINT_INFOR("Array is:");
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        PRINT_INFOR("%#llx", allAnchors[i]->linkBaseAddress);
    }
#endif
    void* anchor = bsearch(&addr, allAnchors, addressAnchors.size(), sizeof(AddressAnchor*), searchLinkBaseAddress);
    if (anchor){
        uint32_t idx = ((uint32_t)anchor-(uint32_t)allAnchors)/sizeof(AddressAnchor*);
        while (idx < addressAnchors.size() &&
                addressAnchors[idx]->linkBaseAddress <= addr &&
                addr < addressAnchors[idx]->linkBaseAddress + addressAnchors[idx]->getLink()->getSizeInBytes()){
            (*needToUpdate).append(allAnchors[idx++]);
        }
    }

#ifdef DEBUG_ANCHOR
    PRINT_INFOR("found %#llx %d %d", addr, needToUpdate->size(), (*needToUpdate).size());
    Vector<AddressAnchor*>* needToUpdate2 = new Vector<AddressAnchor*>();
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        if (addressAnchors[i]->linkBaseAddress <= addr &&
            addr < addressAnchors[i]->linkBaseAddress + addressAnchors[i]->getLink()->getSizeInBytes()){
            needToUpdate2->append(addressAnchors[i]);
        }
    }
    if ((*needToUpdate).size() != (*needToUpdate2).size()){
        PRINT_INFOR("Binary search yields:");
        for (uint32_t i = 0; i < (*needToUpdate).size(); i++){
            PRINT_INFOR("%#llx", (*needToUpdate)[i]->linkBaseAddress);
        }
        PRINT_INFOR("Linear search yields:");
        for (uint32_t i = 0; i < (*needToUpdate2).size(); i++){
            PRINT_INFOR("%#llx", (*needToUpdate2)[i]->linkBaseAddress);
        }
        PRINT_INFOR("Array is:");
        for (uint32_t i = 0; i < addressAnchors.size(); i++){
            PRINT_INFOR("%#llx", allAnchors[i]->linkBaseAddress);
        }
        
    }
    ASSERT((*needToUpdate).size() == (*needToUpdate2).size());
    delete needToUpdate2;
#endif
#else
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        if (addressAnchors[i]->linkBaseAddress <= addr &&
            addr < addressAnchors[i]->linkBaseAddress + addressAnchors[i]->getLink()->getSizeInBytes()){
            needToUpdate->append(addressAnchors[i]);
        }
    }
#endif
    return needToUpdate;
}


uint32_t ElfFileInst::anchorProgramElements(){
    uint32_t anchorCount = 0;
    uint32_t instructionCount = 0;
    PRINT_DEBUG_ANCHOR("Found %d text sections", elfFile->getNumberOfTextSections());
    for (uint32_t i = 0; i < elfFile->getNumberOfTextSections(); i++){
        instructionCount += elfFile->getTextSection(i)->getNumberOfInstructions();
        PRINT_DEBUG_ANCHOR("\tTextSection %d is section %d with %d instructions", i, elfFile->getTextSection(i)->getSectionIndex(), elfFile->getTextSection(i)->getNumberOfInstructions());
    }
    PRINT_DEBUG_ANCHOR("Found %d instructions in all sections", instructionCount);

    Instruction** allInstructions = new Instruction*[instructionCount];
    instructionCount = 0;
    PRINT_DEBUG_ANCHOR("allinst address %lx", allInstructions);
    for (uint32_t i = 0; i < elfFile->getNumberOfTextSections(); i++){
        instructionCount += elfFile->getTextSection(i)->getAllInstructions(allInstructions, instructionCount);
    }

    qsort(allInstructions, instructionCount, sizeof(Instruction*), compareBaseAddress);

    for (uint32_t i = 0; i < instructionCount; i++){
        if (!allInstructions[i]->getBaseAddress()){
            allInstructions[i]->print();
            __SHOULD_NOT_ARRIVE;
        }
    }

#ifdef DEBUG_ANCHOR
    for (uint32_t i = 0; i < instructionCount-1; i++){
        if (allInstructions[i+1]->getBaseAddress() <= allInstructions[i]->getBaseAddress()){
            allInstructions[i]->print();
            allInstructions[i+1]->print();
        }
        ASSERT(allInstructions[i]->getBaseAddress() < allInstructions[i+1]->getBaseAddress() && "Problem with qsort");
    }
#endif

    uint32_t addrAlign;
    if (elfFile->is64Bit()){
        addrAlign = sizeof(uint64_t);
    } else {
        addrAlign = sizeof(uint32_t);
    }

    for (uint32_t i = 0; i < instructionCount; i++){
        Instruction* currentInstruction = allInstructions[i];
        ASSERT(!currentInstruction->getAddressAnchor());
        if (currentInstruction->usesRelativeAddress()){
            uint64_t relativeAddress = currentInstruction->getRelativeValue() + currentInstruction->getBaseAddress();
            if (!currentInstruction->isControl() || currentInstruction->usesIndirectAddress()){
                relativeAddress += currentInstruction->getSizeInBytes();
            }
            if (!currentInstruction->getRelativeValue()){
                PRINT_WARN(4,"An Instruction links to null address %#llx", currentInstruction->getRelativeValue());
            }
            PRINT_DEBUG_ANCHOR("Searching for relative address %llx", relativeAddress);

            // search other instructions
            void* link = bsearch(&relativeAddress,allInstructions,instructionCount,sizeof(Instruction*),searchBaseAddressExact);
            if (link != NULL){
                Instruction* linkedInstruction = *(Instruction**)link;
                PRINT_DEBUG_ANCHOR("Found inst -> inst link: %#llx -> %#llx", currentInstruction->getBaseAddress(), relativeAddress);
                currentInstruction->initializeAnchor(linkedInstruction);
                addressAnchors.append(currentInstruction->getAddressAnchor());
                currentInstruction->getAddressAnchor()->setIndex(anchorCount);
                anchorCount++;
            }

            // search special data references
            if (!currentInstruction->getAddressAnchor()){
                for (uint32_t i = 0; i < specialDataRefs.size(); i++){
                    if (specialDataRefs[i]->getBaseAddress() == relativeAddress){                        
                        PRINT_DEBUG_ANCHOR("Found inst -> sdata link: %#llx -> %#llx", currentInstruction->getBaseAddress(), relativeAddress);
                        currentInstruction->initializeAnchor(specialDataRefs[i]);
                        addressAnchors.append(currentInstruction->getAddressAnchor());
                        currentInstruction->getAddressAnchor()->setIndex(anchorCount);
                        anchorCount++;
                    }
                }
            }

            // search non-text sections
            if (!currentInstruction->getAddressAnchor()){
                for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
                    RawSection* dataRawSection = elfFile->getRawSection(i);
                    SectionHeader* dataSectionHeader = elfFile->getSectionHeader(i);

                    PRINT_DEBUG_ANCHOR("Checking section %d for inst->data link: [%#llx,%#llx)", i, dataSectionHeader->GET(sh_addr), dataSectionHeader->GET(sh_addr) + dataRawSection->getSizeInBytes());
                    if (dataRawSection->getType() != ElfClassTypes_TextSection){
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
                                    memcpy(&currentData,dataRawSection->charStream()+sectionOffset,addrAlign);
                                    extendedData = currentData;
                                } else if (addrAlign == sizeof(uint32_t)){
                                    uint32_t currentData;
                                    memcpy(&currentData,dataRawSection->charStream()+sectionOffset,addrAlign);
                                    extendedData = (uint64_t)currentData;
                                } else {
                                    __SHOULD_NOT_ARRIVE;
                                }
                            }
                            DataReference* dataRef = new DataReference(extendedData,dataRawSection,elfFile->is64Bit(),sectionOffset);
                            currentInstruction->initializeAnchor(dataRef);
                            addressAnchors.append(currentInstruction->getAddressAnchor());
                            currentInstruction->getAddressAnchor()->setIndex(anchorCount);
                            dataRawSection->addDataReference(dataRef);
                            anchorCount++;
                        }
                    }
                }
            }
            if (!currentInstruction->getAddressAnchor()){
                PRINT_WARN(4, "Creating special AddressRelocation for %#llx at the behest of the instruction at %#llx since it wasn't an instruction or part of a data section", 
                           relativeAddress, currentInstruction->getBaseAddress()); 
                DataReference* dataRef = new DataReference(0,NULL,elfFile->is64Bit(),relativeAddress);
                currentInstruction->initializeAnchor(dataRef);
                specialDataRefs.append(dataRef);
            }

#if WARNING_SEVERITY <= 4
            if (!currentInstruction->getAddressAnchor()){
                PRINT_WARN(4,"Unable to link the instruction at %#llx (-> %#llx) to another object", currentInstruction->getBaseAddress(), relativeAddress);
                currentInstruction->print();
            }
#endif
        }
    }

    ASSERT(anchorCount == addressAnchors.size());

    PRINT_DEBUG_ANCHOR("Found %d instructions that required anchoring", anchorCount);
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");

    // find the data sections
    Vector<uint16_t> dataSections;
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->getSectionNamePtr()){
            if (strstr(elfFile->getSectionHeader(i)->getSectionNamePtr(),"data")){
                PRINT_DEBUG_ANCHOR("Found data section at %d", i);
                dataSections.append((uint16_t)i);
            }
        }
    }

    for (uint32_t i = 0; i < dataSections.size(); i++){
        RawSection* dataRawSection = elfFile->getRawSection(dataSections[i]);
        SectionHeader* dataSectionHeader = elfFile->getSectionHeader(dataSections[i]);

        // since there are no constraints on the alignment of stuff in the data sections we must check starting at EVERY byte
        for (uint32_t currByte = 0; currByte < dataRawSection->getSizeInBytes() - addrAlign; currByte++){
            char* dataPtr = (char*)(dataRawSection->charStream()+currByte);
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
            PRINT_DEBUG_ANCHOR("data section %d(%x): extendedData is %#llx", i, dataPtr, extendedData);

            void* link = bsearch(&extendedData,allInstructions,instructionCount,sizeof(Instruction*),searchBaseAddressExact);
            if (link != NULL){
                Instruction* linkedInstruction = *(Instruction**)link;
                PRINT_DEBUG_ANCHOR("Found data -> inst link: %#llx", dataSectionHeader->GET(sh_addr)+currByte, extendedData);
                DataReference* dataRef = new DataReference(extendedData,dataRawSection,elfFile->is64Bit(),currByte);
                dataRef->initializeAnchor(linkedInstruction);
                dataRawSection->addDataReference(dataRef);

                anchorCount++;    
            } else {
                //PRINT_DEBUG_ANCHOR("found data %#llx not linked to an instruction", extendedData);
            }
        }
    }
    
    PRINT_DEBUG_ANCHOR("Found %d anchors total", anchorCount);
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");
    PRINT_DEBUG_ANCHOR("----------------------------------------------------------");

#ifdef DEBUG_ANCHOR
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        addressAnchors[i]->print();
    }
#endif
    
    delete[] allInstructions;
    return anchorCount;
}


uint32_t ElfFileInst::relocateFunction(Function* functionToRelocate, uint64_t offsetToRelocation){
    ASSERT(isEligibleFunction(functionToRelocate) && functionToRelocate->hasCompleteDisassembly());

    TextSection* extraText = (TextSection*)elfFile->getRawSection(extraTextIdx);
    TextSection* text = functionToRelocate->getTextSection();
    uint64_t relocationAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + offsetToRelocation;
    uint32_t functionSize = functionToRelocate->getNumberOfBytes();
    Vector<Instruction*> trampEmpty;
    
    uint32_t currentByte = 0;

    trampEmpty.append(Instruction::generateJumpRelative(functionToRelocate->getBaseAddress(), relocationAddress));
    currentByte += trampEmpty.back()->getSizeInBytes();
    trampEmpty.back()->setBaseAddress(functionToRelocate->getBaseAddress());
    
    if (currentByte > functionToRelocate->getNumberOfBytes()){
        PRINT_WARN(5, "Function %s at address %#llx is only %d bytes -- cannot relocate or fully instrument", functionToRelocate->getName(), functionToRelocate->getBaseAddress(), functionToRelocate->getNumberOfBytes());
        for (uint32_t i = 0; i < trampEmpty.size(); i++){
            delete trampEmpty[i];
        }
        return functionToRelocate->getNumberOfBytes();
    }
    ASSERT(currentByte <= functionToRelocate->getNumberOfBytes() && "Function is not big enough to relocate");

    Function* placeHolder = new Function(text,functionToRelocate->getIndex(),functionToRelocate->getFunctionSymbol(),functionSize);
    Vector<AddressAnchor*>* modAnchors = searchAddressAnchors(functionToRelocate->getBaseAddress());
    for (uint32_t i = 0; i < modAnchors->size(); i++){
        (*modAnchors)[i]->updateLink(trampEmpty.back());
        anchorsAreSorted = false;
    }
    delete modAnchors;

    while (currentByte < functionSize){
        trampEmpty.append(Instruction::generateNoop());
        trampEmpty.back()->setBaseAddress(functionToRelocate->getBaseAddress()+currentByte);
        currentByte += trampEmpty.back()->getSizeInBytes();
    }

    placeHolder->generateCFG(trampEmpty.size(),&trampEmpty);
    placeHolder = text->replaceFunction(functionToRelocate->getIndex(),placeHolder);
    relocatedFunctions.append(placeHolder);
    relocatedFunctionOffsets.append(offsetToRelocation);
    ASSERT(relocatedFunctions.size() == relocatedFunctionOffsets.size());

    uint64_t oldBase = placeHolder->getBaseAddress();
    uint64_t oldSize = placeHolder->getSizeInBytes();

    // adjust the base addresses of the relocated function
    placeHolder->setBaseAddress(relocationAddress);

    // bloat the blocks in the function
    placeHolder->bloatBasicBlocks(SIZE_CONTROL_TRANSFER);
    PRINT_DEBUG_FUNC_RELOC("Function %s relocation map [%#llx,%#llx) --> [%#llx,%#llx)", placeHolder->getName(), oldBase, oldBase+oldSize, placeHolder->getBaseAddress(), placeHolder->getBaseAddress() + placeHolder->getSizeInBytes());

    return placeHolder->getNumberOfBytes();
}

// the order of operations in this function in very important, things will break in
// very insidious ways if the order is changed
void ElfFileInst::generateInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    anchorProgramElements();

    uint16_t textIdx = elfFile->findSectionIdx(".text");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    SectionHeader* textHeader = elfFile->getSectionHeader(textIdx);
    ASSERT(textSection->getType() == ElfClassTypes_TextSection && ".text section has the wrong type");

    uint16_t pltIdx = elfFile->findSectionIdx(".plt");
    TextSection* pltSection = (TextSection*)elfFile->getRawSection(pltIdx);
    ASSERT(pltSection->getType() == ElfClassTypes_TextSection && ".plt section has the wrong type");

    uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);
    uint64_t dataBaseAddress = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr);

    InstrumentationSnippet* snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN];
    snip->addSnippetInstruction(Instruction::generatePushEflags());
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        snip->addSnippetInstruction(Instruction32::generateStackPush(i));
    }

    ASSERT(!instrumentationPoints[INST_POINT_BOOTSTRAP1] && "instrumentationPoint[INST_POINT_BOOTSTRAP1] is reserved");
    for (uint32_t i = 0; i < textSection->getNumberOfTextObjects(); i++){
        if (!strcmp(textSection->getTextObject(i)->getName(),"_start")){
            ASSERT(!instrumentationPoints[INST_POINT_BOOTSTRAP1] && "Found more than one start function");
            instrumentationPoints[INST_POINT_BOOTSTRAP1] = 
                new InstrumentationPoint((Base*)textSection->getTextObject(i), instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN], SIZE_FIRST_INST_POINT, InstLocation_dont_care);
        }
    }

    uint64_t codeOffset = 0;

    uint32_t numberOfTextObjects = textSection->getNumberOfTextObjects();
#ifdef TURNOFF_FUNCTION_RELOCATION
    numberOfTextObjects = 0;
#endif
    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        if (textSection->getTextObject(i)->isFunction()){
            Function* func = (Function*)textSection->getTextObject(i);
            if (isEligibleFunction(func)){
                if (func->hasCompleteDisassembly()){
                    codeOffset += relocateFunction(func,codeOffset);
                } else {
                    PRINT_WARN(6, "Function %s is not eligible for relocation", func->getName());
                }
            } else {
                PRINT_WARN(5, "Function %s is not eligible for relocation (see ElfFileInst::isEligibleFunction)", func->getName());
            }
        }
    }

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];

        if (func){
            PRINT_DEBUG_INST("Setting InstrumentationFunction %d PLT offset to %#llx", i, codeOffset);
            func->setProcedureLinkOffset(codeOffset);
            codeOffset += func->procedureLinkReservedSize();
            
            PRINT_DEBUG_INST("Setting InstrumentationFunction %d Wrapper offset to %#llx", i, codeOffset);
            func->setWrapperOffset(codeOffset);
            codeOffset += func->wrapperReservedSize();
#ifdef DEBUG_INST
            func->print();
#endif
        }
    }

    for (uint32_t i = INST_SNIPPET_BOOTSTRAP_END + 1; i < instrumentationSnippets.size(); i++){        
        snip = instrumentationSnippets[i];
        if (snip){
            snip->generateSnippetControl();

            snip->setCodeOffset(codeOffset);
            codeOffset += snip->snippetSize();
        }
    }

    snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN];
    snip->setCodeOffset(codeOffset);
    uint64_t beginBootstrapOffset = codeOffset;
    // leave room for a jump to be added on
    codeOffset += snip->snippetSize() + SIZE_CONTROL_TRANSFER;

    for (uint32_t i = 0; i < instrumentationPoints.size(); i++){
        InstrumentationPoint* pt = instrumentationPoints[i];
        if (!pt){
            PRINT_ERROR("Instrumentation point %d should exist", i);
        }
#ifdef TURNOFF_INSTRUCTION_SWAP
        break;
#endif

        if (!pt->getSourceAddress()){
            PRINT_WARN(4,"Could not find a place to instrument for point at %#llx", pt->getSourceObject()->getBaseAddress());
            continue;
        }
        if (!pt->getSourceObject()->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
            PRINT_WARN(4,"Could not find a place to instrument for point at %#llx", pt->getSourceObject()->getBaseAddress());
            continue;
        }
        PRINT_DEBUG_INST("Generating code for InstrumentationPoint %d at address %llx", i, pt->getSourceAddress());

        Vector<Instruction*>* repl = new Vector<Instruction*>();
        
        if (instrumentationPoints[i]->getNumberOfBytes() == SIZE_CONTROL_TRANSFER){
            (*repl).append(Instruction::generateJumpRelative(pt->getSourceAddress(), elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + codeOffset));
        } else {
            PRINT_ERROR("This size instrumentation point (%d) not supported", instrumentationPoints[i]->getNumberOfBytes());
        }

        // disassemble the newly minted instructions
        uint32_t bytesUsed = 0;
        for (uint32_t j = 0; j < (*repl).size(); j++){
            (*repl)[j]->setBaseAddress(pt->getSourceAddress()+bytesUsed);
            bytesUsed += (*repl)[j]->getSizeInBytes();
            Base::disassembler->disassembleInstructionInPlace((*repl)[j]);
#ifdef DEBUG_INST
            (*repl)[j]->print();
#endif
        }

        ASSERT((*repl).back()->getSizeInBytes() == SIZE_NEEDED_AT_INST_POINT ||
               (*repl).back()->getSizeInBytes() == SIZE_FIRST_INST_POINT && "Instruction at instrumentation point has a different size than expected");

        Vector<Instruction*>* displaced = pt->swapInstructionsAtPoint(repl);

        // update any address anchor that pointed to the old instruction to point to the new
        ASSERT((*repl).size());
        ASSERT((*displaced).size());
        for (uint32_t j = 0; j < (*displaced).size(); j++){
            Vector<AddressAnchor*>* modAnchors = searchAddressAnchors((*displaced)[j]->getBaseAddress());
            for (uint32_t k = 0; k < modAnchors->size(); k++){
                PRINT_DEBUG_ANCHOR("Instruction swapping at address %#llx because of anchor/swap", (*displaced)[j]->getBaseAddress());
#ifdef DEBUG_ANCHOR
                (*modAnchors)[k]->print();
#endif
                for (uint32_t l = 0; l < (*repl).size(); l++){
                    PRINT_DEBUG_ANCHOR("\t\t********Comparing addresses %#llx and %#llx", (*displaced)[j]->getBaseAddress(), (*repl)[l]->getBaseAddress());
                    if ((*displaced)[j]->getBaseAddress() == (*repl)[l]->getBaseAddress()){
                        (*modAnchors)[k]->updateLink((*repl)[l]);
                        anchorsAreSorted = false;
                    }
                }
            }
            delete modAnchors;
        }

        uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);
        uint64_t returnOffset = pt->getSourceAddress() - elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + (*repl)[0]->getSizeInBytes();
        
        pt->generateTrampoline(displaced,textBaseAddress,codeOffset,returnOffset,elfFile->is64Bit());

        codeOffset += pt->sizeNeeded();
        delete repl;
        delete displaced;
    }

    // we can compute this snippet after instrumentation points because no instpoint will jump to it
    uint64_t bootstrapJumpTarget = codeOffset;
    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];        
        
        if (func){
            func->setBootstrapOffset(codeOffset);
            codeOffset += func->bootstrapReservedSize();
        }
    }

    for (uint32_t i = INST_SNIPPET_BOOTSTRAP_END + 1; i < instrumentationSnippets.size(); i++){        
        snip = instrumentationSnippets[i];

        if (snip){
            snip->setBootstrapOffset(codeOffset);
            codeOffset += snip->bootstrapSize();
            ASSERT(snip->bootstrapSize() == 0 && "Snippets should not require bootstrap code (for now)");
        }
    }

    snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END];
    snip->setCodeOffset(codeOffset);

    InstrumentationSnippet* beginBootstrap = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN];
    Instruction* jumpToFinalBootstrap = Instruction::generateJumpRelative(beginBootstrap->getCodeOffset()+beginBootstrap->snippetSize(),bootstrapJumpTarget);
    beginBootstrap->addSnippetInstruction(jumpToFinalBootstrap);
    
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        snip->addSnippetInstruction(Instruction32::generateStackPop(X86_32BIT_GPRS-i-1));
    }
    snip->addSnippetInstruction(Instruction::generatePopEflags());
    snip->addSnippetInstruction(Instruction::generateReturn());

    codeOffset += snip->snippetSize();

    PRINT_INFOR("Space allocated for extra text: %lld bytes, space needed: %lld bytes", elfFile->getSectionHeader(extraTextIdx)->GET(sh_size), codeOffset);
    PRINT_INFOR("Space allocated for extra data: %lld bytes, space needed: %lld bytes", elfFile->getSectionHeader(extraDataIdx)->GET(sh_size), usableDataOffset);
    ASSERT(codeOffset <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough space in the text section to accomodate the extra code");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (func){
#ifdef DEBUG_INST
            func->print();
#endif
            func->generateGlobalData(textBaseAddress);
            func->generateWrapperInstructions(textBaseAddress,dataBaseAddress);
            func->generateBootstrapInstructions(textBaseAddress,dataBaseAddress);
            func->generateProcedureLinkInstructions(textBaseAddress,dataBaseAddress,pltSection->getBaseAddress());
        }
    }

#ifdef DEBUG_ANCHOR
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
            addressAnchors[i]->print();
    }
    PRINT_DEBUG_ANCHOR("Still have %d anchors", addressAnchors.size());
#endif

}


void ElfFileInst::setPathToInstLib(char* libPath){
    if (sharedLibraryPath){
        PRINT_WARN(4,"Overwriting shared library path");
        delete[] sharedLibraryPath;
    }
    sharedLibraryPath = new char[__MAX_STRING_SIZE];
    sprintf(sharedLibraryPath, "%s\0", libPath);
}

TextSection* ElfFileInst::getExtraTextSection() { return (TextSection*)(elfFile->getRawSection(extraTextIdx)); }
RawSection* ElfFileInst::getExtraDataSection() { return elfFile->getRawSection(extraDataIdx); }
uint64_t ElfFileInst::getExtraDataAddress() { return elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr); }

uint64_t ElfFileInst::reserveDataOffset(uint64_t size){
    ASSERT(currentPhase > ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    uint64_t avail = usableDataOffset + bssReserved;
    usableDataOffset += size;
    if (avail > elfFile->getSectionHeader(extraDataIdx)->GET(sh_size)){
        PRINT_WARN(5,"More than %llx bytes of data are needed for the extra data section", elfFile->getSectionHeader(extraDataIdx)->GET(sh_size));
    }
    ASSERT(avail <= elfFile->getSectionHeader(extraDataIdx)->GET(sh_size) && "Not enough space for the requested data");
    return avail;
}

void ElfFileInst::extendDataSection(uint64_t size){
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

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
    ASSERT(bssSectionIdx && "Could not find the BSS section in the file");
    SectionHeader* bssSection = elfFile->getSectionHeader(bssSectionIdx);
    ASSERT(!strcmp(bssSection->getSectionNamePtr(),".bss") && "BSS section named something other than `.bss'");

    extraDataIdx = bssSectionIdx;
    bssReserved = bssSection->GET(sh_size);


    // increase the memory size of the bss section (note: this section has no size in the file)
    bssSection->INCREMENT(sh_size,size);

    // increase the memory size of the data segment
    dataHeader->INCREMENT(p_memsz,size);

    SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);

    for (uint32_t i = extraDataIdx+1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* scn = elfFile->getSectionHeader(i);
        ASSERT(!scn->GET(sh_addr) && "The bss section should be the final section the programs address space");
    }

    verify();
}


uint32_t ElfFileInst::addInstrumentationSnippet(InstrumentationSnippet* snip){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    instrumentationSnippets.append(snip);
    return instrumentationSnippets.size();
}


TextSection* ElfFileInst::getTextSection(){
    uint16_t textIdx = elfFile->findSectionIdx(".text");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    return textSection;
}
TextSection* ElfFileInst::getFiniSection(){
    uint16_t textIdx = elfFile->findSectionIdx(".fini");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    return textSection;
}
TextSection* ElfFileInst::getInitSection(){
    uint16_t textIdx = elfFile->findSectionIdx(".init");
    TextSection* textSection = (TextSection*)elfFile->getRawSection(textIdx);
    return textSection;
}

InstrumentationFunction* ElfFileInst::getInstrumentationFunction(const char* funcName){
    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        if (instrumentationFunctions[i]){
            if (!strcmp(instrumentationFunctions[i]->getFunctionName(),funcName)){
                return instrumentationFunctions[i];
            }
        }
    }
    return NULL;
}


// the order of the operations in this function matters
void ElfFileInst::phasedInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    uint16_t textIdx = elfFile->findSectionIdx(".text");
    ASSERT(textIdx && "Cannot find the text section");
    TextSection* text = (TextSection*)elfFile->getRawSection(textIdx);
    ASSERT(text && text->getType() == ElfClassTypes_TextSection && "Cannot find the text section");

    ASSERT(elfFile->getFileHeader()->GET(e_flags) == EFINSTSTATUS_NON && "This executable appears to already be instrumented");
    elfFile->getFileHeader()->SET(e_flags,EFINSTSTATUS_MOD);
    
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

    extendTextSection(0x4000000);
    extendDataSection(0x2000000);

    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    instrument();

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    addSharedLibraryPath();

    for (uint32_t i = 0; i < instrumentationLibraries.size(); i++){
        addSharedLibrary(instrumentationLibraries[i]);
    }

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        ASSERT(instrumentationFunctions[i] && "Instrumentation functions should be initialized");
        addFunction(instrumentationFunctions[i]);
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
    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        instrumentationFunctions[i]->dump(binaryOutputFile,extraTextOffset);
    }
    for (uint32_t i = 0; i < instrumentationSnippets.size(); i++){
        instrumentationSnippets[i]->dump(binaryOutputFile,extraTextOffset);
    }
    for (uint32_t i = 0; i < instrumentationPoints.size(); i++){
        instrumentationPoints[i]->dump(binaryOutputFile,extraTextOffset);
    }
    for (uint32_t i = 0; i < relocatedFunctions.size(); i++){
        relocatedFunctions[i]->dump(binaryOutputFile,extraTextOffset+relocatedFunctionOffsets[i]);
    }
}

bool ElfFileInst::verify(){
    if (!elfFile->verify()){
        return false;
    }
    return true;
}

InstrumentationPoint* ElfFileInst::addInstrumentationPoint(Base* instpoint, Instrumentation* inst, uint32_t sz){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");    

    InstrumentationPoint* newpoint = new InstrumentationPoint(instpoint,inst,sz,InstLocation_dont_care);
    bool canInstrument = true;

    // we do this for now, it prevents us from trying to instrument at the same address twice
    /*
    for (uint32_t i = 0; i < instrumentationPoints.size(); i++){
        if (instrumentationPoints[i]){
            if (newpoint->getSourceAddress() == instrumentationPoints[i]->getSourceAddress()){
                canInstrument = false;
            }
        }
    }
    */

    if (canInstrument){
        instrumentationPoints.append(newpoint);
    } else {
        delete newpoint;
    }

    ASSERT(instrumentationPoints.back());
    return instrumentationPoints.back();
}

InstrumentationFunction* ElfFileInst::declareFunction(char* funcName){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (!strcmp(funcName,func->getFunctionName())){
            PRINT_ERROR("Trying to add a function that was already added -- %s", funcName);
            return NULL;
        }
    }

    InstrumentationFunction* newFunction;
    if (elfFile->is64Bit()){
        newFunction = new InstrumentationFunction64(instrumentationFunctions.size(), funcName, reserveDataOffset(Size__64_bit_Global_Offset_Table_Entry));
    } else {
        newFunction = new InstrumentationFunction32(instrumentationFunctions.size(), funcName, reserveDataOffset(Size__32_bit_Global_Offset_Table_Entry));
    }
    instrumentationFunctions.append(newFunction);

    return instrumentationFunctions.back();
}

uint32_t ElfFileInst::declareLibrary(char* libName){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < instrumentationLibraries.size(); i++){
        if (!strcmp(libName,instrumentationLibraries[i])){
            PRINT_ERROR("Trying to add a library that was already added -- %s", libName);
            return instrumentationLibraries.size();
        }
    }

    char* newLib = new char[strlen(libName)+1];
    strcpy(newLib,libName);
    newLib[strlen(libName)] = '\0';

    instrumentationLibraries.append(newLib);
    return instrumentationLibraries.size();
}


uint64_t ElfFileInst::addPLTRelocationEntry(uint32_t symbolIndex, uint64_t gotOffset){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    DynamicTable* dynTable = elfFile->getDynamicTable();

    ASSERT(dynTable->countDynamics(DT_JMPREL) == 1 && "Cannot find a unique Relocation table for this file");
    
    uint64_t relocTableAddr = dynTable->getDynamicByType(DT_JMPREL,0)->GET_A(d_val,d_un);
    ASSERT(relocTableAddr && "Count not find a relocation table address in the dynamic table");

    RelocationTable* relocTable = (RelocationTable*)elfFile->getRawSection(elfFile->findSectionIdx(relocTableAddr));
    ASSERT(relocTable->getType() == ElfClassTypes_RelocationTable && "Found wrong section type when searching for relocation table");

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
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }

    // shrink the size of the extra text section to accomodate the increase in size of the control sections
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


void ElfFileInst::extendTextSection(uint64_t size){
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

    size = nextAlignAddress(size,elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align));
    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ASSERT(!extraTextIdx && "Cannot extend the text segment more than once");

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
            if (subHeader->GET(p_vaddr) < size){
                PRINT_WARN(5,"Unable to extend text section by 0x%llx bytes: the maximum size of a text extension for this binary is 0x%llx bytes", size, subHeader->GET(p_vaddr));
            }
            ASSERT(subHeader->GET(p_vaddr) >= size && "The text extension size is too large");
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
    textHeader->SET(p_vaddr,textHeader->GET(p_vaddr)-size);
    textHeader->SET(p_paddr,textHeader->GET(p_paddr)-size);
    textHeader->INCREMENT(p_memsz,size);
    textHeader->INCREMENT(p_filesz,size);

    // For any section that falls before the program's code, displace its address so that it is in the
    // same location relative to the base address.
    // Likewise, displace the offset of any section that falls during/after the program's code so that
    // the code will be put in the correct location within the text segment.
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        if (i < lowestTextSectionIdx){
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) < lowestTextAddress && "No section that occurs before the first text section should have a larger address");
            // strictly speaking the loader doesn't use these, but for consistency we change them anyway
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) > size && "The text extension size is too large");
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
    elfFile->addSection(lowestTextSectionIdx, ElfClassTypes_TextSection, elfFile->getFileName(), textHdr->GET(sh_name), textHdr->GET(sh_type),
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
}


ElfFileInst::~ElfFileInst(){
    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        delete instrumentationFunctions[i];
    }

    for (uint32_t i = 0; i < instrumentationSnippets.size(); i++){
        delete instrumentationSnippets[i];
    }

    for (uint32_t i = 0; i < instrumentationPoints.size(); i++){
        delete instrumentationPoints[i];
    }

    for (uint32_t i = 0; i < instrumentationLibraries.size(); i++){
        delete[] instrumentationLibraries[i];
    }

    if (lineInfoFinder){
        delete lineInfoFinder;
    }

    if (instSuffix){
        delete[] instSuffix;
    }

    if (sharedLibraryPath){
        delete[] sharedLibraryPath;
    }

    for (uint32_t i = 0; i < relocatedFunctions.size(); i++){
        delete relocatedFunctions[i];
    }

    for (uint32_t i = 0; i < specialDataRefs.size(); i++){
        delete specialDataRefs[i];
    }
}

void ElfFileInst::dump(char* extension){
    ASSERT(currentPhase == ElfInstPhase_dump_file && "Instrumentation phase order must be observed");

    char fileName[80] = "";
    sprintf(fileName,"%s.%s", elfFile->getFileName(), extension);

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
    print(Print_Code_All);
}

void ElfFileInst::print(uint32_t printCodes){
    elfFile->print(printCodes);

    if (HAS_PRINT_CODE(printCodes,Print_Code_Instrumentation)){
        PRINT_INFOR("Instrumentation Reservations:");
        PRINT_INFOR("================");
        if (extraTextIdx){
            SectionHeader* extendedText = elfFile->getSectionHeader(extraTextIdx);
            PRINT_INFOR("Extended TEXT section is section %hd", extraTextIdx);
            PRINT_INFOR("\tExtra text space available @address 0x%016llx + %d bytes", extendedText->GET(sh_addr), extendedText->GET(sh_size));
            PRINT_INFOR("\tExtra text space available @offset  0x%016llx + %d bytes", extendedText->GET(sh_offset), extendedText->GET(sh_size));
        }
        if (extraDataIdx){
            SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);
            PRINT_INFOR("Extended DATA section is section %hd", extraDataIdx);
            PRINT_INFOR("\tExtra data space available @address 0x%016llx + %d bytes", extendedData->GET(sh_addr), extendedData->GET(sh_size));
            PRINT_INFOR("\tExtra data space available @offset  0x%016llx + %d bytes", extendedData->GET(sh_offset), extendedData->GET(sh_size));
        }
        if (instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END]){
            PRINT_INFOR("Bytes for data initialization: %d", instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END]->snippetSize());
        }
    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_Disassemble)){
        Base::disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

        PRINT_INFOR("Relocated Text Disassembly");
        PRINT_INFOR("=============");
        for (uint32_t i = 0; i < relocatedFunctions.size(); i++){
            fprintf(stdout, "\n");
            if (HAS_PRINT_CODE(printCodes,Print_Code_Instruction)){
                relocatedFunctions[i]->printDisassembly(true);
            } else {
                relocatedFunctions[i]->printDisassembly(false);
            }
        }

        Base::disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
    }
}


ElfFileInst::ElfFileInst(ElfFile* elf){
    currentPhase = ElfInstPhase_no_phase;
    elfFile = elf;

    // automatically set 2 snippet for the beginning and end of bootstrap code
    instrumentationSnippets.append(new InstrumentationSnippet());
    instrumentationSnippets.append(new InstrumentationSnippet());

    // automatically set the 1st instrumentation point to go to the bootstrap code
    instrumentationPoints.append(NULL);

    extraTextIdx = 0;
    extraDataIdx = 0;
    dataIdx = 0;

    usableDataOffset = 0;
    bssReserved = 0;

    instSuffix = NULL;
    sharedLibraryPath = NULL;

    lineInfoFinder = NULL;
    if (elfFile->getLineInfoSection()){
        lineInfoFinder = new LineInfoFinder(elfFile->getLineInfoSection());
    }

    DataReference* zeroAddrRef = new DataReference(0,NULL,elfFile->is64Bit(),0);
    specialDataRefs.append(zeroAddrRef);

    anchorsAreSorted = false;
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

    SectionHeader* hashHeader = elfFile->getSectionHeader(hashTable->getSectionIndex());
    uint32_t extraSize = extraHashEntries * hashTable->getEntrySize();
    hashHeader->INCREMENT(sh_size,extraSize);

    for (uint32_t i = hashTable->getSectionIndex() + 1; i <= extraTextIdx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }
    ASSERT(extraSize <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough room to insert extra ELF control");
    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);

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

    for (uint32_t i = 0; i < elfFile->getNumberOfStringTables(); i++){
        StringTable* strTab = elfFile->getStringTable(i);
        SectionHeader* sHdr = elfFile->getSectionHeader(strTab->getSectionIndex());

        if (sHdr->GET(sh_addr) == stringTableAddr){
            ASSERT(stringTableIdx == elfFile->getNumberOfStringTables() && "Cannot have multiple string tables linked to the dynamic table");
            stringTableIdx = i;
        }
    }
    ASSERT(stringTableIdx != elfFile->getNumberOfStringTables() && "There must be a string table that is identifiable with the dynamic table");

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

    verify();

    return relocationOffset;
}


uint32_t ElfFileInst::addSharedLibraryPath(){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    if (!sharedLibraryPath){
        return 0;
    }

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strOffset = addStringToDynamicStringTable(sharedLibraryPath);

    // add a DT_NEEDED entry to the dynamic table
    uint32_t emptyDynamicIdx = dynamicTable->findEmptyDynamic();

    ASSERT(emptyDynamicIdx < dynamicTable->getNumberOfDynamics() && "No free entries found in the dynamic table");

    dynamicTable->getDynamic(emptyDynamicIdx)->SET(d_tag,DT_RPATH);
    dynamicTable->getDynamic(emptyDynamicIdx)->SET_A(d_ptr,d_un,strOffset);

    verify();

    return strOffset;
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

    verify();

    return strOffset;
}

uint64_t ElfFileInst::relocateDynamicSection(){
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    __FUNCTION_NOT_IMPLEMENTED;
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
