#include <CallReplace.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>

#define PROGRAM_ENTRY  "initwrapper"
#define PROGRAM_EXIT   "finishwrapper"
#define INST_LIB_NAME "libtracer.so"

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

CallReplace::CallReplace(ElfFile* elf, char* traceFile)
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
}

void CallReplace::declare(){
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);

    for (uint32_t i = 0; i < (*functionList).size(); i++){
        functionWrappers.append(declareFunction(getWrapperName(i)));
    }
}


void CallReplace::instrument(){
    uint32_t temp32;
    uint64_t temp64;

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

    Vector<X86Instruction*> myInstPoints;
    Vector<uint32_t> myInstList;

    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());

            if (functionSymbol){
                uint32_t funcIdx = searchFileList(functionList, functionSymbol->getSymbolName());
                if (funcIdx < (*functionList).size()){
                    BasicBlock* bb = function->getBasicBlockAtAddress(instruction->getBaseAddress());
                    ASSERT(bb->containsCallToRange(0,-1));
                    ASSERT(instruction->getSizeInBytes() == Size__uncond_jump);

                    myInstPoints.append(instruction);
                    myInstList.append(funcIdx);
                }
            }
        } 
    }
    ASSERT(myInstPoints.size() == myInstList.size());

    for (uint32_t i = 0; i < myInstPoints.size(); i++){
        PRINT_INFOR("%#llx: replacing call %s -> %s", myInstPoints[i]->getBaseAddress(), getFunctionName(myInstList[i]), getWrapperName(myInstList[i]));
        InstrumentationPoint* pt = addInstrumentationPoint(myInstPoints[i], functionWrappers[myInstList[i]], InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
    }
}

