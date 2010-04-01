#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <InstrumentationCommon.h>

int32_t numberOfCallSites;
char** functionNames;

FILE* logfile;

#include <IOWrappers.c>

int32_t inittracer(int32_t* numSites, char** funcNames, char* execName){
    numberOfCallSites = *numSites;
    functionNames = funcNames;

    char logname[__MAX_STRING_SIZE];
    sprintf(logname, "%s.log.%d", execName, getpid());
    logfile = fopen(logname, "w");

    PRINT_INSTR(stdout, "Real args inittracer: %x %x", numSites, funcNames);
}

int32_t functiontracer(int32_t* idx, int64_t* args){
    PRINT_INSTR(stdout, "Real args functiontracer: %x %x", idx, args);
    PRINT_INSTR(stdout, "Call site for %s (%d)", functionNames[*idx], *idx);

    // a macro from IOWrappers.h
    __all_wrapper_decisions
}

int32_t finishtracer(){
    int i;
    PRINT_INSTR(stdout, "number of call sites %d", numberOfCallSites);

    fclose(logfile);
}

