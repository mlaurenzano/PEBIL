#include <Instrumentation.h>

#include <BasicBlock.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <Instruction.h>
#include <TextSection.h>

int compareSourceAddress(const void* arg1,const void* arg2){
    uint64_t vl1 = (*((InstrumentationPoint**)arg1))->getSourceAddress();
    uint64_t vl2 = (*((InstrumentationPoint**)arg2))->getSourceAddress();

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}


uint32_t InstrumentationSnippet::addSnippetInstruction(Instruction* inst){
    snippetInstructions.append(inst);
    return snippetInstructions.size();
}

uint32_t InstrumentationFunction::wrapperSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        totalSize += wrapperInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::procedureLinkSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        totalSize += procedureLinkInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t Instrumentation::bootstrapSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        totalSize += bootstrapInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::globalDataSize(){
    return Size__32_bit_Global_Offset_Table_Entry;
}

Instrumentation::Instrumentation(ElfClassTypes typ)
    : Base(typ)
{
    bootstrapOffset = 0;
}

Instrumentation::~Instrumentation(){
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        delete bootstrapInstructions[i];
    }
}

uint32_t InstrumentationFunction::addArgument(uint64_t offset){
    return addArgument(offset, 0);
}

uint32_t InstrumentationFunction::addArgument(uint64_t offset, uint32_t value){
    uint64_t* newoffsets = new uint64_t[numberOfArguments+1];
    uint32_t* newvalues = new uint32_t[numberOfArguments+1];

    for (uint32_t i = 0; i < numberOfArguments; i++){
        newoffsets[i] = argumentOffsets[i];
        newvalues[i] = argumentValues[i];
    }

    newoffsets[numberOfArguments] = offset;
    newvalues[numberOfArguments] = value;

    delete[] argumentOffsets;
    delete[] argumentValues;

    argumentOffsets = newoffsets;
    argumentValues = newvalues;

    numberOfArguments++;
    return numberOfArguments;
}

void InstrumentationFunction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = procedureLinkOffset;
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        procedureLinkInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += procedureLinkInstructions[i]->getSizeInBytes();
    }
    currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }
    currentOffset = wrapperOffset;
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        wrapperInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += wrapperInstructions[i]->getSizeInBytes();
    }
}

uint64_t InstrumentationFunction::getEntryPoint(){
    return wrapperOffset;
}

uint64_t InstrumentationSnippet::getEntryPoint(){
    return snippetOffset;
}


uint32_t InstrumentationFunction64::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions.size() && "This array should not be initialized");

    uint64_t procedureLinkAddress = textBaseAddress + procedureLinkOffset;
    procedureLinkInstructions.append(Instruction64::generateIndirectRelativeJump(procedureLinkAddress,dataBaseAddress + globalDataOffset));
    procedureLinkInstructions.append(Instruction64::generateStackPushImmediate(relocationOffset));

    uint32_t returnAddress = procedureLinkAddress + procedureLinkSize();
    procedureLinkInstructions.append(Instruction64::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());
    return procedureLinkInstructions.size();
}

uint32_t InstrumentationFunction32::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions.size() && "This array should not be initialized");
    PRINT_DEBUG_INST("Generating PLT instructions at offset %llx", procedureLinkOffset);

    procedureLinkInstructions.append(Instruction32::generateJumpIndirect(dataBaseAddress + globalDataOffset));
    procedureLinkInstructions.append(Instruction32::generateStackPushImmediate(relocationOffset));
    uint32_t returnAddress = textBaseAddress + procedureLinkOffset + procedureLinkSize();
    procedureLinkInstructions.append(Instruction32::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());

    return procedureLinkInstructions.size();
}

uint32_t InstrumentationFunction64::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    bootstrapInstructions.append(Instruction64::generateMoveImmToReg(globalData,X86_REG_CX));
    bootstrapInstructions.append(Instruction64::generateMoveImmToReg(dataBaseAddress + globalDataOffset,X86_REG_DX));
    bootstrapInstructions.append(Instruction::generateMoveRegToRegaddr(X86_REG_CX,X86_REG_DX));

    while (bootstrapSize() < bootstrapReservedSize()){
        bootstrapInstructions.append(Instruction64::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction32::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    bootstrapInstructions.append(Instruction32::generateMoveImmToReg(globalData,X86_REG_CX));
    bootstrapInstructions.append(Instruction32::generateMoveRegToMem(X86_REG_CX,dataBaseAddress + globalDataOffset));

    while (bootstrapSize() < bootstrapReservedSize()){
        bootstrapInstructions.append(Instruction32::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return bootstrapInstructions.size();
}

uint32_t InstrumentationFunction64::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    wrapperInstructions.append(Instruction64::generatePushEflags());
    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(Instruction64::generateStackPush(i));
    }

    ASSERT(numberOfArguments < MAX_ARGUMENTS_64BIT && "More arguments must be pushed onto stack, which is not yet implemented"); 

    for (uint32_t i = 0; i < numberOfArguments; i++){
        uint32_t idx = numberOfArguments - i - 1;
        uint32_t value = argumentValues[idx];

        uint32_t argumentRegister;

        // use GPRs rdi,rsi,rdx,rcx,r8,r9, then push onto stack
        switch(idx){
        case 0:
            argumentRegister = X86_REG_DI;
            break;
        case 1:
            argumentRegister = X86_REG_SI;
            break;
        case 2:
            argumentRegister = X86_REG_DX;
            break;
        case 3:
            argumentRegister = X86_REG_CX;
            break;
        case 4:
            argumentRegister = X86_REG_R8;
            break;
        case 5:
            argumentRegister = X86_REG_R9;
            break;
        default:
            PRINT_ERROR("Cannot pass more than %d argument to an instrumentation function", MAX_ARGUMENTS_64BIT);
            __SHOULD_NOT_ARRIVE;
        }
        wrapperInstructions.append(Instruction64::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],argumentRegister));

        bootstrapInstructions.append(Instruction64::generateMoveImmToReg(value,X86_REG_CX));
        bootstrapInstructions.append(Instruction64::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],X86_REG_DX));
        bootstrapInstructions.append(Instruction::generateMoveRegToRegaddr(X86_REG_CX,X86_REG_DX));
    }
    wrapperInstructions.append(Instruction64::generateCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        wrapperInstructions.append(Instruction64::generateStackPop(X86_64BIT_GPRS-1-i));
    }
    wrapperInstructions.append(Instruction64::generatePopEflags());

    wrapperInstructions.append(Instruction64::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        wrapperInstructions.append(Instruction64::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return wrapperInstructions.size();
}

uint32_t InstrumentationFunction32::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions.size() && "This array should be empty");

    wrapperInstructions.append(Instruction32::generatePushEflags());
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions.append(Instruction32::generateStackPush(i));
    }

    for (uint32_t i = 0; i < numberOfArguments; i++){
        uint32_t idx = numberOfArguments - i - 1;
        uint32_t value = argumentValues[idx];

        // everything is passed on the stack
        wrapperInstructions.append(Instruction32::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],X86_REG_DX));
        wrapperInstructions.append(Instruction32::generateStackPush(X86_REG_DX));

        bootstrapInstructions.append(Instruction32::generateMoveImmToReg(value,X86_REG_CX));
        bootstrapInstructions.append(Instruction32::generateMoveRegToMem(X86_REG_CX,dataBaseAddress+argumentOffsets[idx]));
    }
    wrapperInstructions.append(Instruction32::generateCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    for (uint32_t i = 0; i < numberOfArguments; i++){
        wrapperInstructions.append(Instruction32::generateStackPop(X86_REG_CX));
    }

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        wrapperInstructions.append(Instruction32::generateStackPop(X86_32BIT_GPRS-1-i));
    }
    wrapperInstructions.append(Instruction32::generatePopEflags());

    wrapperInstructions.append(Instruction32::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        wrapperInstructions.append(Instruction32::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return wrapperInstructions.size();
}

uint32_t InstrumentationFunction64::generateGlobalData(uint64_t textBaseAddress){
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_64BIT;
    return globalData;
}

uint32_t InstrumentationFunction32::generateGlobalData(uint64_t textBaseAddress){
    globalData = textBaseAddress + procedureLinkOffset + PLT_RETURN_OFFSET_32BIT;
    return globalData;
}

uint32_t InstrumentationFunction::sizeNeeded(){
    uint32_t totalSize = wrapperSize() + procedureLinkSize() + bootstrapSize();
    return totalSize;
}

InstrumentationFunction::InstrumentationFunction(uint32_t idx, char* funcName, uint64_t dataoffset)
    : Instrumentation(ElfClassTypes_InstrumentationFunction)
{
    index = idx;

    functionName = new char[strlen(funcName)+1];
    strcpy(functionName,funcName);

    procedureLinkOffset = 0;
    bootstrapOffset = 0;
    wrapperOffset = 0;

    globalData = 0;
    globalDataOffset = dataoffset;

    numberOfArguments = 0;
    argumentOffsets = NULL;
    argumentValues = NULL;
}

InstrumentationFunction::~InstrumentationFunction(){
    if (functionName){
        delete[] functionName;
    }
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        delete procedureLinkInstructions[i];
    }
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        delete wrapperInstructions[i];
    }
    if (argumentOffsets){
        delete[] argumentOffsets;
    }
    if (argumentValues){
        delete[] argumentValues;
    }
}

void InstrumentationFunction::print(){
    PRINT_INFOR("Instrumentation Function (%d) %s", index, functionName);
    PRINT_INFOR("\t\tProcedure Link Instructions: %d", procedureLinkInstructions.size());
    PRINT_INFOR("\t\tProcedure Link Offset      : %lld", procedureLinkOffset);
    for (uint32_t i = 0; i < procedureLinkInstructions.size(); i++){
        procedureLinkInstructions[i]->print();
    }
    PRINT_INFOR("\t\tBootstrap Instructions     : %d", bootstrapInstructions.size());
    PRINT_INFOR("\t\tBootstrap Offset           : %lld", bootstrapOffset);
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->print();
    }
    PRINT_INFOR("\t\tWrapper Instructions       : %d", wrapperInstructions.size());
    PRINT_INFOR("\t\tWrapper Offset             : %lld", wrapperOffset);
    for (uint32_t i = 0; i < wrapperInstructions.size(); i++){
        wrapperInstructions[i]->print();
    }
    PRINT_INFOR("\t\tGlobal Data                : %x", globalData);
    PRINT_INFOR("\t\tGlobal Data Offset         : %lld", globalDataOffset);

}

uint32_t InstrumentationSnippet::generateSnippetControl(){
    snippetInstructions.append(Instruction::generateReturn());
    return snippetInstructions.size();
}

void InstrumentationSnippet::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = bootstrapOffset;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += bootstrapInstructions[i]->getSizeInBytes();
    }
    currentOffset = snippetOffset;
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        snippetInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += snippetInstructions[i]->getSizeInBytes();
    }
}


void InstrumentationSnippet::print(){
    __FUNCTION_NOT_IMPLEMENTED;
}

uint32_t InstrumentationSnippet::dataSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfDataEntries; i++){
        totalSize += dataEntrySizes[i];
    }
    return totalSize;
}

uint32_t InstrumentationSnippet::bootstrapSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < bootstrapInstructions.size(); i++){
        totalSize += bootstrapInstructions[i]->getSizeInBytes();
    }
    return totalSize;    
}

uint32_t InstrumentationSnippet::snippetSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        totalSize += snippetInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}

uint32_t InstrumentationSnippet::reserveData(uint64_t offset, uint32_t size){
    uint32_t* newSizes = new uint32_t[numberOfDataEntries+1];
    uint64_t* newOffsets = new uint64_t[numberOfDataEntries+1];

    memcpy(newSizes,dataEntrySizes,sizeof(uint32_t)*numberOfDataEntries);
    memcpy(newOffsets,dataEntryOffsets,sizeof(uint64_t)*numberOfDataEntries);

    newSizes[numberOfDataEntries] = size;
    newOffsets[numberOfDataEntries] = offset;

    if (dataEntrySizes){
        delete[] dataEntrySizes;
    }
    if (dataEntryOffsets){
        delete[] dataEntryOffsets;
    }

    dataEntrySizes = newSizes;
    dataEntryOffsets = newOffsets;

    numberOfDataEntries++;
    return numberOfDataEntries-1;
}

void InstrumentationSnippet::initializeReservedData(uint32_t idx, uint32_t size, char* buff, uint64_t dataBaseAddress){
    ASSERT(idx < numberOfDataEntries && size == dataEntrySizes[idx]);

    // set CX to the base address of this data entry
    bootstrapInstructions.append(Instruction::generateMoveImmToReg(dataBaseAddress + dataEntryOffsets[idx], X86_REG_CX));

    // copy each byte to memory
    for(uint32_t i = 0; i < size; i++){
        bootstrapInstructions.append(Instruction::generateMoveImmByteToMemIndirect(buff[size],i,X86_REG_CX));
    }
}

InstrumentationSnippet::InstrumentationSnippet()
    : Instrumentation(ElfClassTypes_InstrumentationSnippet)
{
    snippetOffset = 0;    

    numberOfDataEntries = 0;
    dataEntrySizes = NULL;
    dataEntryOffsets = NULL;
}

InstrumentationSnippet::~InstrumentationSnippet(){
    for (uint32_t i = 0; i < snippetInstructions.size(); i++){
        delete snippetInstructions[i];
    }
}

Vector<Instruction*>* InstrumentationPoint::swapInstructionsAtPoint(Vector<Instruction*>* replacements){
    return point->swapInstructions(getSourceAddress(),replacements);
    return NULL;
}

void InstrumentationPoint::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currentOffset = trampolineOffset;
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        trampolineInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
        currentOffset += trampolineInstructions[i]->getSizeInBytes();
    }
}

uint32_t InstrumentationPoint::sizeNeeded(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        totalSize += trampolineInstructions[i]->getSizeInBytes();
    }
    return totalSize;
}

uint32_t InstrumentationPoint::generateTrampoline(Vector<Instruction*>* insts, uint64_t textBaseAddress, uint64_t offset, uint64_t returnOffset, bool is64bit, bool doReloc, bool jumpToSource){
    ASSERT(!trampolineInstructions.size() && "Cannot generate trampoline instructions more than once");

    trampolineOffset = offset;

    uint32_t trampolineSize = 0;
    if (is64bit){
        trampolineInstructions.append(Instruction64::generateRegSubImmediate(X86_REG_SP,TRAMPOLINE_FRAME_AUTOINC_SIZE));
    } else {
        trampolineInstructions.append(Instruction32::generateRegSubImmediate(X86_REG_SP,TRAMPOLINE_FRAME_AUTOINC_SIZE));
    }
    trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    PRINT_DEBUG_INST("Generating relative call for trampoline %#llx + %d, %#llx", offset, trampolineSize, getTargetOffset());
    trampolineInstructions.append(Instruction::generateCallRelative(offset+trampolineSize,getTargetOffset()));
    trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    if (is64bit){
        trampolineInstructions.append(Instruction64::generateRegAddImmediate(X86_REG_SP,TRAMPOLINE_FRAME_AUTOINC_SIZE));
    } else {
        trampolineInstructions.append(Instruction32::generateRegAddImmediate(X86_REG_SP,TRAMPOLINE_FRAME_AUTOINC_SIZE));
    }
    trampolineSize += trampolineInstructions.back()->getSizeInBytes();

    uint64_t displacementDist = returnOffset - (offset + trampolineSize + numberOfBytes);

    if (doReloc){
        ASSERT(insts);
#ifdef DEBUG_FUNC_RELOC
        if ((*insts).size()){
            PRINT_DEBUG_FUNC_RELOC("Moving instructions from %#llx to %#llx for relocation", (*insts)[0]->getProgramAddress(), (*insts)[0]->getBaseAddress());
        }
#endif
        int32_t numberOfBranches = 0;
        for (uint32_t i = 0; i < (*insts).size(); i++){
            if ((*insts)[i]->isControl()){
                numberOfBranches++;
                if ((*insts)[i]->bytesUsedForTarget() < sizeof(uint32_t)){
                    PRINT_DEBUG_FUNC_RELOC("This instruction uses %d bytes for target calculation", (*insts)[i]->bytesUsedForTarget());
                    (*insts)[i]->convertTo4ByteOperand();
                }
            }
            (*insts)[i]->setBaseAddress(textBaseAddress+offset+trampolineSize);
            trampolineInstructions.append((*insts)[i]);
            trampolineSize += trampolineInstructions.back()->getSizeInBytes();
#ifdef DEBUG_FUNC_RELOC
            if ((*insts).size()){
                PRINT_DEBUG_FUNC_RELOC("Moving instructions from %#llx to %#llx for trampoline", (*insts)[0]->getProgramAddress(), (*insts)[0]->getBaseAddress());
            }
#endif
        }
        
        ASSERT(numberOfBranches < 2 && "Cannot have multiple branches in a basic block");
    }
    if (jumpToSource){
        trampolineInstructions.append(Instruction::generateJumpRelative(offset+trampolineSize,returnOffset));
        trampolineSize += trampolineInstructions.back()->getSizeInBytes();
    }

    return trampolineSize;
}

uint64_t InstrumentationPoint::getSourceAddress(){
    //    PRINT_INFOR("Type is %d %d", point->getClassType(), ((Base*)point)->getType());
    //    uint64_t sourceAddress = point->findInstrumentationPoint(numberOfBytes,instLocation);
    uint64_t sourceAddress = 0;
    //    if (point->getClassType() != ElfClassTypes_Function){
    if (1){
        sourceAddress = point->findInstrumentationPoint(numberOfBytes,instLocation);
    }
    return sourceAddress;
}

InstrumentationPoint::InstrumentationPoint(Base* pt, Instrumentation* inst, uint32_t size, InstLocations loc)
    : Base(ElfClassTypes_InstrumentationPoint)
{
    point = pt;

    instrumentation = inst;
    numberOfBytes = size;
    instLocation = loc;

    trampolineOffset = 0;

    verify();
}

bool InstrumentationPoint::verify(){
    if (!point->containsProgramBits()){
        PRINT_ERROR("Instrumentation point not allowed to be type %d", point->getType());
        return false;
    }
    return true;
}

InstrumentationPoint::~InstrumentationPoint(){
    for (uint32_t i = 0; i < trampolineInstructions.size(); i++){
        delete trampolineInstructions[i];
    }
}

void InstrumentationPoint::print(){
    __FUNCTION_NOT_IMPLEMENTED;
}
