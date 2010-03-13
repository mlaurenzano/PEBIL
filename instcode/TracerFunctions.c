#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

int32_t numberOfCallSites;

int32_t inittracer(int32_t* numSites){
    numberOfCallSites = *numSites;
}

int32_t functiontracer(int32_t* idx){
    PRINT_INSTR("Call site %d", *idx);
}

int32_t finishtracer(){
    PRINT_INSTR("number of call sites %d", numberOfCallSites);
}

