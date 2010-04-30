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
int32_t nameIsSet;
char* appName;

#include <IOWrappers.c>

int32_t inittracer(int32_t* numSites, char** funcNames, char* execName){
    numberOfCallSites = *numSites;
    functionNames = funcNames;
    nameIsSet = 0;
    logfile = NULL;
    appName = execName;
//    PRINT_INSTR(stdout, "Real args inittracer: %x %x", numSites, funcNames);
}

int32_t functionentry(){
  if (!nameIsSet){
    char logname[__MAX_STRING_SIZE];
    sprintf(logname, "%s.log.%d", appName, getpid());
    logfile = fopen(logname, "w");
    logfile = stdout;
    nameIsSet = 1;
  }
  assert(logfile);
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

    //    fclose(logfile);
}

