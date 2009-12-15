#include <ElfFileInst.h>

#include <AddressAnchor.h>
#include <Base.h>
#include <BasicBlock.h>
#include <DynamicTable.h>
#include <ElfFile.h>
#include <FileHeader.h>
#include <Function.h>
#include <FlowGraph.h>
#include <GnuVersion.h>
#include <HashTable.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <ProgramHeader.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <TextSection.h>

DEBUG(
uint32_t readBytes = 0;
);

#define TEXT_EXTENSION_FUNC (nextAlignAddress(0x10000 + (0x120 * numberOfBasicBlocks), 0x4000))
#define DATA_EXTENSION_INC  0x4000

// some common macros to help debug the instrumentation process
//#define RELOC_MOD_OFF 233
//#define RELOC_MOD 512
//#define TURNOFF_FUNCTION_RELOCATION
//#define BLOAT_MOD_OFF 704
//#define BLOAT_MOD 2048
//#define TURNOFF_FUNCTION_BLOAT
//#define SWAP_MOD_OFF 2
//#define SWAP_MOD 32768
//#define SWAP_FUNCTION_ONLY "raise"
//#define TURNOFF_INSTRUCTION_SWAP
#define ANCHOR_SEARCH_BINARY
//#define VALIDATE_ANCHOR_SEARCH

#ifdef BLOAT_MOD
uint32_t bloatCount = 0;
#endif

void ElfFileInst::extendDataSection(uint64_t size){
    if (instrumentationDataSize){
        char* tmpData = new char[instrumentationDataSize + size];
        memcpy(tmpData, instrumentationData, instrumentationDataSize);
        bzero(tmpData + instrumentationDataSize, size);
        delete[] instrumentationData;
        instrumentationData = tmpData;
    } else {
        ASSERT(!instrumentationData);
        instrumentationData = new char[size];
        bzero(instrumentationData, size);
    }
    for (uint32_t i = 0; i < size; i++){
        //        instrumentationData[instrumentationDataSize + i] = i % 0x100;
    }
    instrumentationDataSize += size;
}

void ElfFileInst::buildInstrumentationData(){
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    ProgramHeader* dataSegmentHeader = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());
    DataSection* dataSection = (DataSection*)elfFile->getRawSection(extraDataIdx);
    ASSERT(dataSection && dataSection->getType() == PebilClassType_DataSection);

    SectionHeader* dataSectionHeader = elfFile->getSectionHeader(extraDataIdx);

    uint32_t dataSegmentInc = instrumentationDataSize;
    dataSegmentInc += systemReservedBss;

    dataSegmentHeader->INCREMENT(p_memsz, dataSegmentInc);
    dataSegmentHeader->INCREMENT(p_filesz, dataSegmentInc);

    dataSection->extendSize(instrumentationDataSize);
    dataSection->setBytesAtOffset(dataSectionHeader->GET(sh_size), instrumentationDataSize, instrumentationData);
    dataSectionHeader->INCREMENT(sh_size, instrumentationDataSize);


    uint32_t align = 0;
    uint32_t sOffsetInc = 0;
    for (uint32_t i = extraDataIdx + 1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        if (sHdr->GET(sh_offset) > elfFile->getFileHeader()->GET(e_shoff) && !sOffsetInc){
            sOffsetInc = instrumentationDataSize + align;
        }

        align += nextAlignAddress(sHdr->GET(sh_addr) + instrumentationDataSize + align, sHdr->GET(sh_addralign)) - (sHdr->GET(sh_addr) + instrumentationDataSize + align);
        sHdr->INCREMENT(sh_offset, instrumentationDataSize + align);
    }
    elfFile->getFileHeader()->INCREMENT(e_shoff, sOffsetInc);

    for (uint32_t i = extraDataIdx + 2; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* scn = elfFile->getSectionHeader(i);
        //        ASSERT(!scn->GET(sh_addr) && "The bss section should be the final section the programs address space");
    }

    //    dataSection->printBytes(programDataSize, 0, 0);
    verify();
}


void ElfFileInst::initializeDisabledFunctions(char* inputFuncList){
    ASSERT(!disabledFunctions.size());
    
    FILE* inFile = NULL;
    inFile = fopen(inputFuncList, "r");
    if(!inFile){
        PRINT_ERROR("Input file can not be opened [%s]", inputFuncList);
    }

    char* inBuffer = new char[__MAX_STRING_SIZE];
    while (fgets(inBuffer, __MAX_STRING_SIZE, inFile) != NULL) {
        char* line = new char[strlen(inBuffer)+1];
        sprintf(line, "%s", inBuffer);
        line[strlen(inBuffer)-1] = '\0';
        disabledFunctions.append(line);
    }
    delete[] inBuffer;
    fclose(inFile);
}

bool ElfFileInst::isDisabledFunction(Function* func){
    for (uint32_t i = 0; i < disabledFunctions.size(); i++){
        if (!strcmp(func->getName(), disabledFunctions[i])){
            return true;
        }
    }
    return false;
}

uint32_t ElfFileInst::initializeReservedData(uint64_t address, uint32_t size, void* data){
    InstrumentationSnippet* snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END];
    char* bytes = (char*)data;

    if (address + size > getExtraDataAddress() + usableDataOffset ||
        address < getExtraDataAddress()){
        PRINT_INFOR("address range %#llx+%d out of range of data section [%#llx,%#llx)", address, size, getExtraDataAddress(),
                    getExtraDataAddress() + usableDataOffset);
    }
    ASSERT(address + size <= getExtraDataAddress() + usableDataOffset && address >= getExtraDataAddress() &&
           "Data initialization address out of range, you should reserve the data first");

    memcpy(instrumentationData + address - getExtraDataAddress(), bytes, size);

    return size;
}

bool ElfFileInst::isEligibleFunction(Function* func){
    if (!canRelocateFunction(func)){
        return false;
    }
    if (func->isInstrumentationFunction()){
        return false;
    }
    if (func->isJumpTable()){
        return false;
    }
    if (isDisabledFunction(func)){
        return false;
    }
    if (func->getNumberOfBytes() < SIZE_NEEDED_AT_INST_POINT){
        return false;
    }
    return true;
}

Vector<AddressAnchor*>* ElfFileInst::searchAddressAnchors(uint64_t addr){
    Vector<AddressAnchor*>* linearUpdate = new Vector<AddressAnchor*>();
    Vector<AddressAnchor*>* binaryUpdate = new Vector<AddressAnchor*>();
    uint64_t binIdx = 0, linIdx = 0;

#if defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)
    if (!anchorsAreSorted){
        addressAnchors.sort(compareLinkBaseAddress);
        anchorsAreSorted = true;
    }
    AddressAnchor** allAnchors = &addressAnchors;
#ifdef DEBUG_ANCHOR
    PRINT_DEBUG_ANCHOR("Array is:");
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        PRINT_DEBUG_ANCHOR("%#llx", allAnchors[i]->linkBaseAddress);
    }
#endif //DEBUG_ANCHOR
    void* anchor = bsearch(&addr, allAnchors, addressAnchors.size(), sizeof(AddressAnchor*), searchLinkBaseAddressExact);
    if (anchor){
        // get the FIRST occurrence of addr in the anchor array
        uint64_t idx = ((uint64_t)anchor-(uint64_t)allAnchors)/sizeof(AddressAnchor*);
        while (idx > 0 && addressAnchors[idx-1]->linkBaseAddress == addressAnchors[idx]->linkBaseAddress){
            idx--;
        }
        binIdx = idx;
        while (idx < addressAnchors.size() &&
                addressAnchors[idx]->linkBaseAddress <= addr &&
               addr < addressAnchors[idx]->linkBaseAddress + addressAnchors[idx]->getLink()->getSizeInBytes()){
            PRINT_DEBUG_ANCHOR("found %#llx <= %#llx < %#llx + %d", addressAnchors[idx]->linkBaseAddress, addr, addressAnchors[idx]->linkBaseAddress, addressAnchors[idx]->getLink()->getSizeInBytes());
            (*binaryUpdate).append(allAnchors[idx++]);
        }
    }
#endif //defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)

#if !defined(ANCHOR_SEARCH_BINARY) || defined(VALIDATE_ANCHOR_SEARCH)
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        /*
        if (addr >= addressAnchors[i]->linkBaseAddress &&
            addr <  addressAnchors[i]->linkBaseAddress + addressAnchors[i]->getLink()->getSizeInBytes()){
        */
        if (addr == addressAnchors[i]->linkBaseAddress){
            PRINT_DEBUG_ANCHOR("%#llx <= %#llx < %#llx", addressAnchors[i]->linkBaseAddress, addr, addressAnchors[i]->linkBaseAddress + addressAnchors[i]->getLink()->getSizeInBytes());
            PRINT_DEBUG_ANCHOR("%#llx <= %#llx < %#llx", addressAnchors[i]->linkBaseAddress, addr, addressAnchors[i]->linkBaseAddress + addressAnchors[i]->getLink()->getSizeInBytes());
            if (!(*linearUpdate).size()){
                linIdx = i;
            }
            linearUpdate->append(addressAnchors[i]);
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
        for (uint32_t i = 0; i < addressAnchors.size(); i++){
            PRINT_DEBUG_ANCHOR("anchors[%d]: %#llx", i, allAnchors[i]->linkBaseAddress);
        }

    }
    ASSERT(addressAnchors.isSorted(compareLinkBaseAddress));
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

#ifdef DEBUG_ANCHOR
    for (uint32_t i = 0; i < instructionCount; i++){
        allInstructions[i]->print();
    }
#endif

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
            uint64_t relativeAddress = currentInstruction->getRelativeValue() + currentInstruction->getBaseAddress() + currentInstruction->getSizeInBytes();
            if (!elfFile->is64Bit()){
                if (!currentInstruction->isControl() || currentInstruction->usesIndirectAddress()){
                    relativeAddress += currentInstruction->getSizeInBytes();
                }
            }
            if (!currentInstruction->getRelativeValue()){
                PRINT_WARN(4,"An Instruction links to null address %#llx", currentInstruction->getRelativeValue());
            }
            PRINT_DEBUG_ANCHOR("Searching for relative address %llx", relativeAddress);

            // search other instructions
            void* link = bsearch(&relativeAddress, allInstructions, instructionCount, sizeof(Instruction*), searchBaseAddressExact);
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
                            addressAnchors.append(currentInstruction->getAddressAnchor());
                            currentInstruction->getAddressAnchor()->setIndex(anchorCount);
                            dataRawSection->addDataReference(dataRef);
                            anchorCount++;
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
                addressAnchors.append(currentInstruction->getAddressAnchor());
                currentInstruction->getAddressAnchor()->setIndex(anchorCount);
                anchorCount++;
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
        // ^^^NO TO THIS^^^, we will check just word-aligned addresses since we were getting false positives
        for (uint32_t currByte = 0; currByte < dataRawSection->getSizeInBytes() - addrAlign; currByte += sizeof(uint32_t)){
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

            void* link = bsearch(&extendedData,allInstructions,instructionCount,sizeof(Instruction*),searchBaseAddressExact);
            if (link != NULL){
                if (!(dataSectionHeader->GET(sh_addr)+currByte % sizeof(uint64_t))){
                    PRINT_WARN(10, "unaligned data %#llx at %llx", extendedData, dataSectionHeader->GET(sh_addr)+currByte);
                }

                Instruction* linkedInstruction = *(Instruction**)link;
                PRINT_DEBUG_ANCHOR("Found data -> inst link: %#llx -> %#llx, offset %x", dataSectionHeader->GET(sh_addr)+currByte, extendedData, currByte);
                DataReference* dataRef = new DataReference(extendedData, dataRawSection, addrAlign, currByte);
#ifdef DEBUG_ANCHOR
                dataRef->print();
#endif
                dataRef->initializeAnchor(linkedInstruction);
                dataRawSection->addDataReference(dataRef);

                addressAnchors.append(dataRef->getAddressAnchor());
                dataRef->getAddressAnchor()->setIndex(anchorCount);
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

    ASSERT(anchorCount == addressAnchors.size());
#ifdef DEBUG_ANCHOR
    for (uint32_t i = 0; i < addressAnchors.size(); i++){
        PRINT_INFOR("");
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
    Vector<Instruction*>* trampEmpty = new Vector<Instruction*>();

    uint32_t currentByte = 0;

    (*trampEmpty).append(InstructionGenerator::generateJumpRelative(functionToRelocate->getBaseAddress(), relocationAddress));
    currentByte += (*trampEmpty).back()->getSizeInBytes();
    (*trampEmpty).back()->setBaseAddress(functionToRelocate->getBaseAddress());

    if (currentByte > functionToRelocate->getNumberOfBytes()){
        PRINT_WARN(5, "Function %s at address %#llx is only %d bytes -- cannot relocate or fully instrument", functionToRelocate->getName(), functionToRelocate->getBaseAddress(), functionToRelocate->getNumberOfBytes());
        for (uint32_t i = 0; i < (*trampEmpty).size(); i++){
            delete (*trampEmpty)[i];
        }
        delete trampEmpty;
        return functionToRelocate->getNumberOfBytes();
    }
    ASSERT(currentByte <= functionToRelocate->getNumberOfBytes() && "Function is not big enough to relocate");

    Function* placeHolder = new Function(text,functionToRelocate->getIndex(),functionToRelocate->getFunctionSymbol(),functionSize);
    Vector<AddressAnchor*>* modAnchors = searchAddressAnchors(functionToRelocate->getBaseAddress());
    for (uint32_t i = 0; i < modAnchors->size(); i++){
        (*modAnchors)[i]->updateLink((*trampEmpty).back());
        anchorsAreSorted = false;
    }
    delete modAnchors;

    while (currentByte < functionSize){
        (*trampEmpty).append(InstructionGenerator::generateInterrupt(X86TRAPCODE_BREAKPOINT));
        (*trampEmpty).back()->setBaseAddress(functionToRelocate->getBaseAddress()+currentByte);
        currentByte += (*trampEmpty).back()->getSizeInBytes();
    }

    placeHolder->generateCFG(trampEmpty);
    delete trampEmpty;

    Function* displacedFunction = text->replaceFunction(functionToRelocate->getIndex(), placeHolder);
    relocatedFunctions.append(displacedFunction);
    relocatedFunctionOffsets.append(offsetToRelocation);
    ASSERT(relocatedFunctions.size() == relocatedFunctionOffsets.size());

    uint64_t oldBase = displacedFunction->getBaseAddress();
    uint64_t oldSize = displacedFunction->getSizeInBytes();

    // adjust the base addresses of the relocated function
    displacedFunction->setBaseAddress(relocationAddress);

    // bloat the blocks in the function
    bool doBloat = true;
#ifdef TURNOFF_CODE_BLOAT
    doBloat = false;
#endif
#ifdef BLOAT_MOD
    doBloat = false;
    if (bloatCount % BLOAT_MOD == BLOAT_MOD_OFF){
        doBloat = true;
        PRINT_INFOR("Bloating function (%d) %s", bloatCount, displacedFunction->getName());
    } else {
        doBloat = false;
    }
    bloatCount++;
#endif


    if (!displacedFunction->hasCompleteDisassembly()){
        PRINT_ERROR("Function %s before bloated to have bad disassembly", displacedFunction->getName());
    }
    if (doBloat){
        displacedFunction->bloatBasicBlocks(bloatType, SIZE_NEEDED_AT_INST_POINT);
    }
    if (!displacedFunction->hasCompleteDisassembly()){
        PRINT_ERROR("Function %s after bloated to have bad disassembly", displacedFunction->getName());
    }

    PRINT_DEBUG_FUNC_RELOC("Function %s relocation map [%#llx,%#llx) --> [%#llx,%#llx)", displacedFunction->getName(), oldBase, oldBase+oldSize, displacedFunction->getBaseAddress(), displacedFunction->getBaseAddress() + displacedFunction->getSizeInBytes());
    PRINT_DEBUG_FUNC_RELOC("Function %s placeholder [%#llx,%#llx)", placeHolder->getName(), placeHolder->getBaseAddress(), placeHolder->getBaseAddress() + placeHolder->getSizeInBytes());

    return displacedFunction->getNumberOfBytes();
}

BasicBlock* ElfFileInst::getProgramEntryBlock(){
    return programEntryBlock;
}

// the order of operations in this function in very important, things will break if they are changed
void ElfFileInst::generateInstrumentation(){
#ifdef VALIDATE_ANCHOR_SEARCH
    PRINT_INFOR("Validating anchor search, this can cause much longer instrumentation times, see VALIDATE_ANCHOR_SEARCH in %s", __FILE__);
#else
    PRINT_INFOR("Not validating anchor search, if problems are encountered try enabling VALIDATE_ANCHOR_SEARCH in %s", __FILE__);
#endif

    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    buildInstrumentationData();

    TextSection* textSection = getTextSection();
    TextSection* pltSection = getPltSection();

    uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);

    InstrumentationSnippet* snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN];
    snip->setRequiresDistinctTrampoline(true);
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        snip->addSnippetInstruction(InstructionGenerator32::generateStackPush(i));
    }

    // add code to correctly initialize the instrumentation data and application bss sections
    ASSERT(instrumentationDataSize);

    uint64_t programDataAddress = getExtraDataSection()->getSectionHeader()->GET(sh_addr);
    uint64_t instdataAddress = programDataAddress + programDataSize + systemReservedBss;

    // these two aren't necessary but show us the low-end bound of the string move that is about to happen
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instdataAddress, X86_REG_SI));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instdataAddress + programBssSize, X86_REG_DI));

    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instdataAddress + instrumentationDataSize, X86_REG_SI));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instdataAddress + instrumentationDataSize + programBssSize, X86_REG_DI));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instrumentationDataSize / sizeof(uint32_t), X86_REG_CX));

    snip->addSnippetInstruction(InstructionGenerator::generateSetDirectionFlag(true));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveString(true));

    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(0, X86_REG_AX));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg(instdataAddress, X86_REG_DI));
    snip->addSnippetInstruction(InstructionGenerator::generateMoveImmToReg((programBssSize - systemReservedBss)/ sizeof(uint32_t), X86_REG_CX));
    snip->addSnippetInstruction(InstructionGenerator::generateSetDirectionFlag(false));
    snip->addSnippetInstruction(InstructionGenerator::generateSTOSWord(true));
    

    // find the entry point of the program and put an instrumentation point for our initialization there
    ASSERT(!instrumentationPoints[INST_POINT_BOOTSTRAP1] && "instrumentationPoint[INST_POINT_BOOTSTRAP1] is reserved");
    BasicBlock* entryBlock = getProgramEntryBlock();

    ASSERT(entryBlock && "Cannot find instruction at the program's entry point");
    
    if (elfFile->is64Bit()){
        instrumentationPoints[INST_POINT_BOOTSTRAP1] = 
            new InstrumentationPoint64((Base*)entryBlock, instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN], SIZE_FIRST_INST_POINT, InstLocation_dont_care);    
    } else {
        instrumentationPoints[INST_POINT_BOOTSTRAP1] = 
            new InstrumentationPoint32((Base*)entryBlock, instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN], SIZE_FIRST_INST_POINT, InstLocation_dont_care);    
    }
    instrumentationPoints[INST_POINT_BOOTSTRAP1]->setPriority(InstPriority_sysinit);
            
    uint64_t codeOffset = relocatedTextSize;

    // some padding so that the instrumentation code is more readable
    codeOffset += 32;


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
    codeOffset += snip->snippetSize();

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (func){
            func->setBootstrapOffset(codeOffset);
            codeOffset += func->bootstrapReservedSize();
        }
    }

    snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END];
    snip->setRequiresDistinctTrampoline(true);
    snip->setCodeOffset(codeOffset);

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        snip->addSnippetInstruction(InstructionGenerator32::generateStackPop(X86_32BIT_GPRS-i-1));
    }
    snip->addSnippetInstruction(InstructionGenerator::generateReturn());

    codeOffset += snip->snippetSize();

    uint64_t returnOffset = 0;
    uint64_t chainOffset = 0;

    instrumentationPoints.sort(compareInstAddress);

    PRINT_DEBUG_DATA_PLACEMENT("Register storage is at address %#llx", getExtraDataAddress() + regStorageOffset);

    for (uint32_t i = 0; i < instrumentationPoints.size(); i++){
        InstrumentationPoint* pt = instrumentationPoints[i];
        if (!pt){
            PRINT_ERROR("Instrumentation point %d should exist", i);
        }
#ifdef TURNOFF_INSTRUCTION_SWAP
        break;
#endif

        bool performSwap = true;

#ifdef SWAP_MOD
        performSwap = false;
        if (i % SWAP_MOD == SWAP_MOD_OFF || pt->getPriority() < InstPriority_regular){
            if (pt->getPointType() == PebilClassType_BasicBlock){
                BasicBlock* bb = (BasicBlock*)pt->getSourceObject();
#ifdef SWAP_FUNCTION_ONLY
                if (strstr(bb->getFunction()->getName(), SWAP_FUNCTION_ONLY)){
#endif
                    PRINT_INFOR("Performing instruction swap at for point (%d/%d) %#llx in %s", i, instrumentationPoints.size(), pt->getSourceObject()->getBaseAddress(), bb->getFunction()->getName());
                    performSwap = true;
#ifdef SWAP_FUNCTION_ONLY
                }
#endif

            } else if (pt->getPointType() == PebilClassType_Instruction){
                Instruction* ins = (Instruction*)pt->getSourceObject();
#ifdef SWAP_FUNCTION_ONLY
                if (strstr(ins->getContainer()->getName(), SWAP_FUNCTION_ONLY)){
#endif
                    PRINT_INFOR("Performing instruction swap at for point (%d/%d) %#llx in %s", i, instrumentationPoints.size(), pt->getSourceObject()->getBaseAddress(), ins->getContainer()->getName());
                    performSwap = true;
#ifdef SWAP_FUNCTION_ONLY
                }
#endif
            }
        }
#endif

        if (performSwap){
            if (!pt->getInstAddress()){
                PRINT_WARN(4,"Could not find a place to instrument for point at %#llx", pt->getSourceObject()->getBaseAddress());
                continue;
            }
            PRINT_DEBUG_INST("Generating code for InstrumentationPoint %d at address %llx", i, pt->getInstAddress());
            PRINT_DEBUG_POINT_CHAIN("Examining instrumentation point %d at %#llx", i, pt->getInstAddress());
            
            bool isFirstInChain = false;
            if (i == 0 || 
                (i > 0 && instrumentationPoints[i-1]->getInstAddress() != pt->getInstAddress())){
                PRINT_DEBUG_POINT_CHAIN("\tFirst in chain at %#llx (%d)", pt->getInstAddress(), i);
                
                isFirstInChain = true;
                chainOffset = codeOffset;
            }
            
            Vector<Instruction*>* repl = NULL;
            Vector<Instruction*>* displaced = NULL;
            
            repl = new Vector<Instruction*>();
            if (instrumentationPoints[i]->getNumberOfBytes() == SIZE_CONTROL_TRANSFER){
                (*repl).append(InstructionGenerator::generateJumpRelative(pt->getInstAddress(), elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + chainOffset));
            } else {
                PRINT_ERROR("This size instrumentation point (%d) not supported", instrumentationPoints[i]->getNumberOfBytes());
            }
            
            // disassemble the newly minted instructions
            uint32_t bytesUsed = 0;
            for (uint32_t j = 0; j < (*repl).size(); j++){
                (*repl)[j]->setBaseAddress(pt->getInstAddress()+bytesUsed);
                bytesUsed += (*repl)[j]->getSizeInBytes();
                
#ifdef DEBUG_INST
                (*repl)[j]->print();
#endif
            }
            
            ASSERT((*repl).back()->getSizeInBytes() == SIZE_NEEDED_AT_INST_POINT ||
                   (*repl).back()->getSizeInBytes() == SIZE_FIRST_INST_POINT && "Instruction at instrumentation point has a different size than expected");
            
            
            
            bool isLastInChain = false;
            if (i == instrumentationPoints.size()-1 || 
                (i < instrumentationPoints.size()-1 && instrumentationPoints[i+1]->getInstAddress() != pt->getInstAddress())){
                PRINT_DEBUG_POINT_CHAIN("\tLast of chain at %#llx (%d)", pt->getInstAddress(), i);
                isLastInChain = true;
                
                displaced = pt->swapInstructionsAtPoint(repl);
                ASSERT((*repl).size());
                ASSERT((*displaced).size());
                
                // update any address anchor that pointed to the old instruction to point to the new
                for (uint32_t j = 0; j < (*displaced).size(); j++){
                    Vector<AddressAnchor*>* modAnchors = searchAddressAnchors((*displaced)[j]->getBaseAddress());
                    PRINT_DEBUG_ANCHOR("Looking for anchors for address %#llx", (*displaced)[j]->getBaseAddress());
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
            }
            
            if (isFirstInChain){
                returnOffset = pt->getInstAddress() - elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + (*repl)[0]->getSizeInBytes();
            }
            
            uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);
            
            bool stackIsSafe = false;
            if (pt->getPointType() == PebilClassType_BasicBlock){
                BasicBlock* bb = (BasicBlock*)pt->getSourceObject();
                if (!bb->getFlowGraph()->getFunction()->hasLeafOptimization()){
                    PRINT_DEBUG_LEAF_OPT("Basic block at %#llx in function %s is safe from leaf optimization", bb->getBaseAddress(), bb->getFlowGraph()->getFunction()->getName());
                    stackIsSafe = true;
                }
            } else if (pt->getPointType() == PebilClassType_Function){
                Function* fn = (Function*)pt->getSourceObject();
                if (!fn->hasLeafOptimization()){
                    PRINT_DEBUG_LEAF_OPT("Function at %#llx in function %s is safe from leaf optimization", fn->getBaseAddress(), fn->getName());
                    stackIsSafe = true;
                }
            } else if (pt->getPointType() == PebilClassType_TextSection){
                TextSection* ts = (TextSection*)pt->getSourceObject();
                BasicBlock* bb = ts->getBasicBlockAtAddress(pt->getInstAddress());
                if (!bb->getFlowGraph()->getFunction()->hasLeafOptimization()){
                    PRINT_DEBUG_LEAF_OPT("Basic block at %#llx in function %s is safe from leaf optimization", bb->getBaseAddress(), bb->getFlowGraph()->getFunction()->getName());
                    stackIsSafe = true;
                }
            }
            
            uint64_t registerStorage = getExtraDataAddress() + regStorageOffset;
            // need to treat the bootstrap instrumentation point specially because the instrumentation data
            // is not initialized to its correct location (that is done by the code in this inst point)
            if (i == INST_POINT_BOOTSTRAP1){
                registerStorage = elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + programDataSize + regStorageOffset;
            }
            pt->generateTrampoline(displaced, textBaseAddress, codeOffset, returnOffset, isLastInChain, registerStorage, stackIsSafe);
            
            codeOffset += pt->sizeNeeded();
            if (!isLastInChain){
                for (uint32_t j = 0; j < (*repl).size(); j++){
                    delete (*repl)[j];
                }
            }
            if (repl){
                delete repl;
            }
            if (displaced){
                delete displaced;
            }
        }
    }

    for (uint32_t i = INST_SNIPPET_BOOTSTRAP_END + 1; i < instrumentationSnippets.size(); i++){        
        snip = instrumentationSnippets[i];
        if (snip){
            snip->setBootstrapOffset(codeOffset);
            codeOffset += snip->bootstrapSize();
            ASSERT(snip->bootstrapSize() == 0 && "Snippets should not require bootstrap code");
        }
    }

    PRINT_INFOR("%d instrumentation points, %lld bytes used for text, %#lld bytes used for data", instrumentationPoints.size(), codeOffset, usableDataOffset);
    ASSERT(codeOffset <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough space in the text section to accomodate the extra code");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (func){
#ifdef DEBUG_INST
            func->print();
#endif
            func->generateGlobalData(textBaseAddress);
            func->generateWrapperInstructions(textBaseAddress, getExtraDataAddress());
            func->generateBootstrapInstructions(textBaseAddress, getExtraDataAddress());

            // plt section only exists in dynamic binary, and for static binary we don't use this value
            uint64_t realPLTaddr = 0;
            if (!elfFile->isStaticLinked()){
                realPLTaddr = pltSection->getBaseAddress();
            }
            func->generateProcedureLinkInstructions(textBaseAddress, getExtraDataAddress(), realPLTaddr);
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
uint64_t ElfFileInst::getExtraDataAddress() { return elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) + programDataSize + programBssSize; }

uint64_t ElfFileInst::reserveDataOffset(uint64_t size){
    ASSERT(currentPhase > ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    while (usableDataOffset + size  >= instrumentationDataSize){
        extendDataSection(DATA_EXTENSION_INC);
    }
    ASSERT(usableDataOffset + size < instrumentationDataSize && "Not enough space for the requested data");

    uint64_t avail = usableDataOffset;
    usableDataOffset += size;
    return avail;
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
TextSection* ElfFileInst::getPltSection(){
    uint16_t textIdx = elfFile->findSectionIdx(".plt");
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

uint64_t ElfFileInst::functionRelocateAndTransform(){

    uint64_t codeOffset = 0;
    uint32_t numberOfFunctions = exposedFunctions.size();

#ifdef TURNOFF_FUNCTION_RELOCATION
    numberOfFunctions = 0;
    PRINT_WARN(10,"Function relocation has been disabled by the macro TURNOFF_FUNCTION_RELOCATION");
#endif
#ifdef TURNOFF_FUNCTION_BLOAT
    PRINT_WARN(10,"Function bloating has been disabled by the macro TURNOFF_FUNCTION_BLOAT");
#endif
#ifdef TURNOFF_INSTRUCTION_SWAP
    PRINT_WARN(10,"Instruction swapping has been disabled by the macro TURNOFF_INSTRUCTION_SWAP");
#endif

    for (uint32_t i = 0; i < numberOfFunctions; i++){
        Function* func = exposedFunctions[i];

        if (!isEligibleFunction(func)){
            func->print();
        }
#ifdef RELOC_MOD
        if (i % RELOC_MOD == RELOC_MOD_OFF){
            PRINT_INFOR("relocating function (%d) %s", i, func->getName());
#endif
            
        ASSERT(isEligibleFunction(func) && func->hasCompleteDisassembly());
        codeOffset += relocateFunction(func, codeOffset);

        

#ifdef RELOC_MOD
        }
#endif
    }
    return codeOffset;
}

void ElfFileInst::functionSelect(){
    uint32_t numberOfBBs = 0;
    uint32_t numberOfBBsReloc = 0;
    uint32_t numberOfMemops = 0;
    uint32_t numberOfMemopsReloc = 0;

    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    TextSection* init = getInitSection();
    ASSERT(text && text->getType() == PebilClassType_TextSection && "Cannot find the text section");
    ASSERT(fini && text->getType() == PebilClassType_TextSection && "Cannot find the fini section");
    ASSERT(init && text->getType() == PebilClassType_TextSection && "Cannot find the init section");

    Vector<TextObject*> textObjects = Vector<TextObject*>();
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        textObjects.append(text->getTextObject(i));
    }
    /*
    for (uint32_t i = 0; i < init->getNumberOfTextObjects(); i++){
        textObjects.append(init->getTextObject(i));
    }
    for (uint32_t i = 0; i < fini->getNumberOfTextObjects(); i++){
        textObjects.append(fini->getTextObject(i));
    }
    */
    // choose the set of functions to expose to the instrumentation tool
    PRINT_DEBUG_FUNC_RELOC("Choosing from %d functions", text->getNumberOfTextObjects()+fini->getNumberOfTextObjects()+init->getNumberOfTextObjects());

    for (uint32_t i = 0; i < textObjects.size(); i++){
        if (textObjects[i]->isFunction()){
            Function* f = (Function*)textObjects[i];

            uint32_t memopsInFunc = 0;
            for (uint32_t i = 0; i < f->getNumberOfBasicBlocks(); i++){
                memopsInFunc += f->getBasicBlock(i)->getNumberOfMemoryOps();
            } 
            numberOfMemops += memopsInFunc;
            numberOfBBs += f->getNumberOfBasicBlocks();

            if (f->hasCompleteDisassembly() && isEligibleFunction(f)){
                PRINT_DEBUG_FUNC_RELOC("\texposed: %s", f->getName());
                exposedFunctions.append(f);
                numberOfMemopsReloc += memopsInFunc;
                numberOfBBsReloc += f->getNumberOfBasicBlocks();
            } else {
                PRINT_DEBUG_FUNC_RELOC("\thidden: %s", f->getName());
                hiddenFunctions.append(f);
                if (f->hasCompleteDisassembly() && f->isJumpTable()){
                    numberOfMemopsReloc += memopsInFunc;
                    numberOfBBsReloc += f->getNumberOfBasicBlocks();
                }
            }
        }
    }

    //PRINT_INFOR("DisassemblyCoverageReportWithJumpTable\tBlocks\t%d\t%d\tMemops\t%d\t%d", numberOfBBs, numberOfBBsReloc, numberOfMemops, numberOfMemopsReloc);
}


// the order of the operations in this function matters
void ElfFileInst::phasedInstrumentation(){
    TIMER(double t1 = timer(), t2; char stepNumber = 'A');
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    ASSERT(elfFile->getFileHeader()->GET(e_flags) == EFINSTSTATUS_NON && "This executable appears to already be instrumented");
    elfFile->getFileHeader()->SET(e_flags,EFINSTSTATUS_MOD);

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

    uint32_t numberOfBasicBlocks = getInitSection()->getNumberOfBasicBlocks() + 
        getTextSection()->getNumberOfBasicBlocks() + getFiniSection()->getNumberOfBasicBlocks();

    extendTextSection(TEXT_EXTENSION_FUNC);
    extendDataSection(DATA_EXTENSION_INC);
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    anchorProgramElements();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    declare();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    functionSelect();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    if (!elfFile->isStaticLinked()){
        addSharedLibraryPath();

        for (uint32_t i = 0; i < instrumentationLibraries.size(); i++){
            addSharedLibrary(instrumentationLibraries[i]);
        }

        for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
            ASSERT(instrumentationFunctions[i] && "Instrumentation functions should be initialized");
            addFunction(instrumentationFunctions[i]);
        }
    }
    relocatedTextSize = functionRelocateAndTransform();


    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    instrument();

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c UsrResrv : %.2f seconds",stepNumber++,t2-t1);t1=t2);
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c Control  : %.2f seconds",stepNumber++,t2-t1);t1=t2);
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    generateInstrumentation();

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c Generate : %.2f seconds",stepNumber++,t2-t1);t1=t2);
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

    InstLocations location = InstLocation_dont_care;
    if (instpoint->getType() == PebilClassType_Instruction){
        location = InstLocation_prior;
    }

    InstrumentationPoint* newpoint;
    if (elfFile->is64Bit()){
        newpoint = new InstrumentationPoint64(instpoint, inst, sz, location);
    } else {
        newpoint = new InstrumentationPoint32(instpoint, inst, sz, location);
    }

    instrumentationPoints.append(newpoint);

    ASSERT(instrumentationPoints.back());
    return instrumentationPoints.back();
}

InstrumentationFunction* ElfFileInst::declareFunction(char* funcName){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (!strcmp(funcName,func->getFunctionName())){
            PRINT_ERROR("Trying to add a function that was already added -- %s", funcName);
            return NULL;
        }
    }

    uint64_t functionEntry = 0;
    if (elfFile->isStaticLinked()){
        for (uint32_t i = 0; i < elfFile->getNumberOfTextSections(); i++){
            for (uint32_t j = 0; j < elfFile->getTextSection(i)->getNumberOfTextObjects(); j++){
                TextObject* tobj = elfFile->getTextSection(i)->getTextObject(j);
                if (tobj->getType() == PebilClassType_Function &&
                    !strcmp(((Function*)tobj)->getName(), funcName)){
                    functionEntry = ((Function*)tobj)->getBaseAddress();
                    ((Function*)tobj)->setInstrumentationFunction();
                    PRINT_WARN(5, "Instrumentation function statically compiled into binary and found -- %s", ((Function*)tobj)->getName());
                }
            }
        }
        if (!functionEntry){
            PRINT_ERROR("In static linked binary instrumentation function %s must be linked in", funcName);
        }
    }
        
    if (elfFile->is64Bit()){
        instrumentationFunctions.append(new InstrumentationFunction64(instrumentationFunctions.size(), funcName, 
                                                                      reserveDataOffset(Size__64_bit_Global_Offset_Table_Entry), functionEntry));
    } else {
        instrumentationFunctions.append(new InstrumentationFunction32(instrumentationFunctions.size(), funcName, 
                                                                      reserveDataOffset(Size__32_bit_Global_Offset_Table_Entry), functionEntry));
    }
    
    return instrumentationFunctions.back();
}

uint32_t ElfFileInst::declareLibrary(char* libName){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

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
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");
    DynamicTable* dynTable = elfFile->getDynamicTable();

    ASSERT(dynTable->countDynamics(DT_JMPREL) == 1 && "Cannot find a unique Relocation table for this file");
    
    uint64_t relocTableAddr = dynTable->getDynamicByType(DT_JMPREL,0)->GET_A(d_val,d_un);
    ASSERT(relocTableAddr && "Count not find a relocation table address in the dynamic table");

    RelocationTable* relocTable = (RelocationTable*)elfFile->getRawSection(elfFile->findSectionIdx(relocTableAddr));
    ASSERT(relocTable->getType() == PebilClassType_RelocationTable && "Found wrong section type when searching for relocation table");

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
        sHdr->INCREMENT(sh_offset, extraSize);
        sHdr->INCREMENT(sh_addr, extraSize);
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
    // but we will try to keep them as consistent as possible anyway
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

    // since some sections were displaced in the file, displace the section header table also
    elfFile->getFileHeader()->INCREMENT(e_shoff,size);

    // update the dynamic table to correctly point to the displaced elf control sections
    if (!elfFile->isStaticLinked()){
        ASSERT(elfFile->getDynamicTable());
        for (uint32_t i = 0; i < elfFile->getDynamicTable()->getNumberOfDynamics(); i++){
            Dynamic* dyn = elfFile->getDynamicTable()->getDynamic(i);
            uint64_t tag = dyn->GET(d_tag);
            if (tag == DT_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
                tag == DT_VERSYM || tag == DT_VERNEED ||
                tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
                dyn->SET_A(d_ptr,d_un,dyn->GET_A(d_ptr,d_un)-size);
            }
        }
    }

    SectionHeader* textHdr = elfFile->getSectionHeader(lowestTextSectionIdx);
    elfFile->addSection(lowestTextSectionIdx, PebilClassType_TextSection, elfFile->getFileName(), textHdr->GET(sh_name), textHdr->GET(sh_type),
                        textHdr->GET(sh_flags), textHdr->GET(sh_addr)-size, textHdr->GET(sh_offset)-size, size, textHdr->GET(sh_link), 
                        textHdr->GET(sh_info), textHdr->GET(sh_addralign), textHdr->GET(sh_entsize));

    extraTextIdx = lowestTextSectionIdx;
    if (extraDataIdx > lowestTextSectionIdx){
        extraDataIdx++;
    } else {
        __SHOULD_NOT_ARRIVE;
    }

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

    for (uint32_t i = 0; i < disabledFunctions.size(); i++){
        delete[] disabledFunctions[i];
    }

    if (instrumentationData){
        delete[] instrumentationData;
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
        float ratio;
        if (extraTextIdx){
            SectionHeader* extendedText = elfFile->getSectionHeader(extraTextIdx);
        }
        if (extraDataIdx){
            SectionHeader* extendedData = elfFile->getSectionHeader(extraDataIdx);
        }
        if (instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END]){
            uint32_t bytesUsed = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_END]->snippetSize();
        }

    }

    if (HAS_PRINT_CODE(printCodes,Print_Code_Disassemble)){

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
    }
}


ElfFileInst::ElfFileInst(ElfFile* elf, char* inputFuncList){
    currentPhase = ElfInstPhase_no_phase;
    elfFile = elf;
    bloatType = BloatType_undefined;

    // automatically set 2 snippets for the beginning and end of bootstrap code
    instrumentationSnippets.append(new InstrumentationSnippet());
    instrumentationSnippets.append(new InstrumentationSnippet());

    // automatically set the 1st instrumentation point to go to the bootstrap code
    instrumentationPoints.append(NULL);

    extraTextIdx = 0;
    extraDataIdx = elfFile->getDotDataSection()->getSectionIndex();
    dataIdx = 0;

    instSuffix = NULL;
    sharedLibraryPath = NULL;

    lineInfoFinder = NULL;
    if (elfFile->getLineInfoSection()){
        lineInfoFinder = new LineInfoFinder(elfFile->getLineInfoSection());
    }

    uint32_t addrAlign;
    if (elfFile->is64Bit()){
        addrAlign = sizeof(uint64_t);
    } else {
        addrAlign = sizeof(uint32_t);
    }
    DataReference* zeroAddrRef = new DataReference(0, NULL, addrAlign, 0);
    specialDataRefs.append(zeroAddrRef);

    anchorsAreSorted = false;

    if (inputFuncList){
        initializeDisabledFunctions(inputFuncList);
    }

    programEntryBlock = getTextSection()->getBasicBlockAtAddress(elfFile->getFileHeader()->GET(e_entry));

    instrumentationData = NULL;
    instrumentationDataSize = 0;

    uint16_t bssDataIdx = 0;

    /*** apparently some newer pgi compilers use thread-local storage and thus have a .tbss
         that has sh_type = SHT_NOBITS, so this doesn't really work 
    for (uint16_t i = extraDataIdx + 1; i < elfFile->getNumberOfSections(); i++){
        if (elfFile->getSectionHeader(i)->GET(sh_type) == SHT_NOBITS){
            bssDataIdx = i;
        }
    }
    **/
    bssDataIdx = elfFile->findSectionIdx(".bss");


    ASSERT(bssDataIdx == extraDataIdx + 1 && ".data and .bss sections should be adjacent");

    //programDataSize = elfFile->getSectionHeader(extraDataIdx+1)->GET(sh_addr) - elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr);
    programDataSize = elfFile->getSectionHeader(extraDataIdx)->GET(sh_size);
    ASSERT(elfFile->getSectionHeader(bssDataIdx)->GET(sh_type) == SHT_NOBITS);
    programBssSize = elfFile->getSectionHeader(bssDataIdx)->GET(sh_size);

    // find out which is the last (address-wise) symbol to use the the BSS section
    systemReservedBss = 0;

    SymbolTable* dynamicSymtab = elfFile->getDynamicSymbolTable();
    if (dynamicSymtab){
        for (uint32_t i = 0; i < dynamicSymtab->getNumberOfSymbols(); i++){
            Symbol* sym = dynamicSymtab->getSymbol(i);
            if (sym->GET(st_shndx) == bssDataIdx){
                if (systemReservedBss < sym->GET(st_size) + sym->GET(st_value)){
                    systemReservedBss = sym->GET(st_size) + sym->GET(st_value);
                }
            }
        }
    }

    if (systemReservedBss){
        systemReservedBss = systemReservedBss - elfFile->getSectionHeader(bssDataIdx)->GET(sh_addr);
    }

    PRINT_DEBUG_DATA_PLACEMENT("Reserving %d bytes for system in BSS section", systemReservedBss);

    // also add in space to account for empty space between sections caused by alignment constraints on the bss section
    systemReservedBss += elfFile->getSectionHeader(bssDataIdx)->GET(sh_addr) - elfFile->getSectionHeader(extraDataIdx)->GET(sh_addr) - programDataSize;
    PRINT_DEBUG_DATA_PLACEMENT("Reserving %d bytes for sysBSS + alignment", systemReservedBss);

    regStorageOffset = systemReservedBss;
    regStorageReserved = sizeof(uint64_t) * X86_64BIT_GPRS;
    usableDataOffset = regStorageOffset + regStorageReserved;
    
    if (systemReservedBss > programBssSize){
        systemReservedBss = programBssSize;
    }
    ASSERT(systemReservedBss <= programBssSize && "The system cannot reserve more space in BSS than the size of BSS");
}

uint32_t ElfFileInst::addSymbolToDynamicSymbolTable(uint32_t name, uint64_t value, uint64_t size, uint8_t bind, uint8_t type, uint32_t other, uint16_t scnidx){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

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
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

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
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

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
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    uint32_t funcNameOffset = addStringToDynamicStringTable(func->getFunctionName());
    uint32_t symbolIndex = addSymbolToDynamicSymbolTable(funcNameOffset, 0, 0, STB_GLOBAL, STT_FUNC, 0, 0);

    uint64_t relocationOffset = addPLTRelocationEntry(symbolIndex,func->getGlobalDataOffset());
    func->setRelocationOffset(relocationOffset);

    verify();

    return relocationOffset;
}


uint32_t ElfFileInst::addSharedLibraryPath(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    if (!sharedLibraryPath){
        return 0;
    }

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t strOffset = addStringToDynamicStringTable(sharedLibraryPath);

    // add a DT_NEEDED entry to the dynamic table
    uint32_t emptyDynamicIdx = dynamicTable->findEmptyDynamic();

    ASSERT(emptyDynamicIdx < dynamicTable->getNumberOfDynamics() && "No free entries found in the dynamic table");

    // if any DT_RUNPATH entries are present we must use DT_RUNPATH since DT_RPATH entries will be overrun by DT_RUNPATH entries
    // if no DT_RUNPATH are present, we must not use DT_RPATH since using DT_RUNPATH would overrun the DT_RPATH entries
    if (dynamicTable->countDynamics(DT_RUNPATH)){
        dynamicTable->getDynamic(emptyDynamicIdx)->SET(d_tag, DT_RUNPATH);
    } else {
        dynamicTable->getDynamic(emptyDynamicIdx)->SET(d_tag, DT_RPATH);
    }

    dynamicTable->getDynamic(emptyDynamicIdx)->SET_A(d_ptr,d_un,strOffset);

    verify();

    return strOffset;
}

uint32_t ElfFileInst::addSharedLibrary(const char* libname){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

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
