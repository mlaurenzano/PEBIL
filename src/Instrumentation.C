#include <Instrumentation.h>
#include <ElfFileInst.h>
#include <Instruction.h>
#include <TextSection.h>
#include <Function.h>

uint32_t InstrumentationFunction::wrapperSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
        totalSize += wrapperInstructions[i]->getLength();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::procedureLinkSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfProcedureLinkInstructions; i++){
        totalSize += procedureLinkInstructions[i]->getLength();
    }
    return totalSize;
}
uint32_t Instrumentation::bootstrapSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
        totalSize += bootstrapInstructions[i]->getLength();
    }
    return totalSize;
}
uint32_t InstrumentationFunction::globalDataSize(){
    return Size__32_bit_Global_Offset_Table_Entry;
}

Instrumentation::Instrumentation(ElfClassTypes typ)
    : Base(typ)
{
    numberOfBootstrapInstructions = 0;
    bootstrapInstructions = NULL;
    bootstrapOffset = 0;
}

Instrumentation::~Instrumentation(){
    if (bootstrapInstructions){
        for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
            delete bootstrapInstructions[i];
        }
        delete[] bootstrapInstructions;
    }
}

uint32_t InstrumentationFunction::addArgument(ElfArgumentTypes typ, uint64_t offset){
    return addArgument(typ, offset, 0);
}

uint32_t InstrumentationFunction::addArgument(ElfArgumentTypes typ, uint64_t offset, uint32_t value){
    ElfArgumentTypes* newargs = new ElfArgumentTypes[numberOfArguments+1];
    uint64_t* newoffsets = new uint64_t[numberOfArguments+1];
    uint32_t* newvalues = new uint32_t[numberOfArguments+1];

    for (uint32_t i = 0; i < numberOfArguments; i++){
        newargs[i] = arguments[i];
        newoffsets[i] = argumentOffsets[i];
        newvalues[i] = argumentValues[i];
    }

    newargs[numberOfArguments] = typ;
    newoffsets[numberOfArguments] = offset;
    newvalues[numberOfArguments] = value;

    delete[] arguments;
    delete[] argumentOffsets;
    delete[] argumentValues;

    arguments = newargs;
    argumentOffsets = newoffsets;
    argumentValues = newvalues;

    numberOfArguments++;
    return numberOfArguments;
}

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
    return snippetOffset;
}


uint32_t InstrumentationFunction64::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions && "This array should not be initialized");

    uint64_t procedureLinkAddress = textBaseAddress + procedureLinkOffset;
    addProcedureLinkInstruction(Instruction64::generateIndirectRelativeJump(procedureLinkAddress,dataBaseAddress + globalDataOffset));
    addProcedureLinkInstruction(Instruction64::generateStackPushImmediate(relocationOffset));

    uint32_t returnAddress = procedureLinkAddress + procedureLinkSize();
    addProcedureLinkInstruction(Instruction64::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());
    return numberOfProcedureLinkInstructions;
}

uint32_t InstrumentationFunction32::generateProcedureLinkInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress, uint64_t realPLTAddress){
    ASSERT(!procedureLinkInstructions && "This array should not be initialized");

    addProcedureLinkInstruction(Instruction32::generateJumpIndirect(dataBaseAddress + globalDataOffset));
    addProcedureLinkInstruction(Instruction32::generateStackPushImmediate(relocationOffset));
    uint32_t returnAddress = textBaseAddress + procedureLinkOffset + procedureLinkSize();
    addProcedureLinkInstruction(Instruction32::generateJumpRelative(returnAddress,realPLTAddress));

    ASSERT(procedureLinkSize() == procedureLinkReservedSize());

    return numberOfProcedureLinkInstructions;
}

uint32_t InstrumentationFunction64::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    addBootstrapInstruction(Instruction64::generateMoveImmToReg(globalData,X86_REG_CX));
    addBootstrapInstruction(Instruction64::generateMoveImmToReg(dataBaseAddress + globalDataOffset,X86_REG_DX));
    addBootstrapInstruction(Instruction::generateMoveRegToRegaddr(X86_REG_CX,X86_REG_DX));

    while (bootstrapSize() < bootstrapReservedSize()){
        addBootstrapInstruction(Instruction64::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return numberOfBootstrapInstructions;
}

uint32_t InstrumentationFunction32::generateBootstrapInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    addBootstrapInstruction(Instruction32::generateMoveImmToReg(globalData,X86_REG_CX));
    addBootstrapInstruction(Instruction32::generateMoveRegToMem(X86_REG_CX,dataBaseAddress + globalDataOffset));

    while (bootstrapSize() < bootstrapReservedSize()){
        addBootstrapInstruction(Instruction32::generateNoop());
    }
    ASSERT(bootstrapSize() == bootstrapReservedSize());
    return numberOfBootstrapInstructions;
}

uint32_t InstrumentationFunction64::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions && "This array should not be initialized");

    addWrapperInstruction(Instruction64::generatePushEflags());
    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        addWrapperInstruction(Instruction64::generateStackPush(i));
    }

    ASSERT(numberOfArguments < MAX_ARGUMENTS_64BIT && "More arguments must be pushed onto stack, which is not yet implemented"); 

    for (uint32_t i = 0; i < numberOfArguments; i++){
        uint32_t idx = numberOfArguments - i - 1;
        uint32_t value = argumentValues[idx];

        uint32_t argumentRegister;

        // rdi,rsi,rdx,rcx,r8,r9, then push onto stack
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
        addWrapperInstruction(Instruction64::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],argumentRegister));

        addBootstrapInstruction(Instruction64::generateMoveImmToReg(value,X86_REG_CX));
        addBootstrapInstruction(Instruction64::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],X86_REG_DX));
        addBootstrapInstruction(Instruction::generateMoveRegToRegaddr(X86_REG_CX,X86_REG_DX));
    }
    addWrapperInstruction(Instruction64::generateCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    for (uint32_t i = 0; i < X86_64BIT_GPRS; i++){
        addWrapperInstruction(Instruction64::generateStackPop(X86_64BIT_GPRS-1-i));
    }
    addWrapperInstruction(Instruction64::generatePopEflags());

    addWrapperInstruction(Instruction64::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        addWrapperInstruction(Instruction64::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return numberOfWrapperInstructions;
}

uint32_t InstrumentationFunction32::generateWrapperInstructions(uint64_t textBaseAddress, uint64_t dataBaseAddress){
    ASSERT(!wrapperInstructions && "This array should not be initialized");

    addWrapperInstruction(Instruction32::generatePushEflags());
    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        addWrapperInstruction(Instruction32::generateStackPush(i));
    }

    for (uint32_t i = 0; i < numberOfArguments; i++){
        uint32_t idx = numberOfArguments - i - 1;
        uint32_t value = argumentValues[idx];

        // everything is passed on the stack
        addWrapperInstruction(Instruction32::generateMoveImmToReg(dataBaseAddress+argumentOffsets[idx],X86_REG_DX));
        addWrapperInstruction(Instruction32::generateStackPush(X86_REG_DX));

        addBootstrapInstruction(Instruction32::generateMoveImmToReg(value,X86_REG_CX));
        addBootstrapInstruction(Instruction32::generateMoveRegToMem(X86_REG_CX,dataBaseAddress+argumentOffsets[idx]));
    }
    addWrapperInstruction(Instruction32::generateCallRelative(wrapperOffset + wrapperSize(), procedureLinkOffset));

    for (uint32_t i = 0; i < numberOfArguments; i++){
        addWrapperInstruction(Instruction32::generateStackPop(X86_REG_CX));
    }

    for (uint32_t i = 0; i < X86_32BIT_GPRS; i++){
        addWrapperInstruction(Instruction32::generateStackPop(X86_32BIT_GPRS-1-i));
    }
    addWrapperInstruction(Instruction32::generatePopEflags());

    addWrapperInstruction(Instruction32::generateReturn());

    while (wrapperSize() < wrapperReservedSize()){
        addWrapperInstruction(Instruction32::generateNoop());
    }
    ASSERT(wrapperSize() == wrapperReservedSize());

    return numberOfWrapperInstructions;
}

uint32_t InstrumentationFunction64::generateGlobalData(uint64_t textBaseAddress){
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

InstrumentationFunction::InstrumentationFunction(uint32_t idx, char* funcName, uint64_t dataoffset)
    : Instrumentation(ElfClassTypes_InstrumentationFunction)
{
    index = idx;

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
    globalDataOffset = dataoffset;

    numberOfArguments = 0;
    arguments = NULL;
    argumentOffsets = NULL;
    argumentValues = NULL;
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
    if (wrapperInstructions){
        for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
            delete wrapperInstructions[i];
        }
        delete[] wrapperInstructions;
    }
    if (arguments){
        delete[] arguments;
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

uint32_t InstrumentationSnippet::generateSnippetControl(){
    addSnippetInstruction(Instruction::generateReturn());
    return numberOfSnippetInstructions;
}

void InstrumentationSnippet::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (bootstrapInstructions){
        uint32_t currentOffset = bootstrapOffset;
        for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
            bootstrapInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += bootstrapInstructions[i]->getLength();
        }
    }
    if (snippetInstructions){
        uint32_t currentOffset = snippetOffset;
        for (uint32_t i = 0; i < numberOfSnippetInstructions; i++){
            snippetInstructions[i]->dump(binaryOutputFile,offset+currentOffset);
            currentOffset += snippetInstructions[i]->getLength();
        }
    }
}

void InstrumentationSnippet::setCodeOffsets(uint64_t btOffset, uint64_t spOffset){ 
    bootstrapOffset = btOffset; 
    snippetOffset = spOffset; 
}

void InstrumentationSnippet::print(){
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
    for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
        totalSize += bootstrapInstructions[i]->getLength();
    }
    return totalSize;    
}

uint32_t InstrumentationSnippet::snippetSize(){
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < numberOfSnippetInstructions; i++){
        totalSize += snippetInstructions[i]->getLength();
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
    addBootstrapInstruction(Instruction::generateMoveImmToReg(dataBaseAddress + dataEntryOffsets[idx], X86_REG_CX));

    // copy each byte to memory
    for(uint32_t i = 0; i < size; i++){
        addBootstrapInstruction(Instruction::generateMoveImmByteToMemIndirect(buff[size],i,X86_REG_CX));
    }
}

InstrumentationSnippet::InstrumentationSnippet()
    : Instrumentation(ElfClassTypes_InstrumentationSnippet)
{
    numberOfSnippetInstructions = 0;
    snippetInstructions = NULL;
    snippetOffset = 0;    

    numberOfDataEntries = 0;
    dataEntrySizes = NULL;
    dataEntryOffsets = NULL;
}

InstrumentationSnippet::~InstrumentationSnippet(){
    if (snippetInstructions){
        for (uint32_t i = 0; i < numberOfSnippetInstructions; i++){
            delete snippetInstructions[i];
        }
        delete[] snippetInstructions;
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

    delete[] insts;

    return trampolineSize;
}

InstrumentationPoint::InstrumentationPoint(Base* pt, Instrumentation* inst)
    : Base(ElfClassTypes_InstrumentationPoint)
{
    point = pt;
    if (point->getType() != ElfClassTypes_TextSection &&
        point->getType() != ElfClassTypes_Instruction &&
        point->getType() != ElfClassTypes_Function){
        PRINT_ERROR("Cannot use an object of type %d as an instrumentation point", point->getType());
        __SHOULD_NOT_ARRIVE;
    }
    instrumentation = inst;

    if (point->getType() == ElfClassTypes_TextSection){
        TextSection* ts = (TextSection*)point;
        sourceAddress = ts->findInstrumentationPoint();
    } else if (point->getType() == ElfClassTypes_Instruction){
        Instruction* in = (Instruction*)point;
        sourceAddress = in->getAddress();
    } else if (point->getType() == ElfClassTypes_Function){
        Function* fn = (Function*)point;
        sourceAddress = fn->findInstrumentationPoint();
    } else {
        PRINT_ERROR("Cannot use an object of type %d as an instrumentation point", point->getType());
    }

    trampolineInstructions = NULL;
    numberOfTrampolineInstructions = 0;
    trampolineOffset = 0;
}

InstrumentationPoint::~InstrumentationPoint(){
    if (trampolineInstructions){
        for (uint32_t i = 0; i < numberOfTrampolineInstructions; i++){
            delete trampolineInstructions[i];
        }
        delete[] trampolineInstructions;
    }
}

void InstrumentationPoint::print(){
    __FUNCTION_NOT_IMPLEMENTED;
}

uint32_t InstrumentationFunction::addProcedureLinkInstruction(Instruction* inst){
    Instruction** newinsts = new Instruction*[numberOfProcedureLinkInstructions+1];
    for (uint32_t i = 0; i < numberOfProcedureLinkInstructions; i++){
        newinsts[i] = procedureLinkInstructions[i];
    }
    newinsts[numberOfProcedureLinkInstructions] = inst;
    delete[] procedureLinkInstructions;
    procedureLinkInstructions = newinsts;
    numberOfProcedureLinkInstructions++;
    return numberOfProcedureLinkInstructions;
}

uint32_t Instrumentation::addBootstrapInstruction(Instruction* inst){
    Instruction** newinsts = new Instruction*[numberOfBootstrapInstructions+1];
    for (uint32_t i = 0; i < numberOfBootstrapInstructions; i++){
        newinsts[i] = bootstrapInstructions[i];
    }
    newinsts[numberOfBootstrapInstructions] = inst;
    delete[] bootstrapInstructions;
    bootstrapInstructions = newinsts;
    numberOfBootstrapInstructions++;
    return numberOfBootstrapInstructions;
}

uint32_t InstrumentationFunction::addWrapperInstruction(Instruction* inst){
    Instruction** newinsts = new Instruction*[numberOfWrapperInstructions+1];
    for (uint32_t i = 0; i < numberOfWrapperInstructions; i++){
        newinsts[i] = wrapperInstructions[i];
    }
    newinsts[numberOfWrapperInstructions] = inst;
    delete[] wrapperInstructions;
    wrapperInstructions = newinsts;
    numberOfWrapperInstructions++;
    return numberOfWrapperInstructions;
}

uint32_t InstrumentationSnippet::addSnippetInstruction(Instruction* inst){
    Instruction** newinsts = new Instruction*[numberOfSnippetInstructions+1];
    for (uint32_t i = 0; i < numberOfSnippetInstructions; i++){
        newinsts[i] = snippetInstructions[i];
    }
    newinsts[numberOfSnippetInstructions] = inst;
    delete[] snippetInstructions;
    snippetInstructions = newinsts;
    numberOfSnippetInstructions++;
    return numberOfSnippetInstructions;
}

