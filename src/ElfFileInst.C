/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <Loop.h>
#include <ProgramHeader.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>
#include <SymbolTable.h>
#include <TextSection.h>

#ifdef BLOAT_MOD
uint32_t bloatCount = 0;
#endif

#define Reserve__Instrumentation_DynamicTable 0x8000

BasicBlock* ElfFileInst::initInterposeBlock(FlowGraph* fg, uint32_t bbsrcidx, uint32_t bbtgtidx){
    BasicBlock* bb = new BasicBlock(fg->getNumberOfBasicBlocks(), fg);
    interposedBlocks.append(bb);

    X86Instruction* jumpToTarget = X86InstructionFactory::emitJumpRelative(0,0);
    bb->addInstruction(jumpToTarget);

    // store the source and target indices in the source/target bitsets
    bb->addSourceBlock(fg->getBasicBlock(bbsrcidx));
    bb->addTargetBlock(fg->getBasicBlock(bbtgtidx));

    jumpToTarget->setLeader(true);
    jumpToTarget->setContainer(fg->getFunction());
    jumpToTarget->setIndex(0);

    fg->getFunction()->setManipulated();

    return bb;
}

BasicBlock* ElfFileInst::findExposedBasicBlock(HashCode hashCode){
    //    PRINT_INFOR("Solving hashcode %d %d %d %d", hashCode.getSection(), hashCode.getFunction(), hashCode.getBlock(), hashCode.getInstruction());
    for (uint32_t i = 0; i < exposedBasicBlocks.size(); i++){
        HashCode blockHash = exposedBasicBlocks[i]->getHashCode();
        //  PRINT_INFOR("\t\tblock %d %d %d %d", blockHash.getSection(), blockHash.getFunction(), blockHash.getBlock(), blockHash.getInstruction());
        if (blockHash.getSection() == hashCode.getSection()){
            if (blockHash.getFunction() == hashCode.getFunction()){
                if (blockHash.getBlock() == hashCode.getBlock()){
                    return exposedBasicBlocks[i];
                }
            }
        }
    }
    return NULL;
}

Vector<X86Instruction*>* ElfFileInst::findAllCalls(char* names){
    char* fnames = new char[strlen(names)+1];
    memcpy(fnames, names, strlen(names));
    uint32_t len = strlen(names);
    fnames[len] = '\0';

    Vector<uint32_t> fstart;
    fstart.append(0);
    for (uint32_t i = 0; i < len; i++){
        if (fnames[i] == ':'){
            fnames[i] = '\0';
            fstart.append(i+1);
        }
    }

    Vector<X86Instruction*>* calls = new Vector<X86Instruction*>();
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();

        if (instruction->isFunctionCall()){
            Symbol* functionSymbol = elfFile->lookupFunctionSymbol(instruction->getTargetAddress());

            if (functionSymbol){
                for (uint32_t j = 0; j < fstart.size(); j++){
                    if (!strcmp(fnames + fstart[j], functionSymbol->getSymbolName())){
                        (*calls).append(instruction);
                        break;
                    }
                }
            }
        }
    }
    delete[] fnames;
    return calls;
}

void ElfFileInst::extendDynamicTable(){
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    uint32_t oldDynamicIdx = dynamicTable->getSectionIndex();
    uint32_t oldDynamicSize = dynamicTable->getSizeInBytes();
    SectionHeader* dynTableHdr = dynamicTable->getSectionHeader();
    uint64_t oldDynamicOffset = dynTableHdr->GET(sh_offset);
    uint64_t oldDynamicAddress = dynTableHdr->GET(sh_addr);
    SectionHeader* finalHeader = elfFile->getSectionHeader(elfFile->getNumberOfSections() - 1);
    ProgramHeader* dHdr = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());
    uint64_t usableOffset = nextAlignAddress(finalHeader->GET(sh_offset) + finalHeader->GET(sh_size), dHdr->GET(p_align));

    dynTableHdr->INCREMENT(sh_size, dynamicTable->extendTable(instrumentationFunctions.size() + 5));
    ASSERT(dynamicTable->getSizeInBytes() <= 0x8000);

    uint64_t newDynamicAddress = dynamicTableReserved;
    dynTableHdr->SET(sh_addr, newDynamicAddress);
    dynTableHdr->SET(sh_offset, usableOffset);

    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
            Symbol* symbol = symbolTable->getSymbol(j);
            if (symbol->GET(st_value) == oldDynamicAddress){
                symbol->SET(st_value, newDynamicAddress);
            }
        }
    }

    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* programHeader = elfFile->getProgramHeader(i);
        if (programHeader->GET(p_type) == PT_DYNAMIC){
            ASSERT(programHeader->GET(p_vaddr) == oldDynamicAddress);
            programHeader->SET(p_vaddr, newDynamicAddress);
            programHeader->SET(p_paddr, newDynamicAddress);
            programHeader->SET(p_offset, usableOffset);
            programHeader->SET(p_filesz, dynamicTable->getSizeInBytes());
            programHeader->SET(p_memsz, dynamicTable->getSizeInBytes());
        }
    }

    // add a new data section, which will be swapped with the grown dynamic section
    SectionHeader* genericDataHdr = elfFile->getSectionHeader(elfFile->findSectionIdx(".data"));
    ASSERT(genericDataHdr);

    uint32_t newDynamicIdx = elfFile->getNumberOfSections();
    elfFile->addSection(newDynamicIdx, PebilClassType_DataSection, elfFile->getFileName(), genericDataHdr->GET(sh_name), dynTableHdr->GET(sh_type),
                        dynTableHdr->GET(sh_flags), oldDynamicAddress, oldDynamicOffset, 0, dynTableHdr->GET(sh_link),
                        dynTableHdr->GET(sh_info), dynTableHdr->GET(sh_addralign), dynTableHdr->GET(sh_entsize));

    elfFile->swapSections(newDynamicIdx, oldDynamicIdx);
    ((DataSection*)elfFile->getRawSection(oldDynamicIdx))->extendSize(oldDynamicSize);
    elfFile->getSectionHeader(oldDynamicIdx)->SET(sh_size, oldDynamicSize);

    for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
        elfFile->getSectionHeader(i)->setIndex(i);
        if (elfFile->getSectionHeader(i)->GET(sh_link) == oldDynamicIdx){
            elfFile->getSectionHeader(i)->SET(sh_link, newDynamicIdx);
        }
        elfFile->getRawSection(i)->setSectionIndex(i);
    }
}

void ElfFileInst::setInstExtension(char* extension){
    ASSERT(!instSuffix);
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix, "%s\0", extension);
}

BasicBlock* ElfFileInst::getProgramExitBlock(){
    TextSection* fini = getDotFiniSection();
    ASSERT(fini && "Cannot find fini section");
    return ((Function*)fini->getTextObject(0))->getFlowGraph()->getBasicBlock(0);
}

void ElfFileInst::extendDataSection(){
    if (instrumentationDataSize){
        char* tmpData = new char[instrumentationDataSize + DATA_EXTENSION_INC];
        memcpy(tmpData, instrumentationData, instrumentationDataSize);
        bzero(tmpData + instrumentationDataSize, DATA_EXTENSION_INC);
        delete[] instrumentationData;
        instrumentationData = tmpData;
    } else {
        ASSERT(!instrumentationData);
        instrumentationData = new char[DATA_EXTENSION_INC];
        bzero(instrumentationData, DATA_EXTENSION_INC);
    }

    instrumentationDataSize += DATA_EXTENSION_INC;
    verify();
}

void ElfFileInst::buildInstrumentationSections(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    verify();

    FileHeader* fileHeader = elfFile->getFileHeader();
    SectionHeader* finalHeader = elfFile->getSectionHeader(elfFile->getNumberOfSections() - 1);

    SectionHeader* genericDataHdr = elfFile->getSectionHeader(elfFile->findSectionIdx(".data"));
    ASSERT(genericDataHdr);

    uint64_t lowestTextAddress = -1;
    uint16_t lowestTextSectionIdx = -1;

    ProgramHeader* textHeader = elfFile->getProgramHeader(elfFile->getTextSegmentIdx());
    
    // find the address of the first text section
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

    ProgramHeader* pHdr = elfFile->getProgramHeaderPHDR();
    uint32_t phdrAlign = 1;
    if (pHdr){
        phdrAlign = pHdr->GET(p_align);
    }
    SectionHeader* genericTextHdr = elfFile->getSectionHeader(lowestTextSectionIdx);
    ProgramHeader* dHdr = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());
    uint64_t usableAddress = dynamicTableReserved;
    uint64_t usableOffset = finalHeader->GET(sh_offset);
    if (!elfFile->isStaticLinked()){
        ASSERT(finalHeader->GET(sh_type) == SHT_DYNAMIC);
    } else {
        usableOffset += finalHeader->GET(sh_size);
	usableAddress = nextAlignAddress(usableAddress, dHdr->GET(p_align));
	usableOffset = nextAlignAddress(usableOffset, dHdr->GET(p_align));
    }

    // add the instrumentation segment
    //    PRINT_INFOR("usable address %#llx, align %x", usableAddress, dHdr->GET(p_align));
    ASSERT(usableAddress % dHdr->GET(p_align) == usableOffset % dHdr->GET(p_align));
    instSegment = elfFile->addSegment(DEFAULT_INST_SEGMENT_IDX, dHdr->GET(p_type), usableOffset, usableAddress,
                                      usableAddress, TEMP_SEGMENT_SIZE, TEMP_SEGMENT_SIZE, PF_R | PF_W | PF_X, dHdr->GET(p_align));


    if (!elfFile->isStaticLinked()){
        usableAddress += Reserve__Instrumentation_DynamicTable;
        usableOffset += Reserve__Instrumentation_DynamicTable;
    }

    // add the instrumentation data section
    extraDataIdx = elfFile->getNumberOfSections();
    elfFile->addSection(extraDataIdx, PebilClassType_DataSection, elfFile->getFileName(), genericDataHdr->GET(sh_name), genericDataHdr->GET(sh_type),
                        genericDataHdr->GET(sh_flags), usableAddress, usableOffset, instrumentationDataSize, genericDataHdr->GET(sh_link),
                        genericDataHdr->GET(sh_info), genericDataHdr->GET(sh_addralign), genericDataHdr->GET(sh_entsize));

    elfFile->getSectionHeader(extraDataIdx)->SET(sh_addr, usableAddress);
    ASSERT(elfFile->getRawSection(extraDataIdx)->getType() == PebilClassType_DataSection);
    usableAddress = nextAlignAddress(usableAddress + instrumentationDataSize, genericTextHdr->GET(sh_addralign));
    usableOffset = nextAlignAddress(usableOffset + instrumentationDataSize, genericTextHdr->GET(sh_addralign));

    // add the instrumentation text section
    extraTextIdx = extraDataIdx + 1;
    elfFile->addSection(extraTextIdx, PebilClassType_TextSection, elfFile->getFileName(), genericTextHdr->GET(sh_name), genericTextHdr->GET(sh_type),
                        genericTextHdr->GET(sh_flags), usableAddress, usableOffset, TEMP_SEGMENT_SIZE - instrumentationDataSize, genericTextHdr->GET(sh_link),
                        genericTextHdr->GET(sh_info), genericTextHdr->GET(sh_addralign), genericTextHdr->GET(sh_entsize));

    SectionHeader* instDataHeader = elfFile->getSectionHeader(extraDataIdx);
    SectionHeader* instTextHeader = elfFile->getSectionHeader(extraTextIdx);
    ASSERT(instDataHeader && elfFile->getRawSection(extraDataIdx)->getType() == PebilClassType_DataSection);

    ((DataSection*)elfFile->getRawSection(extraDataIdx))->extendSize(instrumentationDataSize - DATA_EXTENSION_INC);
    applyInstrumentationDataToRaw();

    verify();
    ASSERT(elfFile->getRawSection(extraDataIdx)->charStream());

    return;
}

void ElfFileInst::applyInstrumentationDataToRaw(){
    ((DataSection*)elfFile->getRawSection(extraDataIdx))->setBytesAtOffset(0, instrumentationDataSize, instrumentationData);
}

void ElfFileInst::compressInstrumentation(uint32_t textSize){
    ASSERT(textSize < TEMP_SEGMENT_SIZE && "TEMP_SEGMENT_SIZE was not set large enough");

    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size, textSize);
    ((TextSection*)elfFile->getRawSection(extraTextIdx))->setSizeInBytes(textSize);

    instSegment->SET(p_memsz, Reserve__Instrumentation_DynamicTable + instrumentationDataSize + textSize);
    instSegment->SET(p_filesz, Reserve__Instrumentation_DynamicTable + instrumentationDataSize + textSize);
    
}

bool ElfFileInst::isDisabledFunction(Function* func){
    bool white = (*disabledFunctions).size() && (*disabledFunctions)[0][0]=='*';
    for (uint32_t i = 0 + white; i < (*disabledFunctions).size(); i++){
        if (!strcmp(func->getName(), (*disabledFunctions)[i])){
            return true ^ white;
        }
    }
    return false ^ white;
}

uint32_t ElfFileInst::initializeReservedData(uint64_t address, uint32_t size, void* data){
    char* bytes = (char*)data;

    if (address + size > getInstDataAddress() + usableDataOffset ||
        address < getInstDataAddress()){
        PRINT_INFOR("address range %#llx+%d out of range of data section [%#llx,%#llx)", address, size, getInstDataAddress(),
                    getInstDataAddress() + usableDataOffset);
    }
    ASSERT(address + size <= getInstDataAddress() + usableDataOffset && address >= getInstDataAddress() &&
           "Data initialization address out of range, you should reserve the data first");

    memcpy(instrumentationData + address - getInstDataAddress(), bytes, size);

    return size;
}

bool ElfFileInst::isEligibleFunction(Function* func){
    if (!strcmp("_start", func->getName())){
        return true;
    }
    if (!strcmp("_fini", func->getName())){
        return true;
    }
    if (!canRelocateFunction(func)){
        return false;
    }
    if (func->isInstrumentationFunction()){
        return false;
    }
    /*
    if (!func->containsReturn()){
        return false;
    }
    if (func->isJumpTable()){
        return false;
    }
    */
    if (isDisabledFunction(func)){
        return false;
    }
    if (func->getNumberOfBytes() < Size__uncond_jump){
        return false;
    }

    return true;
}

uint32_t ElfFileInst::relocateAndBloatFunction(Function* operatedFunction, uint64_t offsetToRelocation, Vector<Vector<InstrumentationPoint*>*>* functionInstPoints){
    //    ASSERT(isEligibleFunction(operatedFunction) && operatedFunction->hasCompleteDisassembly());

    TextSection* extraText = (TextSection*)elfFile->getRawSection(extraTextIdx);
    TextSection* text = operatedFunction->getTextSection();
    uint64_t relocationAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + offsetToRelocation;
    uint32_t functionSize = operatedFunction->getNumberOfBytes();
    Vector<X86Instruction*>* trampEmpty = new Vector<X86Instruction*>();

    uint32_t currentByte = 0;

    X86Instruction* connector = X86InstructionFactory::emitJumpRelative(operatedFunction->getBaseAddress(), relocationAddress);
    connector->initializeAnchor(operatedFunction->getFlowGraph()->getBasicBlock(0)->getLeader());
    
    (*trampEmpty).append(connector);
    currentByte += (*trampEmpty).back()->getSizeInBytes();
    (*trampEmpty).back()->setBaseAddress(operatedFunction->getBaseAddress());

    if (currentByte > operatedFunction->getNumberOfBytes()){
        PRINT_WARN(5, "Function %s at address %#llx is only %d bytes -- cannot relocate or fully instrument", operatedFunction->getName(), operatedFunction->getBaseAddress(), operatedFunction->getNumberOfBytes());
        for (uint32_t i = 0; i < (*trampEmpty).size(); i++){
            delete (*trampEmpty)[i];
        }
        delete trampEmpty;
        return operatedFunction->getNumberOfBytes();
    }
    ASSERT(currentByte <= operatedFunction->getNumberOfBytes() && "Function is not big enough to relocate");

    Function* placeHolder = new Function(text, operatedFunction->getIndex(), operatedFunction->getFunctionSymbol(), functionSize);

    Vector<AddressAnchor*>* modAnchors = elfFile->searchAddressAnchors(operatedFunction->getBaseAddress());
    for (uint32_t i = 0; i < modAnchors->size(); i++){
        (*modAnchors)[i]->updateLink((*trampEmpty).back());
        elfFile->setAnchorsSorted(false);
    }
    delete modAnchors;

    // we dont want the above search to find this anchor, so we add after the search is done
    (*(elfFile->getAddressAnchors())).append(connector->getAddressAnchor());

#ifdef FILL_RELOCATED_WITH_INTERRUPTS
    while (currentByte < functionSize){
        (*trampEmpty).append(X86InstructionFactory::emitInterrupt(X86TRAPCODE_BREAKPOINT));
        (*trampEmpty).back()->setBaseAddress(operatedFunction->getBaseAddress() + currentByte);
        currentByte += (*trampEmpty).back()->getSizeInBytes();
    }
#endif

    placeHolder->generateCFG(trampEmpty, elfFile->getAddressAnchors());
    delete trampEmpty;

    Function* displacedFunction = text->replaceFunction(operatedFunction->getIndex(), placeHolder);
    relocatedFunctions.append(displacedFunction);
    relocatedFunctionOffsets.append(offsetToRelocation);
    ASSERT(relocatedFunctions.size() == relocatedFunctionOffsets.size());

    uint64_t oldBase = displacedFunction->getBaseAddress();
    uint64_t oldSize = displacedFunction->getSizeInBytes();

    if (displacedFunction->getIndex() < allFunctions.size() - 1){
        Function* nextFunc = allFunctions[displacedFunction->getIndex() + 1];
        X86Instruction* firstI = nextFunc->getFlowGraph()->getBasicBlock(0)->getLeader();
        X86Instruction* safetyJump;
        if (elfFile->is64Bit()){
            safetyJump = X86InstructionFactory64::emitJumpRelative(0,0);
        } else {
            safetyJump = X86InstructionFactory32::emitJumpRelative(0,0);
        }
        safetyJump->initializeAnchor(firstI);
        (*(elfFile->getAddressAnchors())).append(safetyJump->getAddressAnchor());
        displacedFunction->addSafetyJump(safetyJump);
    }

    // adjust the base addresses of the relocated function
    displacedFunction->setBaseAddress(relocationAddress);

    // bloat the blocks in the function
    bool doBloat = true;
#ifdef TURNOFF_FUNCTION_BLOAT
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
        displacedFunction->bloatBasicBlocks(functionInstPoints);
        elfFile->setAnchorsSorted(false);
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
uint32_t ElfFileInst::generateInstrumentation(){
#ifdef VALIDATE_ANCHOR_SEARCH
    PRINT_INFOR("Validating anchor search, this can cause much longer instrumentation times, see VALIDATE_ANCHOR_SEARCH in %s", __FILE__);
#else
    PRINT_WARN(4, "Not validating anchor search, if problems are encountered try enabling VALIDATE_ANCHOR_SEARCH in %s", __FILE__);
#endif

    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    TextSection* textSection = getDotTextSection();
    TextSection* pltSection = getDotPltSection();

    uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);

    InstrumentationSnippet* snip = instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN];
    snip->setRequiresDistinctTrampoline(true);
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        snip->addSnippetInstruction(X86InstructionFactory32::emitStackPush(i));
    }

    uint64_t codeOffset = relocatedTextSize;

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];

        if (func){
            PRINT_DEBUG_INST("Setting InstrumentationFunction %d PLT offset to %#llx", i, codeOffset);
            func->setProcedureLinkOffset(codeOffset);
            codeOffset += func->procedureLinkReservedSize();
            
            PRINT_DEBUG_INST("Setting InstrumentationFunction %d Wrapper offset to %#llx", i, codeOffset);
            func->setWrapperOffset(codeOffset);
            codeOffset += func->wrapperReservedSize();

            DEBUG_ANCHOR(func->print();)
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
        snip->addSnippetInstruction(X86InstructionFactory32::emitStackPop(X86_32BIT_GPRS-i-1));
    }
    snip->addSnippetInstruction(X86InstructionFactory::emitReturn());

    codeOffset += snip->snippetSize();

    uint64_t returnOffset = 0;
    uint64_t chainOffset = 0;

    (*instrumentationPoints).sort(compareInstBaseAddress);

    PRINT_INFOR("There are %d instrumentation points", (*instrumentationPoints).size());

    Vector<AddressAnchor*> anchors;
    Vector<X86Instruction*> updates;

    for (uint32_t i = 0; i < (*instrumentationPoints).size(); i++){
        InstrumentationPoint* pt = (*instrumentationPoints)[i];

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
            X86Instruction* ins = pt->getSourceObject();
#ifdef SWAP_FUNCTION_ONLY
            if (strstr(ins->getContainer()->getName(), SWAP_FUNCTION_ONLY)){
#endif
                PRINT_INFOR("Performing instruction swap at for point (%d/%d) %#llx in %s", i, (*instrumentationPoints).size(), pt->getSourceObject()->getProgramAddress(), ins->getContainer()->getName());
                performSwap = true;
#ifdef SWAP_FUNCTION_ONLY
            }
#endif
        }
#endif

        uint64_t textBaseAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr);
            
        bool stackIsSafe = false;
        ASSERT(pt->getSourceObject()->getContainer()->getType() == PebilClassType_Function);
        Function* f = (Function*)pt->getSourceObject()->getContainer();
        BasicBlock* bb = f->getBasicBlockAtAddress(pt->getInstSourceAddress());
        if (!f->hasLeafOptimization() && bb && !bb->isEntry()){
            stackIsSafe = true;
        }
        if (pt->getInstrumentation()->getType() == PebilClassType_InstrumentationFunction &&
            ((InstrumentationFunction*)pt->getInstrumentation())->hasSkipWrapper()){
            stackIsSafe = true;
        } 
        uint64_t registerStorage = getInstDataAddress() + regStorageOffset;

        if (performSwap){
            if (!pt->getInstBaseAddress() || !pt->getInstSourceAddress()){
                PRINT_WARN(4,"Could not find a place to instrument for point at %#llx", pt->getSourceObject()->getBaseAddress());
                continue;
            }
            PRINT_DEBUG_INST("Generating code for InstrumentationPoint %d at address %llx", i, pt->getInstBaseAddress());
            PRINT_DEBUG_POINT_CHAIN("Examining instrumentation point %d at %#llx in function %s", i, pt->getInstBaseAddress(), f->getName());
            
            Vector<X86Instruction*>* repl = NULL;
            Vector<X86Instruction*>* displaced = NULL;
            
            repl = new Vector<X86Instruction*>();

            if ((*instrumentationPoints)[i]->getInstrumentationMode() == InstrumentationMode_tramp ||
                (*instrumentationPoints)[i]->getInstrumentationMode() == InstrumentationMode_trampinline){
                uint64_t instAddress = pt->getInstSourceAddress();
                (*repl).append(X86InstructionFactory::emitJumpRelative(instAddress, elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + codeOffset));
            } else if ((*instrumentationPoints)[i]->getInstrumentationMode() == InstrumentationMode_inline){
                if ((*instrumentationPoints)[i]->getFlagsProtectionMethod() == FlagsProtectionMethod_light){
                    if (elfFile->is64Bit()){
                        (*repl).append(X86InstructionFactory64::emitMoveRegToMem(X86_REG_AX, registerStorage));
                        (*repl).append(X86InstructionFactory64::emitLoadAHFromFlags());
                        while ((*instrumentationPoints)[i]->getInstrumentation()->hasMoreCoreInstructions()){
                            (*repl).append((*instrumentationPoints)[i]->getInstrumentation()->removeNextCoreInstruction());
                        }
                        (*repl).append(X86InstructionFactory64::emitStoreAHToFlags());
                        (*repl).append(X86InstructionFactory64::emitMoveMemToReg(registerStorage, X86_REG_AX, true));
                    } else { 
                        (*repl).append(X86InstructionFactory32::emitMoveRegToMem(X86_REG_AX, registerStorage));
                        (*repl).append(X86InstructionFactory32::emitLoadAHFromFlags());
                        while ((*instrumentationPoints)[i]->getInstrumentation()->hasMoreCoreInstructions()){
                            (*repl).append((*instrumentationPoints)[i]->getInstrumentation()->removeNextCoreInstruction());
                        }
                        (*repl).append(X86InstructionFactory32::emitStoreAHToFlags());
                        (*repl).append(X86InstructionFactory32::emitMoveMemToReg(registerStorage, X86_REG_AX));
                    }                
                } else if ((*instrumentationPoints)[i]->getFlagsProtectionMethod() == FlagsProtectionMethod_full){
                    (*repl).append(X86InstructionFactory::emitPushEflags());
                    while ((*instrumentationPoints)[i]->getInstrumentation()->hasMoreCoreInstructions()){
                        (*repl).append((*instrumentationPoints)[i]->getInstrumentation()->removeNextCoreInstruction());
                    }
                    (*repl).append(X86InstructionFactory::emitPopEflags());                    
                } else { // (*instrumentationPoints)[i]->getFlagsProtectionMethod() == FlagsProtectionMethod_none
                    while ((*instrumentationPoints)[i]->getInstrumentation()->hasMoreCoreInstructions()){
                        (*repl).append((*instrumentationPoints)[i]->getInstrumentation()->removeNextCoreInstruction());
                    }
                }
            } else {
                PRINT_ERROR("This instrumentation mode (%d) not supported", (*instrumentationPoints)[i]->getInstrumentationMode());
            }
            
            // disassemble the newly minted instructions
            uint32_t bytesUsed = 0;
            for (uint32_t j = 0; j < (*repl).size(); j++){
                (*repl)[j]->setBaseAddress(pt->getInstSourceAddress()+bytesUsed);
                bytesUsed += (*repl)[j]->getSizeInBytes();
                
                DEBUG_INST((*repl)[j]->print();)
            }

            ASSERT((*repl).size());
            
            displaced = pt->swapInstructionsAtPoint(repl);
            ASSERT((*repl).size());
            ASSERT((*displaced).size());

            // update any address anchor that pointed to the old instruction to point to the new
            if ((*displaced).size()){
                if ((*repl).size() && (*repl)[0]->getBaseAddress() == (*displaced)[0]->getBaseAddress()){
                    Vector<AddressAnchor*>* modAnchors = elfFile->searchAddressAnchors((*displaced)[0]->getBaseAddress());
                    PRINT_DEBUG_ANCHOR("Looking for anchors for address %#llx", (*displaced)[0]->getBaseAddress());
                    for (uint32_t k = 0; k < modAnchors->size(); k++){
                        X86Instruction* modInst = (*repl)[0];
                        anchors.append((*modAnchors)[k]);
                        updates.append(modInst);
                    }
                    delete modAnchors;
                }
            }
            

            returnOffset = pt->getInstSourceAddress() - elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + (*repl)[0]->getSizeInBytes();

            if (pt->getInstLocation() == InstLocation_replace){
                while ((*displaced).size()){
                    replacedInstructions.append((*displaced).remove(0));
                }
            }

            if (pt->getInstrumentationMode() != InstrumentationMode_inline){
                pt->generateTrampoline(displaced, textBaseAddress, codeOffset, returnOffset, true, registerStorage, stackIsSafe, codeOffset);
            } else {
                for (uint32_t k = 0; k < (*displaced).size(); k++){
                    delete (*displaced)[k];
                }
            }
            codeOffset += pt->sizeNeeded();
            if (repl){
                delete repl;
            }
            if (displaced){
                delete displaced;
            }
        }
    }

    ASSERT(anchors.size() == updates.size());
    while (anchors.size()){
        AddressAnchor* modAnchor = anchors.remove(0);
        X86Instruction* update = updates.remove(0);
        modAnchor->updateLink(update);
        elfFile->setAnchorsSorted(false);
    }
    ASSERT(!updates.size());

    for (uint32_t i = INST_SNIPPET_BOOTSTRAP_END + 1; i < instrumentationSnippets.size(); i++){        
        snip = instrumentationSnippets[i];
        if (snip){
            snip->setBootstrapOffset(codeOffset);
            codeOffset += snip->bootstrapSize();
            ASSERT(snip->bootstrapSize() == 0 && "Snippets should not require bootstrap code");
        }
    }

    if (codeOffset > elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)){
        PRINT_INFOR("code used %#llx bytes > available space %#llx bytes", codeOffset, elfFile->getSectionHeader(extraTextIdx)->GET(sh_size));
    }
    ASSERT(codeOffset <= elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) && "Not enough space in the text section to accomodate the extra code");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (func){
            DEBUG_INST(func->print();)

            func->generateGlobalData(textBaseAddress);
            func->generateWrapperInstructions(textBaseAddress, getInstDataAddress(), fxStorageOffset);
            func->generateBootstrapInstructions(textBaseAddress, getInstDataAddress());

            // plt section only exists in dynamic binary, and for static binary we don't use this value
            uint64_t realPLTaddr = 0;
            if (!elfFile->isStaticLinked()){
                realPLTaddr = pltSection->getBaseAddress();
            }
            func->generateProcedureLinkInstructions(textBaseAddress, getInstDataAddress(), realPLTaddr);
        }
    }

    for (uint32_t i = 0; i < (*(elfFile->getAddressAnchors())).size(); i++){
        DEBUG_ANCHOR((*(elfFile->getAddressAnchors()))[i]->print();)
    }
    PRINT_DEBUG_ANCHOR("Still have %d anchors", (*(elfFile->getAddressAnchors())).size());

    return codeOffset;

}


void ElfFileInst::setPathToInstLib(char* libPath){
    if (sharedLibraryPath){
        PRINT_WARN(4,"Overwriting shared library path");
        delete[] sharedLibraryPath;
    }
    sharedLibraryPath = new char[__MAX_STRING_SIZE];
    sprintf(sharedLibraryPath, "%s\0", libPath);
}

TextSection* ElfFileInst::getInstTextSection() { return (TextSection*)(elfFile->getRawSection(extraTextIdx)); }
RawSection* ElfFileInst::getInstDataSection() { return elfFile->getRawSection(extraDataIdx); }
uint64_t ElfFileInst::getInstDataAddress() { return instrumentationDataAddress; }

uint64_t ElfFileInst::reserveDataOffset(uint64_t size){
    ASSERT(currentPhase > ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    ASSERT(currentPhase <= ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    uint64_t nextOffset = nextAlignAddress(usableDataOffset + size, sizeof(uint64_t));
    while (nextOffset  >= instrumentationDataSize){
        extendDataSection();
    }
    ASSERT(nextOffset < instrumentationDataSize && "Not enough space for the requested data");

    uint64_t avail = usableDataOffset;
    usableDataOffset = nextOffset;
    return avail;
}

uint32_t ElfFileInst::addInstrumentationSnippet(InstrumentationSnippet* snip){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    instrumentationSnippets.append(snip);
    return instrumentationSnippets.size();
}

TextSection* ElfFileInst::getDotTextSection(){
    return elfFile->getDotTextSection();
}
TextSection* ElfFileInst::getDotFiniSection(){
    return elfFile->getDotFiniSection();
}
TextSection* ElfFileInst::getDotInitSection(){
    return elfFile->getDotInitSection();
}
TextSection* ElfFileInst::getDotPltSection(){
    return elfFile->getDotPltSection();
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

uint64_t ElfFileInst::functionRelocateAndTransform(uint32_t offset){
    TIMER(double t1 = timer(), t2; char stepNumber = '1');

    uint64_t codeOffset = offset;
    uint32_t numberOfFunctions = exposedFunctions.size();

    for (uint32_t i = 0; i < interposedBlocks.size(); i++){
        BasicBlock* bb = interposedBlocks[i];
        bb->getFlowGraph()->getFunction()->interposeBlock(bb);
        exposedBasicBlocks.append(bb);
    }

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

    uint32_t skippedRelocation = 0;
    if (!HAS_INSTRUMENTOR_FLAG(InstrumentorFlag_norelocate, flags)){

        exposedFunctions.sort(compareBaseAddress);
        exposedBasicBlocks.sort(compareBaseAddress);
        (*instrumentationPoints).sort(compareInstFuncBaseAddress);

        ASSERT(exposedFunctions.isSorted(compareBaseAddress));
        ASSERT(exposedBasicBlocks.isSorted(compareBaseAddress));
        ASSERT((*instrumentationPoints).isSorted(compareInstFuncBaseAddress));

        Vector<Vector<Vector<InstrumentationPoint*>*>*>* instPointsPerBlock = new Vector<Vector<Vector<InstrumentationPoint*>*>*>();

        for (uint32_t i = 0; i < exposedFunctions.size(); i++){
            (*instPointsPerBlock).append(new Vector<Vector<InstrumentationPoint*>*>());
            for (uint32_t j = 0; j < exposedBasicBlocks.size(); j++){
                if (exposedFunctions[i]->getBaseAddress() == exposedBasicBlocks[j]->getFunction()->getBaseAddress()){
                    (*instPointsPerBlock)[i]->append(new Vector<InstrumentationPoint*>());
                }
            }
            ASSERT((*instPointsPerBlock)[i]->size() == exposedFunctions[i]->getNumberOfBasicBlocks());
        }

        bool* needsRelocate = new bool[exposedFunctions.size()];
        for (uint32_t i = 0; i < exposedFunctions.size(); i++){
            needsRelocate[i] = false;
            if (exposedFunctions[i]->isManipulated()){
                needsRelocate[i] = true;
            }
        }

        uint32_t currentFunc = 0;
        uint32_t localBlock = 0;
        for (uint32_t k = 0; k < (*instrumentationPoints).size(); k++){
            ASSERT((*instrumentationPoints)[k]->getSourceObject()->getContainer()->getType() == PebilClassType_Function);
            Function* pointsFunction = (Function*)(*instrumentationPoints)[k]->getSourceObject()->getContainer();
            while (exposedFunctions[currentFunc]->getBaseAddress() < pointsFunction->getBaseAddress()){
                currentFunc++;
                localBlock = 0;
            }
            while (!exposedFunctions[currentFunc]->getFlowGraph()->getBasicBlock(localBlock)->inRange((*instrumentationPoints)[k]->getInstBaseAddress())){
                //PRINT_INFOR("local block %d -- %#llx", localBlock, exposedFunctions[currentFunc]->getFlowGraph()->getBasicBlock(localBlock)->getBaseAddress());
                localBlock++;
            }
            (*((*instPointsPerBlock)[currentFunc]))[localBlock]->append((*instrumentationPoints)[k]);
            if ((*instrumentationPoints)[k]->getInstLocation() != InstLocation_replace){
                needsRelocate[currentFunc] = true;
            }
        }

        (*instrumentationPoints).sort(compareInstBaseAddress);
        /*
        uint32_t currentFunction = 0;
        for (uint32_t i = 0; i < exposedBasicBlocks.size(); i++){
            while (exposedFunctions[currentFunction]->getBaseAddress() + exposedFunctions[currentFunction]->getSizeInBytes() - 1
                   < exposedBasicBlocks[i]->getBaseAddress()){
                currentFunction++;
                ASSERT(currentFunction < exposedFunctions.size());
            }
            if (exposedFunctions[currentFunction]->getBaseAddress() == exposedBasicBlocks[i]->getFunction()->getBaseAddress()){
            }
        }
        ASSERT(currentFunction == exposedFunctions.size()-1);

        currentFunction = 0;
        uint32_t currentBlock = 0;
        uint32_t localBlock = 0;

        for (uint32_t i = 0; i < (*instrumentationPoints).size(); i++){
            while (exposedBasicBlocks[currentBlock]->getBaseAddress() + exposedBasicBlocks[currentBlock]->getNumberOfBytes() - 1
                   < (*instrumentationPoints)[i]->getInstBaseAddress()){
                currentBlock++;
                ASSERT(currentBlock < exposedBasicBlocks.size());
                localBlock++;
            }
            while (exposedFunctions[currentFunction]->getBaseAddress() + exposedFunctions[currentFunction]->getNumberOfBytes() - 1
                   < (*instrumentationPoints)[i]->getInstBaseAddress()){
                currentFunction++;
                ASSERT(currentFunction < exposedFunctions.size());
                localBlock = 0;
            }
            ASSERT(exposedFunctions[currentFunction]->inRange(exposedBasicBlocks[currentBlock]->getBaseAddress()));
            ASSERT(exposedBasicBlocks[currentBlock]->inRange((*instrumentationPoints)[i]->getInstBaseAddress()));
            (*instrumentationPoints)[i]->print();
            if ((*instrumentationPoints)[i]->getInstLocation() != InstLocation_replace){
                needsRelocate[currentFunction] = true;
            }
        }
        */


        for (uint32_t i = 0; i < numberOfFunctions; i++){
            Function* func = exposedFunctions[i];
            /*
            if (!isEligibleFunction(func)){
                func->print();
                __SHOULD_NOT_ARRIVE;
            }
            if (!func->hasCompleteDisassembly()){
                func->print();
                __SHOULD_NOT_ARRIVE;
            }
            */
#ifdef RELOC_MOD
            if (i % RELOC_MOD == RELOC_MOD_OFF){
                PRINT_INFOR("relocating function (%d) %s", i, func->getName());
#endif
                //                ASSERT(isEligibleFunction(func) && func->hasCompleteDisassembly());
                if (needsRelocate[i]){
                    codeOffset += relocateAndBloatFunction(func, codeOffset, (*instPointsPerBlock)[i]);
                    func->setRelocated();
                } else {
                    func->setBaseAddress(func->getBaseAddress());
                    skippedRelocation++;
                }
#ifdef RELOC_MOD
            }
#endif
        }
        delete[] needsRelocate;

        while (instPointsPerBlock->size()){
            Vector<Vector<InstrumentationPoint*>*>* tmp = (*instPointsPerBlock).remove(0);
            while (tmp->size()){
                delete (*tmp).remove(0);
            }
            delete tmp;
        }
        delete instPointsPerBlock;
    }
    if (skippedRelocation){
        PRINT_INFOR("Skipped relocation on %d/%d functions", skippedRelocation, numberOfFunctions);
    }

    TIMER(t2 = timer();PRINT_INFOR("___timer: \t\tFncReloc Step %c Reloc : %.2f seconds",stepNumber++,t2-t1);t1=t2);

    // update address anchors modified by the tranformation (things anchored to the
    // instructions at the front of blocks)
    for (uint32_t i = 0; i < (*(elfFile->getAddressAnchors())).size(); i++){
        (*(elfFile->getAddressAnchors()))[i]->refreshCache();
    }

    Vector<Vector<AddressAnchor*>*> anchors;
    Vector<BasicBlock*> blocks;
    for (uint32_t j = 0; j < (*instrumentationPoints).size(); j++){
        if ((*instrumentationPoints)[j]->getSourceObject()->isLeader()){
            uint64_t searchAddr = (*instrumentationPoints)[j]->getInstBaseAddress();
            ASSERT((*instrumentationPoints)[j]->getSourceObject()->getContainer()->getType() == PebilClassType_Function);
            Function* container = (Function*)(*instrumentationPoints)[j]->getSourceObject()->getContainer();
            BasicBlock* containerBB = (BasicBlock*)container->getBasicBlockAtAddress(searchAddr);
            ASSERT(containerBB);
            
            Vector<AddressAnchor*>* modAnchors = elfFile->searchAddressAnchors(searchAddr);
            ASSERT(containerBB->getNumberOfInstructions() && containerBB->getLeader());
            PRINT_DEBUG_ANCHOR("In block at %#llx, updating %d anchors", containerBB->getBaseAddress(), modAnchors->size());
            anchors.append(modAnchors);
            blocks.append(containerBB);
        }
    }

    ASSERT(anchors.size() == blocks.size());
    while (anchors.size()){
        Vector<AddressAnchor*>* modAnchors = anchors.remove(0);
        BasicBlock* containerBB = blocks.remove(0);
        for (uint32_t k = 0; k < modAnchors->size(); k++){
            (*modAnchors)[k]->updateLink(containerBB->getLeader());
            elfFile->setAnchorsSorted(false);
        }
        delete modAnchors;
    }
    ASSERT(!blocks.size());

    TIMER(t2 = timer();PRINT_INFOR("___timer: \t\tFncReloc Step %c Reanchor : %.2f seconds",stepNumber++,t2-t1);t1=t2);

    return codeOffset;
}

void ElfFileInst::functionSelect(){
    uint32_t numberOfBytes = 0;
    uint32_t missingBytes = 0;

    TextSection* text = getDotTextSection();
    TextSection* fini = getDotFiniSection();
    TextSection* init = getDotInitSection();
    ASSERT(text && text->getType() == PebilClassType_TextSection && "Cannot find the text section");
    ASSERT(fini && text->getType() == PebilClassType_TextSection && "Cannot find the fini section");
    ASSERT(init && text->getType() == PebilClassType_TextSection && "Cannot find the init section");

    Vector<TextObject*> textObjects = Vector<TextObject*>();
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        textObjects.append(text->getTextObject(i));
    }
    for (uint32_t i = 0; i < fini->getNumberOfTextObjects(); i++){
        textObjects.append(fini->getTextObject(i));
    }
    for (uint32_t i = 0; i < init->getNumberOfTextObjects(); i++){
        textObjects.append(init->getTextObject(i));
    }

    // choose the set of functions to expose to the instrumentation tool
    PRINT_DEBUG_FUNC_RELOC("Choosing from %d functions", text->getNumberOfTextObjects()+fini->getNumberOfTextObjects()+init->getNumberOfTextObjects());

    for (uint32_t i = 0; i < textObjects.size(); i++){
        if (textObjects[i]->isFunction()){
            Function* f = (Function*)textObjects[i];
            allFunctions.append(f);

            numberOfBytes += f->getSizeInBytes();

            if (f->hasCompleteDisassembly() && isEligibleFunction(f)){
                PRINT_DEBUG_FUNC_RELOC("\texposed: %s", f->getName());
                exposedFunctions.append(f);
                for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                    BasicBlock* bb = f->getBasicBlock(j);
                    exposedBasicBlocks.append(bb);
                    for (uint32_t k = 0; k < bb->getNumberOfInstructions(); k++){
                        X86Instruction* ins = bb->getInstruction(k);
                        exposedInstructions.append(ins);
                        if (ins->isMemoryOperation()){
                            exposedMemOps.append(ins);
                        }
                    }
                }
            } else {
                if (!isDisabledFunction(f)){
                    PRINT_DEBUG_FUNC_RELOC("\thidden: %s\ta%d b%d c%#llx d%d e%d f%d g%d h%d i%d j%d", f->getName(), 
                                /*a*/f->hasCompleteDisassembly(), 
                                /*b*/isEligibleFunction(f), 
                                /*c*/f->getBadInstruction(), 
                                /*d*/f->isDisasmFail(), 
                                /*e*/canRelocateFunction(f), 
                                /*f*/f->isInstrumentationFunction(), 
                                /*g*/f->getNumberOfBytes(), 
                                /*h*/isDisabledFunction(f), 
                                /*i*/f->hasSelfDataReference(), 
                                /*j*/f->isDisasmFail());
                    PRINT_INFOR("Hiding function from instrumentation: %s (%#llx + %d bytes)", f->getName(), f->getBaseAddress(), f->getSizeInBytes());
                }
                hiddenFunctions.append(f);
                missingBytes += f->getSizeInBytes();
            }
        }
    }
    exposedBasicBlocks.sort(compareBaseAddress);

    PRINT_INFOR("Total hidden from instrumentation (bytes):\t%d/%d (%.2f%)", missingBytes, numberOfBytes, ((float)((float)missingBytes*100)/((float)numberOfBytes)));
}


// the order of the operations in this function matters
void ElfFileInst::phasedInstrumentation(){
    TIMER(double t1 = timer(), t2; char stepNumber = 'A');
    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");

    ASSERT(elfFile->getFileHeader()->GET(e_flags) == EFINSTSTATUS_NON && "This executable appears to already be instrumented");
    elfFile->getFileHeader()->SET(e_flags, EFINSTSTATUS_MOD);

    ASSERT(currentPhase == ElfInstPhase_no_phase && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

    uint32_t numberOfBasicBlocks = getDotInitSection()->getNumberOfBasicBlocks() + 
        getDotTextSection()->getNumberOfBasicBlocks() + getDotFiniSection()->getNumberOfBasicBlocks();

    extendTextSection(TEXT_EXTENSION_INC, INSTHDR_RESERVE_AMT); // creates space for extra elf control info (symbols, dynamic table entries, hash entries, etc)
    elfFile->anchorProgramElements();

    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    declare();
    if (!elfFile->isStaticLinked()){
        extendDynamicTable();
    }

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    functionSelect();

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
    verify();

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");
    currentPhase++;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    instrument();

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    (*instrumentationPoints).sort(compareInstBaseAddress);
    verify();

    for (uint32_t i = 0; i < (*instrumentationPoints).size();){
        Vector<InstrumentationPoint*> priorpt = Vector<InstrumentationPoint*>();
        Vector<InstrumentationPoint*> afterpt = Vector<InstrumentationPoint*>();
        Vector<InstrumentationPoint*> replacept = Vector<InstrumentationPoint*>();
        uint32_t j = i;
        while (j < (*instrumentationPoints).size() && 
               (*instrumentationPoints)[j]->getInstBaseAddress() == (*instrumentationPoints)[i]->getInstBaseAddress() &&
               (*instrumentationPoints)[j]->getSourceObject() == (*instrumentationPoints)[i]->getSourceObject()){
            if ((*instrumentationPoints)[j]->getInstLocation() == InstLocation_prior){
                priorpt.append((*instrumentationPoints)[j]);
            } else if ((*instrumentationPoints)[j]->getInstLocation() == InstLocation_after){
                afterpt.append((*instrumentationPoints)[j]);
            } else if ((*instrumentationPoints)[j]->getInstLocation() == InstLocation_replace){
                replacept.append((*instrumentationPoints)[j]);
            } else {
                __SHOULD_NOT_ARRIVE;
            }
            j++;
        }
        
        int32_t currentOffset = 0;
        for (int32_t k = priorpt.size() - 1; k >= 0; k--){
            uint32_t bytesreq = Size__uncond_jump;
            if (priorpt[k]->getInstrumentationMode() == InstrumentationMode_inline){
                bytesreq = priorpt[k]->getNumberOfBytes();
            }
            currentOffset -= bytesreq;
            priorpt[k]->setInstSourceOffset(currentOffset);
        }

        currentOffset = (*instrumentationPoints)[i]->getSourceObject()->getSizeInBytes();
        for (uint32_t k = 0; k < afterpt.size(); k++){
            uint32_t bytesreq = Size__uncond_jump;
            if (afterpt[k]->getInstrumentationMode() == InstrumentationMode_inline){
                bytesreq = afterpt[k]->getNumberOfBytes();
            }
            afterpt[k]->setInstSourceOffset(bytesreq);
            currentOffset += bytesreq;
        }

        currentOffset = 0;
        ASSERT(replacept.size() < 2 && "Cannot have more than 1 point replacement");
        for (uint32_t k = 0; k < replacept.size(); k++){
            replacept[k]->setInstSourceOffset(currentOffset);
        }        

        i = j;
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    buildInstrumentationSections();
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c UsrResrv : %.2f seconds",stepNumber++,t2-t1);t1=t2);
    relocatedTextSize += functionRelocateAndTransform(relocatedTextSize);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c FncReloc : %.2f seconds",stepNumber++,t2-t1);t1=t2);
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");

    ASSERT(currentPhase == ElfInstPhase_modify_control && "Instrumentation phase order must be observed");
    TIMER(t2 = timer();PRINT_INFOR("___timer: \tInstr Step %c Control  : %.2f seconds",stepNumber++,t2-t1);t1=t2);
    currentPhase++;
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

    uint32_t textSize = generateInstrumentation();
    compressInstrumentation(textSize);
    
    usesModifiedProgram();
    applyInstrumentationDataToRaw();

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
    for (uint32_t i = 0; i < (*instrumentationPoints).size(); i++){
        (*instrumentationPoints)[i]->dump(binaryOutputFile,extraTextOffset);
    }
    for (uint32_t i = 0; i < relocatedFunctions.size(); i++){
        relocatedFunctions[i]->dump(binaryOutputFile,extraTextOffset+relocatedFunctionOffsets[i]);
    }
}

bool ElfFileInst::verify(){
    if (!elfFile->verify()){
        return false;
    }
    if (!allowStatic && elfFile->isStaticLinked()){
        PRINT_ERROR("Static-linked binaries can usually not be instrumented, use --allow-static to try to instrument anyway");
	return false;
    }

    return true;
}

InstrumentationPoint* ElfFileInst::addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");    
    return addInstrumentationPoint(instpoint, inst, instMode, flagsMethod, InstLocation_prior);
}
InstrumentationPoint* ElfFileInst::addInstrumentationPoint(Base* instpoint, Instrumentation* inst, InstrumentationModes instMode, FlagsProtectionMethods flagsMethod, InstLocations loc){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");    

    InstrumentationPoint* newpoint;
    if (elfFile->is64Bit()){
        newpoint = new InstrumentationPoint64(instpoint, inst, instMode, flagsMethod, loc);
    } else {
        newpoint = new InstrumentationPoint32(instpoint, inst, instMode, flagsMethod, loc);
    }

    (*instrumentationPoints).append(newpoint);

    ASSERT((*instrumentationPoints).back());
    return (*instrumentationPoints).back();
}

InstrumentationFunction* ElfFileInst::declareFunction(char* funcName){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        InstrumentationFunction* func = instrumentationFunctions[i];
        if (!strcmp(funcName,func->getFunctionName())){
            return func;
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
            PRINT_WARN(15, "In static linked binary instrumentation function %s should be linked in", funcName);
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

    uint64_t gotAddress = instrumentationDataAddress + gotOffset;    
    uint64_t relocOffset;
    if (elfFile->is64Bit()){
        relocOffset = relocTable->addRelocation(gotAddress, ELF64_R_INFO(symbolIndex, R_X86_64_JUMP_SLOT));
    } else {
        relocOffset = relocTable->addRelocation(gotAddress, ELF32_R_INFO(symbolIndex, R_386_JMP_SLOT));
        // for 32bit the linker uses an offset into the table instead of its index
        relocOffset *= relocTable->getRelocationSize();
    }
    ASSERT(relocOffset && "Should set the relocation offset to a non-trival value");


    SectionHeader* relocationSection = elfFile->getSectionHeader(relocTable->getSectionIndex());
    uint32_t extraSize = relocTable->getRelocationSize();
    relocationSection->INCREMENT(sh_size,extraSize);

    // displace every section in the text segment that comes after the dynamic string section and before the initial text section
    uint16_t ftidx = elfFile->findSectionIdx(".init") - 1;
    for (uint32_t i = relocationSection->getIndex()+1; i <= ftidx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset, extraSize);
        if (sHdr->GET(sh_addr)){
            sHdr->INCREMENT(sh_addr, extraSize);
        }
    }

    // shrink the size of the extra text section to accomodate the increase in size of the control sections
    //    elfFile->getSectionHeader(extraTextIdx)->SET(sh_size,elfFile->getSectionHeader(extraTextIdx)->GET(sh_size)-extraSize);

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


void ElfFileInst::extendTextSection(uint64_t totalSize, uint64_t headerSize){
    ASSERT(currentPhase == ElfInstPhase_extend_space && "Instrumentation phase order must be observed");

    totalSize = nextAlignAddress(totalSize, elfFile->getProgramHeader(elfFile->getTextSegmentIdx())->GET(p_align));
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
            if (subHeader->GET(p_vaddr) < totalSize){
                PRINT_WARN(5,"Unable to extend text section by 0x%llx bytes: the maximum size of a text extension for this binary is 0x%llx bytes", totalSize, subHeader->GET(p_vaddr));
            }
            ASSERT(subHeader->GET(p_vaddr) >= totalSize && "The text extension size is too large");
            subHeader->SET(p_vaddr, subHeader->GET(p_vaddr) - totalSize);
            subHeader->SET(p_paddr, subHeader->GET(p_paddr) - totalSize);
        } 
    }

    // for each segment that is (or is contained within) the data segment,
    // update its offset to reflect the the base address of the executable
    // (ie the base address of the text segment)
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* subHeader = elfFile->getProgramHeader(i);
        if (dataHeader->inRange(subHeader->GET(p_vaddr))){
            subHeader->INCREMENT(p_offset,totalSize);
        }
    }

    // update section symbols for the sections that were moved. technically the loader won't use them 
    // but we will try to keep them as consistent as possible anyway
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symTab = elfFile->getSymbolTable(i);
        for (uint32_t j = 0; j < symTab->getNumberOfSymbols(); j++){
            Symbol* sym = symTab->getSymbol(j);
            if (sym->getSymbolType() == STT_SECTION && sym->GET(st_value) < lowestTextAddress && sym->GET(st_value)){
                sym->SET(st_value,sym->GET(st_value)-totalSize);
            }
        }
    }
    
    // modify the base address of the text segment and increase its size so it ends at the same address
    textHeader->SET(p_vaddr,textHeader->GET(p_vaddr)-totalSize);
    textHeader->SET(p_paddr,textHeader->GET(p_paddr)-totalSize);
    textHeader->INCREMENT(p_memsz,totalSize);
    textHeader->INCREMENT(p_filesz,totalSize);

    // For any section that falls before the program's code, displace its address so that it is in the
    // same location relative to the base address.
    // Likewise, displace the offset of any section that falls during/after the program's code so that
    // the code will be put in the correct location within the text segment.
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        if (i < lowestTextSectionIdx){
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) < lowestTextAddress && "No section that occurs before the first text section should have a larger address");
            // strictly speaking the loader doesn't use these, but for consistency we change them anyway
            ASSERT(elfFile->getSectionHeader(i)->GET(sh_addr) > totalSize && "The text extension size is too large");
            sHdr->SET(sh_addr,sHdr->GET(sh_addr) - totalSize);
        } else {
            sHdr->INCREMENT(sh_offset, totalSize);            
        }
    }

    FileHeader* fHdr = elfFile->getFileHeader();

    // update the dynamic table to correctly point to the displaced elf control sections
    if (!elfFile->isStaticLinked()){
        ASSERT(elfFile->getDynamicTable());
        for (uint32_t i = 0; i < elfFile->getDynamicTable()->getNumberOfDynamics(); i++){
            Dynamic* dyn = elfFile->getDynamicTable()->getDynamic(i);
            uint64_t tag = dyn->GET(d_tag);
            if (tag == DT_HASH || tag == DT_GNU_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
                tag == DT_VERSYM || tag == DT_VERNEED ||
                tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
                dyn->SET_A(d_ptr,d_un,dyn->GET_A(d_ptr,d_un)-totalSize);
            }
        }
    }

    // reserve space for the program header at the front of the file
    for (uint32_t i = 1; i < elfFile->getNumberOfSections(); i++){
        if (i < lowestTextSectionIdx){
            elfFile->getSectionHeader(i)->INCREMENT(sh_offset, headerSize);
            elfFile->getSectionHeader(i)->INCREMENT(sh_addr, headerSize);
        }
    }    

    // update the dynamic table to correctly point to the displaced elf control sections
    if (!elfFile->isStaticLinked()){
        ASSERT(elfFile->getDynamicTable());
        for (uint32_t i = 0; i < elfFile->getDynamicTable()->getNumberOfDynamics(); i++){
            Dynamic* dyn = elfFile->getDynamicTable()->getDynamic(i);
            uint64_t tag = dyn->GET(d_tag);
            if (tag == DT_HASH || tag == DT_GNU_HASH || tag == DT_STRTAB || tag == DT_SYMTAB ||
                tag == DT_VERSYM || tag == DT_VERNEED ||
                tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
                dyn->SET_A(d_ptr,d_un,dyn->GET_A(d_ptr,d_un) + headerSize);
            }
        }
    }

    // update the phdr table to reflect the reserved space
    for (uint32_t i = 0; i < elfFile->getNumberOfPrograms(); i++){
        ProgramHeader* pHdr = elfFile->getProgramHeader(i);
        if (pHdr->GET(p_type) == PT_INTERP ||
            pHdr->GET(p_type) == PT_NOTE){
            pHdr->INCREMENT(p_offset, headerSize);
            pHdr->INCREMENT(p_paddr, headerSize);
            pHdr->INCREMENT(p_vaddr, headerSize);
        }
    }
    
    // move the shdr table into the reserved aread (phdr table is already there)
    fHdr->SET(e_shoff, fHdr->GET(e_phoff) + ((fHdr->GET(e_phnum) + 2) * fHdr->GET(e_phentsize)));

    verify();
}


ElfFileInst::~ElfFileInst(){
    for (uint32_t i = 0; i < instrumentationFunctions.size(); i++){
        delete instrumentationFunctions[i];
    }

    for (uint32_t i = 0; i < instrumentationSnippets.size(); i++){
        delete instrumentationSnippets[i];
    }

    for (uint32_t i = 0; i < (*instrumentationPoints).size(); i++){
        delete (*instrumentationPoints)[i];
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

    for (uint32_t i = 0; i < (*disabledFunctions).size(); i++){
        delete[] (*disabledFunctions)[i];
    }
    delete disabledFunctions;
    
    if (instrumentationData){
        delete[] instrumentationData;
    }

    if (instrumentationPoints){
        delete instrumentationPoints;
    }

    for (uint32_t i = 0; i < replacedInstructions.size(); i++){
        delete replacedInstructions[i];
    }
}

void ElfFileInst::dump(){
    ASSERT(currentPhase == ElfInstPhase_dump_file && "Instrumentation phase order must be observed");

    char fileName[__MAX_STRING_SIZE] = "";
    sprintf(fileName,"%s.%s", elfFile->getFileName(), getInstSuffix());

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


ElfFileInst::ElfFileInst(ElfFile* elf){
    currentPhase = ElfInstPhase_no_phase;
    elfFile = elf;

    // automatically set 2 snippets for the beginning and end of bootstrap code
    instrumentationSnippets.append(new InstrumentationSnippet());
    instrumentationSnippets.append(new InstrumentationSnippet());

    extraTextIdx = 0;
    extraDataIdx = elfFile->getDotDataSection()->getSectionIndex();
    dataIdx = 0;

    instSuffix = NULL;
    sharedLibraryPath = NULL;

    lineInfoFinder = NULL;
    if (elfFile->getLineInfoSection()){
        lineInfoFinder = new LineInfoFinder(elfFile->getLineInfoSection());
    }

    programEntryBlock = getDotTextSection()->getBasicBlockAtAddress(elfFile->getFileHeader()->GET(e_entry));

    relocatedTextSize = 0;
    instrumentationData = NULL;
    instrumentationDataSize = 0;
    instrumentationDataAddress = 0;

    for (uint32_t i = 0; i < elfFile->getNumberOfSections(); i++){
        SectionHeader* sec = elfFile->getSectionHeader(i);
        if (sec->GET(sh_addr)){
            if (sec->GET(sh_addr) + sec->GET(sh_size) > instrumentationDataAddress){
                instrumentationDataAddress = sec->GET(sh_addr) + sec->GET(sh_size);
            }
        }
    }

    ProgramHeader* dHdr = elfFile->getProgramHeader(elfFile->getDataSegmentIdx());
    instrumentationDataAddress = nextAlignAddress(instrumentationDataAddress, dHdr->GET(p_align));
    dynamicTableReserved = instrumentationDataAddress;
    if (!elfFile->isStaticLinked()){
      instrumentationDataAddress += Reserve__Instrumentation_DynamicTable;
    }

    instrumentationPoints = new Vector<InstrumentationPoint*>();
    // automatically set the 1st instrumentation point to go to the bootstrap code
    ASSERT(instrumentationPoints);
    (*instrumentationPoints).append(NULL);
    // find the entry point of the program and put an instrumentation point for our initialization there
    ASSERT(!(*instrumentationPoints)[INST_POINT_BOOTSTRAP1] && "instrumentationPoint[INST_POINT_BOOTSTRAP1] is reserved");
    BasicBlock* entryBlock = getProgramEntryBlock();
    ASSERT(entryBlock && "Cannot find instruction at the program's entry point");
    if (elfFile->is64Bit()){
        (*instrumentationPoints)[INST_POINT_BOOTSTRAP1] = 
            new InstrumentationPoint64((Base*)entryBlock, instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN], InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);    
    } else {
        (*instrumentationPoints)[INST_POINT_BOOTSTRAP1] = 
            new InstrumentationPoint32((Base*)entryBlock, instrumentationSnippets[INST_SNIPPET_BOOTSTRAP_BEGIN], InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);    
    }
    (*instrumentationPoints)[INST_POINT_BOOTSTRAP1]->setPriority(InstPriority_sysinit);

    regStorageOffset = 0;
    fxStorageOffset = sizeof(uint64_t) * X86_64BIT_GPRS;
    // space for gprs, plus space for fp state
    regStorageReserved = fxStorageOffset + FXSTORAGE_RESERVED;
    usableDataOffset = regStorageOffset + regStorageReserved;

    instSegment = NULL;

    disabledFunctions = new Vector<char*>();

    flags = InstrumentorFlag_none;
    allowStatic = false;
}

void ElfFileInst::setInputFunctions(char* inputFuncList){
    ASSERT(!(*disabledFunctions).size());

    if (inputFuncList){
        initializeFileList(inputFuncList, disabledFunctions);
    }
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
    uint16_t ftidx = elfFile->findSectionIdx(".init") - 1;
    for (uint32_t i = dynamicSymbolSection->getIndex()+1; i <= ftidx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,entrySize);
        if (sHdr->GET(sh_addr)){
            sHdr->INCREMENT(sh_addr,entrySize);
        }
    }

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

    // add an entry to the hash table and version symbol table
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

    for (int32_t i = elfFile->getNumberOfHashTables() - 1; i >= 0; i--){
        HashTable* hashTable = elfFile->getHashTable(i);

        // wow! this is an ultrahack. the gnu hash table is required to come after the
        // expansion because it is possible that the addition of the element prior to
        // expansion causes a build with 0 buckets available, which we dont want
        if (!hashTable->isGnuStyleHash()){
            hashTable->addEntry();
        }
        if (hashTable->passedThreshold()){
            expandHashTable(i);
        }    
        if (hashTable->isGnuStyleHash()){
            hashTable->addEntry();
        }
    }

    // displace every section that comes after the gnu versym section and before the code
    for (uint32_t i = versymHeader->getIndex()+1; i <= ftidx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    } 

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

uint32_t ElfFileInst::expandHashTable(uint32_t idx){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    HashTable* hashTable = elfFile->getHashTable(idx);
    uint32_t extraHashEntries = hashTable->expandSize(hashTable->getNumberOfEntries()/2);

    SectionHeader* hashHeader = elfFile->getSectionHeader(hashTable->getSectionIndex());
    uint32_t extraSize = extraHashEntries * hashTable->getEntrySize();
    hashHeader->INCREMENT(sh_size,extraSize);

    uint16_t ftidx = elfFile->findSectionIdx(".init") - 1;
    for (uint32_t i = hashTable->getSectionIndex() + 1; i <= ftidx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset,extraSize);
        sHdr->INCREMENT(sh_addr,extraSize);
    }

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED || tag == DT_STRTAB || tag == DT_SYMTAB ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            dyn->INCREMENT_A(d_ptr,d_un,extraSize);
        }
        if (!hashTable->isGnuStyleHash() && tag == DT_GNU_HASH){
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
    uint16_t ftidx = elfFile->findSectionIdx(".init") - 1;
    for (uint32_t i = dynamicStringSection->getIndex()+1; i <= ftidx; i++){
        SectionHeader* sHdr = elfFile->getSectionHeader(i);
        extraSize = nextAlignAddress(sHdr->GET(sh_addr) + extraSize, sHdr->GET(sh_addralign)) - sHdr->GET(sh_addr);
        sHdr->INCREMENT(sh_offset, extraSize);
        sHdr->INCREMENT(sh_addr, extraSize);
    }

    for (uint32_t i = 0; i < dynamicTable->getNumberOfDynamics(); i++){
        Dynamic* dyn = dynamicTable->getDynamic(i);
        uint64_t tag = dyn->GET(d_tag);
        if (tag == DT_VERSYM || tag == DT_VERNEED ||
            tag == DT_REL || tag == DT_RELA || tag == DT_JMPREL){
            ASSERT(dyn->GET_A(d_ptr, d_un) > dynamicStringSection->GET(sh_addr) && "The gnu version tables and relocation tables should be after the dynamic string table");
            dyn->INCREMENT_A(d_ptr, d_un, extraSize);
        }

    }

    verify();
    return origSize;
}

uint64_t ElfFileInst::addFunction(InstrumentationFunction* func){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed");

    uint32_t funcNameOffset = addStringToDynamicStringTable(func->getFunctionName());

    DynamicTable* dynamicTable = elfFile->getDynamicTable();
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
    for (uint32_t i = 0; i < dynamicSymbolTable->getNumberOfSymbols(); i++){
        if (!strcmp(dynamicSymbolTable->getSymbolName(i),func->getFunctionName())){
            PRINT_ERROR("A symbol named `%s' already exists in the dynamic symbol table", dynamicSymbolTable->getSymbolName(i));
            __SHOULD_NOT_ARRIVE;
        }
    }

    uint64_t relocationOffset = addPLTRelocationEntry(dynamicSymbolTable->getNumberOfSymbols(), func->getGlobalDataOffset());
    uint32_t symbolIndex = addSymbolToDynamicSymbolTable(funcNameOffset, 0, 0, STB_GLOBAL, STT_FUNC, 0, 0);

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
    verify();
    uint32_t strOffset = addStringToDynamicStringTable(sharedLibraryPath);
    verify();

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

    PRINT_INFOR("Linking instrumented binary to shared library: %s", libname);

    // first make sure the lib isn't already linked
    DynamicTable* dynamicTable = elfFile->getDynamicTable();
    for (uint32_t i = 0; i < dynamicTable->countDynamics(DT_NEEDED); i++){
        Dynamic* dyn = dynamicTable->getDynamicByType(DT_NEEDED, i);
        if (!strcmp(elfFile->getDynamicStringTable()->getString(dyn->GET_A(d_ptr,d_un)), libname)){
            PRINT_WARN(10, "Library %s already exists in the executable, skipping", libname);
            return 0;
        }
    }

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

    ASSERT(segmentBase != -1 && "No loadable segments found (or their p_vaddr fields are incorrect)");
    return segmentBase;
}
