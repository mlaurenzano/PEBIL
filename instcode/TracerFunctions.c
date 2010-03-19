#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

int32_t numberOfCallSites;
char** functionNames;

int32_t inittracer(int32_t* numSites, char** funcNames){
    numberOfCallSites = *numSites;
    functionNames = funcNames;

    PRINT_INSTR("Real args: %x %x", numSites, funcNames);
}

int32_t functiontracer(int32_t* idx){
    PRINT_INSTR("Call site for %s", functionNames[*idx]);
}

int32_t finishtracer(){
    int i;

    PRINT_INSTR("number of call sites %d", numberOfCallSites);
}

