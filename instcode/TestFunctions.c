#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

int32_t functioncounter(void* arg1, void* arg2, void* arg3){
    int32_t i;
    int32_t numFunctions = *(int32_t*)arg1;
    int32_t* functionCounts = (int32_t*)(arg2);
    char** functionNames = *(char**)arg3;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "Function arguments: %x %x %x\n", arg1, arg2, arg3);
    fprintf(stdout, "There are %d functions in the code:\n", numFunctions);

    for (i = 0; i < numFunctions; i++){
        fprintf(stdout, "\tFunction(%d) -- %x -- %s -- executed %d times\n", i, functionNames[i], functionNames[i], functionCounts[i]);
    }

    fflush(stdout);
    return 3;
}

void secondtest(){
    int32_t i;
    for (i = 0; i < 3; i++){
        fprintf(stdout, "shipping loop index %d\n", i);
    }
    fflush(stdout);
}
