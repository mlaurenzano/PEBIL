#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define NOINST_VALUE 0xffffffff


int32_t functioncounter(void* arg1, void* arg2, void* arg3){
    int32_t i;
    int32_t numFunctions = *(int32_t*)arg1;
    int32_t* functionCounts = (int32_t*)(arg2);
    char** functionNames = *(char**)arg3;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "Raw instrumentation function arguments: %x %x %x\n", arg1, arg2, arg3);
    fprintf(stdout, "There are %d functions in the code:\n", numFunctions);

    for (i = 0; i < numFunctions; i++){
        fprintf(stdout, "\tFunction(%d) -- %x -- %s -- executed %d times\n", i, functionNames[i], functionNames[i], functionCounts[i]);
    }

    fflush(stdout);
    return 0;
}

int32_t blockcounter(void* arg1, void* arg2, void* arg3, void* arg4, void* arg5){
    int32_t i;
    int32_t excluded = 0;

    // interpret the arguments
    int32_t numBlocks = *(int32_t*)arg1;
    int32_t* functionCounts = (int32_t*)(arg2);
    int64_t* blockAddrs = (int64_t*)(arg3);
    int32_t* lineNumbers = (int32_t*)(arg4);
    char** fileNames = (char**)arg5;
    
    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "Raw instrumentation function arguments: %x %x %x %x %x\n", arg1, arg2, arg3, arg4, arg5);
    fprintf(stdout, "There are %d basic blocks in the code:\n", numBlocks);

    fprintf(stdout, "Index\t\tAddress\t\tLine\t\tCounter\n");
    for (i = 0; i < numBlocks; i++){
        fprintf(stdout, "%d\t0x%016llx\t%s:%d\t%d\n", i, blockAddrs[i], fileNames[i], lineNumbers[i], functionCounts[i]);
        if (functionCounts[i] == NOINST_VALUE){
            excluded++;
        }
    }
    fprintf(stdout, "NOTE -- %d blocks weren't instrumented, these are denoted by a counter value of %d (0x%x)\n\n", excluded, NOINST_VALUE, NOINST_VALUE);

    fflush(stdout);
    return 0;
}


