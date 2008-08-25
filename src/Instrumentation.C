#include <Instrumentation.h>

void InstrumentationFunction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (procedureLinkInstructions){
        uint32_t currentOffset = procedureLinkOffset;
        for (uint32_t i = 0; i < numberOfProcedureLinkInstructions; i++){
            procedureLinkInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += procedureLinkInstructions[i]->getLength();
        }
    }
    if (bootstrapInstructions){
        uint32_t currentOffset = bootstrapOffset;
        for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
            bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += bootstrapInstructions[i]->getLength();
        }
    }
    if (wrapperInstructions){
        uint32_t currentOffset = wrapperOffset;
        for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
            wrapperInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += wrapperInstructions[i]->getLength();
        }
    }
}

uint64_t InstrumentationFunction::getEntryPoint(){
    return wrapperOffset;
}

uint64_t InstrumentationSnippet::getEntryPoint(){
    return codeOffset;
}


uint32_t InstrumentationFunction64::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    /*
    ASSERT(currentPhase == ElfInstPhase_generate_instrumentation && "Instrumentation phase order must be observed");

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

    */
    __SHOULD_NOT_ARRIVE;
    return 0;
}

uint32_t InstrumentationFunction32::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions && "This array should not be initialized");

    uint32_t pltSize = 0;

    numberOfProcedureLinkInstructions = PLT_INSTRUCTION_COUNT_32BIT;
    procedureLinkInstructions = new Instruction*[numberOfProcedureLinkInstructions];
    numberOfProcedureLinkInstructions = 0;

    procedureLinkInstructions[numberOfProcedureLinkInstructions] = Instruction::generateJumpIndirect32(dataBaseAddress + globalDataOffset);
    pltSize += procedureLinkInstructions[numberOfProcedureLinkInstructions]->getLength();
    numberOfProcedureLinkInstructions++;

    procedureLinkInstructions[numberOfProcedureLinkInstructions] = Instruction::generateStackPushImmediate(relocationOffset);
    pltSize += procedureLinkInstructions[numberOfProcedureLinkInstructions]->getLength();
    numberOfProcedureLinkInstructions++;

    uint32_t returnAddress = textBaseAddress + procedureLinkOffset + pltSize;
    procedureLinkInstructions[numberOfProcedureLinkInstructions] = Instruction::generateJumpRelative(returnAddress,realPLTAddress);
    pltSize += procedureLinkInstructions[numberOfProcedureLinkInstructions]->getLength();
    numberOfProcedureLinkInstructions++;

    ASSERT(numberOfProcedureLinkInstructions == PLT_INSTRUCTION_COUNT_32BIT);
    ASSERT(pltSize == Size__32_bit_Procedure_Link);
    return numberOfProcedureLinkInstructions;
}

uint32_t InstrumentationFunction64::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    /*
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

    */
    __SHOULD_NOT_ARRIVE;
    return 0;
}

uint32_t InstrumentationFunction32::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!bootstrapInstructions && "This array should not be initialized");

    uint32_t bootstrapSize = 0;

    numberOfBootstrapInstructions = BOOTSTRAP_INSTRUCTION_COUNT_32BIT;
    bootstrapInstructions = new Instruction*[numberOfBootstrapInstructions];
    numberOfBootstrapInstructions = 0;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveImmToReg(globalData,X86_REG_CX);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateMoveRegToMem(X86_REG_CX,dataBaseAddress + globalDataOffset);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    PRINT_INFOR("bootstrap size %d, expected %d", bootstrapSize, Size__32_bit_Bootstrap);
    ASSERT(numberOfBootstrapInstructions == BOOTSTRAP_INSTRUCTION_COUNT_32BIT);
    ASSERT(bootstrapSize == Size__32_bit_Bootstrap);
    return numberOfBootstrapInstructions;
}

uint32_t InstrumentationFunction64::generateWrapperInstructions(uint64_t textBaseAddress){
    /*

    uint64_t pltAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + pltOffset;
    uint64_t currAddress = elfFile->getSectionHeader(extraTextIdx)->GET(sh_addr) + bootstrapOffset + bootstrapSize;

    bootstrapInstructions[numberOfBootstrapInstructions] = Instruction::generateCallRelative(currAddress,pltAddress);
    bootstrapSize += bootstrapInstructions[numberOfBootstrapInstructions]->getLength();
    numberOfBootstrapInstructions++;

    ASSERT(bootstrapSize < elfFile->getSectionHeader(extraTextIdx)->GET(sh_size) - extraTextOffset);
    extraTextOffset += bootstrapSize;


    PRINT_INFOR("Verifying elf structures after adding our PLT");

    verify();

    return pltReturnOffset;

    */

    __SHOULD_NOT_ARRIVE;
    return 0;
}

uint32_t InstrumentationFunction32::generateWrapperInstructions(uint64_t textBaseAddress){
    ASSERT(!wrapperInstructions && "This array should not be initialized");
    uint32_t wrapperSize = 0;

    numberOfWrapperInstructions = WRAPPER_INSTRUCTION_COUNT_32BIT;
    wrapperInstructions = new Instruction*[numberOfWrapperInstructions];
    numberOfWrapperInstructions = 0;

    wrapperInstructions[numberOfWrapperInstructions] = Instruction::generatePushEflags();
    wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
    numberOfWrapperInstructions++;

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions[numberOfWrapperInstructions] = Instruction::generateStackPush32(i);
        wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
        numberOfWrapperInstructions++;
    }

    wrapperInstructions[numberOfWrapperInstructions] = Instruction::generateCallRelative(wrapperOffset + wrapperSize, procedureLinkOffset);
    wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
    numberOfWrapperInstructions++;

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions[numberOfWrapperInstructions] = Instruction::generateStackPop32(X86_32BIT_GPRS-1-i);
        wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
        numberOfWrapperInstructions++;
    }

    wrapperInstructions[numberOfWrapperInstructions] = Instruction::generatePopEflags();
    wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
    numberOfWrapperInstructions++;

    wrapperInstructions[numberOfWrapperInstructions] = Instruction::generateReturn();
    wrapperSize += wrapperInstructions[numberOfWrapperInstructions]->getLength();
    numberOfWrapperInstructions++;

    ASSERT(numberOfWrapperInstructions == WRAPPER_INSTRUCTION_COUNT_32BIT);
    ASSERT(wrapperSize == Size__32_bit_Function_Wrapper);
    return numberOfWrapperInstructions;
}

uint32_t InstrumentationFunction64::generateGlobalData(uint64_t textBaseAddress){
    __SHOULD_NOT_ARRIVE;
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_64BIT;
    return globalData;
}

uint32_t InstrumentationFunction32::generateGlobalData(uint64_t textBaseAddress){
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_32BIT;
    return globalData;
}

void InstrumentationFunction::setCodeOffsets(uint64_t plOffset, uint64_t bsOffset, uint64_t fwOffset){
    procedureLinkOffset = plOffset;
    bootstrapOffset = bsOffset;
    wrapperOffset = fwOffset;
}

uint32_t InstrumentationFunction::sizeNeeded(){
    uint32_t totalSize = wrapperSize() + procedureLinkSize() + bootstrapSize();
    return totalSize;
}

InstrumentationFunction::InstrumentationFunction(uint32_t idx, char* funcName)
    : Instrumentation(ElfClassTypes_InstrumentationFunction, idx)
{
    functionName = new char[strlen(funcName)+1];
    strcpy(functionName,funcName);

    numberOfProcedureLinkInstructions = 0;
    procedureLinkInstructions = NULL;
    procedureLinkOffset = 0;

    numberOfBootstrapInstructions = 0;
    bootstrapInstructions = NULL;
    bootstrapOffset = 0;

    numberOfWrapperInstructions = 0;
    wrapperInstructions = NULL;
    wrapperOffset = 0;

    globalData = 0;
    globalDataOffset = index * Size__32_bit_Global_Offset_Table_Entry;
}

InstrumentationFunction::~InstrumentationFunction(){
    if (functionName){
        delete[] functionName;
    }
    if (procedureLinkInstructions){
        for (uint32_t i = 0; i < numberOfProcedureLinkInstructions; i++){
            delete procedureLinkInstructions[i];
        }
        delete[] procedureLinkInstructions;
    }
    if (bootstrapInstructions){
        for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
            delete bootstrapInstructions[i];
        }
        delete[] bootstrapInstructions;
    }
    if (wrapperInstructions){
        for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
            delete wrapperInstructions[i];
        }
        delete[] wrapperInstructions;
    }
}

void InstrumentationFunction::print(){
    PRINT_INFOR("Instrumentation Function (%d) %s", index, functionName);
    PRINT_INFOR("\t\tProcedure Link Instructions: %d", numberOfProcedureLinkInstructions);
    PRINT_INFOR("\t\tProcedure Link Offset      : %lld", procedureLinkOffset);
    for (uint32_t i = 0; i < numberOfProcedureLinkInstructions; i++){
        procedureLinkInstructions[i]->print();
    }
    PRINT_INFOR("\t\tBootstrap Instructions     : %d", numberOfBootstrapInstructions);
    PRINT_INFOR("\t\tBootstrap Offset           : %lld", bootstrapOffset);
    for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
        bootstrapInstructions[i]->print();
    }
    PRINT_INFOR("\t\tWrapper Instructions       : %d", numberOfWrapperInstructions);
    PRINT_INFOR("\t\tWrapper Offset             : %lld", wrapperOffset);
    for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
        wrapperInstructions[i]->print();
    }
    PRINT_INFOR("\t\tGlobal Data                : %x", globalData);
    PRINT_INFOR("\t\tGlobal Data Offset         : %lld", globalDataOffset);

}

void InstrumentationSnippet::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (instructions){
        uint32_t currentOffset = codeOffset;
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            instructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += instructions[i]->getLength();
        }
    }
}

uint32_t InstrumentationSnippet::addInstruction(Instruction* inst){
    Instruction** newInstructions = new Instruction*[numberOfInstructions+1];
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        newInstructions[i] = instructions[i];
    }
    newInstructions[numberOfInstructions] = inst;
    delete[] instructions;
    instructions = newInstructions;
    numberOfInstructions++;
}

void InstrumentationSnippet::print(){
}

uint32_t InstrumentationSnippet::sizeNeeded(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        totalSize += instructions[i]->getLength();
    }
    return totalSize;
}

InstrumentationSnippet::InstrumentationSnippet(uint32_t idx)
    : Instrumentation(ElfClassTypes_InstrumentationSnippet, idx)
{
    numberOfInstructions = 0;
    instructions = NULL;

    codeOffset = 0;
}

InstrumentationSnippet::~InstrumentationSnippet(){
    if (instructions){
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            delete instructions[i];
        }
        delete[] instructions;
    }
}

void InstrumentationPoint::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (trampolineInstructions){
        uint32_t currentOffset = trampolineOffset;
        for (uint32_t i = 0; i < numberOfTrampolineInstructions; i++){
            trampolineInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += trampolineInstructions[i]->getLength();
        }
    }
}

uint32_t InstrumentationPoint::sizeNeeded(){
    uint32_t totalSize = 0;
    if (trampolineInstructions){
        for (uint32_t i = 0; i < numberOfTrampolineInstructions; i++){
            totalSize += trampolineInstructions[i]->getLength();
        }
    }
    return totalSize;
}

uint32_t InstrumentationPoint::generateTrampoline(uint32_t count, Instruction** insts, uint64_t offset, uint64_t returnOffset){
    ASSERT(!trampolineInstructions && "Cannot generate trampoline instructions more than once");

    trampolineOffset = offset;
    numberOfTrampolineInstructions = count+2;
    uint32_t trampolineSize = 0;
    trampolineInstructions = new Instruction*[numberOfTrampolineInstructions];
    trampolineInstructions[0] = Instruction::generateCallRelative(offset,getTargetOffset());
    trampolineSize += trampolineInstructions[0]->getLength();
    for (uint32_t i = 0; i < count; i++){
        trampolineInstructions[i+1] = insts[i];
        trampolineSize += trampolineInstructions[i+1]->getLength();
    }
    trampolineInstructions[numberOfTrampolineInstructions-1] = Instruction::generateJumpRelative(offset+trampolineSize,returnOffset);
    trampolineSize += trampolineInstructions[numberOfTrampolineInstructions-1]->getLength();

    return trampolineSize;
}

InstrumentationPoint::InstrumentationPoint(uint32_t idx, Base* pt, Instrumentation* inst)
    : Base(ElfClassTypes_InstrumentationPoint)
{
    index = idx;
    PRINT_INFOR("Initializing instrumentation point %d", index);
    point = pt;
    if (point->getType() != ElfClassTypes_TextSection &&
        point->getType() != ElfClassTypes_Instruction &&
        point->getType() != ElfClassTypes_Function){
        PRINT_ERROR("Cannot use an object of type %d as an instrumentation point", point->getType());
        __SHOULD_NOT_ARRIVE;
    }
    instrumentation = inst;

    trampolineInstructions = NULL;
    numberOfTrampolineInstructions = 0;
    trampolineOffset = 0;
}

uint64_t InstrumentationPoint::getSourceAddress(){
    if (point->getType() == ElfClassTypes_TextSection){
        TextSection* ts = (TextSection*)point;
        return ts->getAddress();
    } else if (point->getType() == ElfClassTypes_Instruction){
        Instruction* in = (Instruction*)point;
        return in->getAddress();
    } else if (point->getType() == ElfClassTypes_Function){
        Function* fn = (Function*)point;
        return fn->getAddress();
    } else {
        PRINT_ERROR("Cannot use an object of type %d as an instrumentation point", point->getType());
    }
}

void InstrumentationPoint::print(){
}
