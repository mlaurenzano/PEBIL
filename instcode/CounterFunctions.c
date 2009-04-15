#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define PRINT_MINIMUM 1000

int32_t functioncounter(int32_t* numFunctions, int32_t* functionCounts, char** functionNames){
    int32_t i;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fflush(stdout);
    fprintf(stdout, "raw args: %x %x %x\n", numFunctions, functionCounts, functionNames);
    fprintf(stdout, "There are %d functions in the code:\n", *numFunctions);
    fprintf(stdout, "Printing functions with at least %d executions\n", PRINT_MINIMUM);

    for (i = 0; i < *numFunctions; i++){
        if (functionCounts[i] >= PRINT_MINIMUM){
            fprintf(stdout, "\tFunction(%d) %.24s executed %d times\n", i, functionNames[i], functionCounts[i]);
        }
    }

    fflush(stdout);
    return 0;
}

int32_t blockcounter(int32_t* numBlocks, int32_t* blockCounts, int32_t* lineNumbers, char** fileNames, char** funcNames){
    int32_t i;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fflush(stdout);

    /*
    fprintf(stdout, "raw args: %x %x %x %x %x\n", numBlocks, blockCounts, lineNumbers, fileNames, funcNames);
    fprintf(stdout, "There are %d basic blocks in the code:\n", *numBlocks);
    fprintf(stdout, "Printing blocks with at least %d executions\n", PRINT_MINIMUM);

    fprintf(stdout, "Index\tFunction\tFile:Line\tCounter\n");
    for (i = 0; i < *numBlocks; i++){
        if (blockCounts[i] >= PRINT_MINIMUM){
            fprintf(stdout, "%d\t%s\t%s:%d\t%d\n", i, funcNames[i], fileNames[i], lineNumbers[i], blockCounts[i]);
        }
    }
    fflush(stdout);
    */
    return 0;
}


