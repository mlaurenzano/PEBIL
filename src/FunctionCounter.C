#include <FunctionCounter.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <InstructionGenerator.h>
#include <LineInformation.h>
#include <TextSection.h>

#define EXIT_FUNCTION "functioncounter"
#define LIB_NAME "libcounter.so"
#define NOINST_VALUE 0xffffffff

FunctionCounter::FunctionCounter(ElfFile* elf, char* inputFuncList)
    : InstrumentationTool(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", "fncinst");
}

void FunctionCounter::declare(){
}

void FunctionCounter::instrument(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
