#include <CallReplace.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>

#define PROGRAM_ENTRY  "initwrapper"
#define PROGRAM_EXIT   "finishwrapper"
#define NOSTRING "__pebil_no_string__"

CallReplace::~CallReplace(){
    for (uint32_t i = 0; i < (*functionList).size(); i++){
        delete[] (*functionList)[i];
    }
    delete functionList;
}

char* CallReplace::getFunctionName(uint32_t idx){
    ASSERT(idx < (*functionList).size());
    return (*functionList)[idx];

}
char* CallReplace::getWrapperName(uint32_t idx){
    ASSERT(idx < (*functionList).size());
    char* both = (*functionList)[idx];
    both += strlen(both) + 1;
    return both;
}

CallReplace::CallReplace(ElfFile* elf, char* traceFile, char* libList)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;

    functionList = new Vector<char*>();
    initializeFileList(traceFile, functionList);

    // replace any ':' character with a '\0'
    for (uint32_t i = 0; i < (*functionList).size(); i++){
        char* both = (*functionList)[i];
        uint32_t numrepl = 0;
        for (uint32_t j = 0; j < strlen(both); j++){
            if (both[j] == ':'){
                both[j] = '\0';
                numrepl++;
            }
        }
        if (numrepl != 1){
            PRINT_ERROR("input file %s line %d should contain a ':'", traceFile, i+1);
        }
    }

    Vector<uint32_t> libIdx;
    libIdx.append(0);
    uint32_t listSize = strlen(libList);
    for (uint32_t i = 0; i < listSize; i++){
        if (libList[i] == ','){
            libList[i] = '\0';
            libIdx.append(i+1);
        }
    }
    for (uint32_t i = 0; i < libIdx.size(); i++){
        if (libIdx[i] < listSize){
            libraries.append(libList + libIdx[i]);
        } else {
            PRINT_ERROR("improperly formatted library list given to call replacement tool");
            __SHOULD_NOT_ARRIVE;
        }
    }

    for (uint32_t i = 0; i < libraries.size(); i++){
        PRINT_INFOR("library -- %s", libraries[i]);
    }

}

void CallReplace::declare(){
    // declare any shared library that will contain instrumentation functions
    for (uint32_t i = 0; i < libraries.size(); i++){
        declareLibrary(libraries[i]);
    }

    // declare any instrumentation functions that will be used
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);

    for (uint32_t i = 0; i < (*functionList).size(); i++){
        functionWrappers.append(declareFunction(getWrapperName(i)));
        functionWrappers.back()->setSkipWrapper();
    }
}


void CallReplace::instrument(){
    uint32_t temp32;
    uint64_t temp64;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    uint64_t siteIndex = reserveDataOffset(sizeof(uint32_t));
    programEntry->addArgument(siteIndex);

    Vector<X86Instruction*> myInstPoints;
    Vector<uint32_t> myInstList;
    Vector<LineInfo*> myLineInfos;
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());

            if (functionSymbol){
                //PRINT_INFOR("looking for function %s", functionSymbol->getSymbolName());
                uint32_t funcIdx = searchFileList(functionList, functionSymbol->getSymbolName());
                if (funcIdx < (*functionList).size()){
                    BasicBlock* bb = function->getBasicBlockAtAddress(instruction->getBaseAddress());
                    ASSERT(bb->containsCallToRange(0,-1));
                    ASSERT(instruction->getSizeInBytes() == Size__uncond_jump);

                    myInstPoints.append(instruction);
                    myInstList.append(funcIdx);
                    LineInfo* li = NULL;
                    if (lineInfoFinder){
                        li = lineInfoFinder->lookupLineInfo(bb);
                    }
                    myLineInfos.append(li);
                }
            }
        } 
    }
    ASSERT(myInstPoints.size() == myInstList.size());
    ASSERT(myLineInfos.size() == myInstList.size());

    uint64_t fileNames = reserveDataOffset(sizeof(char*) * myInstList.size());
    uint64_t lineNumbers = reserveDataOffset(sizeof(uint32_t) * myInstList.size());
    for (uint32_t i = 0; i < myInstList.size(); i++){
        uint32_t line = 0;
        char* fname = NOSTRING;
        if (myLineInfos[i]){
            line = myLineInfos[i]->GET(lr_line);
            fname = myLineInfos[i]->getFileName();
        }
        uint64_t filenameaddr = getInstDataAddress() + reserveDataOffset(strlen(fname) + 1);
        initializeReservedData(getInstDataAddress() + fileNames + i*sizeof(char*), sizeof(char*), &filenameaddr);
        initializeReservedData(filenameaddr, strlen(fname), fname);
        initializeReservedData(getInstDataAddress() + lineNumbers + i*sizeof(uint32_t), sizeof(uint32_t), &line);
    }
    programEntry->addArgument(fileNames);
    programEntry->addArgument(lineNumbers);

    for (uint32_t i = 0; i < myInstPoints.size(); i++){
        PRINT_INFOR("(site %d) %#llx: replacing call %s -> %s in function %s", i, myInstPoints[i]->getBaseAddress(), getFunctionName(myInstList[i]), getWrapperName(myInstList[i]), myInstPoints[i]->getContainer()->getName());
        InstrumentationPoint* pt = addInstrumentationPoint(myInstPoints[i], functionWrappers[myInstList[i]], InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        if (getElfFile()->is64Bit()){
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(i, X86_REG_CX));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
        } else {
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(i, X86_REG_CX));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));            
        }
    }
}

