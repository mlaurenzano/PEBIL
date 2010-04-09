#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <InstrumentationCommon.h>

int32_t numberOfCallSites;
char** functionNames;

FILE* logfile;
int64_t timerstart;

#include <IOWrappers.c>

int32_t inittracer(int32_t* numSites, char** funcNames, char* execName){
    numberOfCallSites = *numSites;
    functionNames = funcNames;

    char logname[__MAX_STRING_SIZE];
    sprintf(logname, "%s.log.%d", execName, getpid());
    logfile = fopen(logname, "w");
    logfile = stdout;
    PRINT_INSTR(stdout, "Real args inittracer: %x %x", numSites, funcNames);
}

int32_t functionentry(){
    timerstart = readtsc();
}

int32_t functiontracer(int32_t* idx, int64_t* args){
    unsigned long long timerstop = readtsc();

    PRINT_INSTR(stdout, "Real args functiontracer: %x %x", idx, args);
    PRINT_INSTR(stdout, "Call site for %s (%d)", functionNames[*idx], *idx);

    unsigned long long tmr = (timerstop - timerstart);

    // a macro from IOWrappers.h
    __all_wrapper_decisions

    PRINT_INSTR(logfile, "function %s: %lld cycles", functionNames[*idx], tmr);
}

int32_t finishtracer(){
    int i;
    PRINT_INSTR(stdout, "number of call sites %d", numberOfCallSites);

    fclose(logfile);
}

