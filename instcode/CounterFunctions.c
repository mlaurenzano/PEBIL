#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define PRINT_MINIMUM 1

int32_t functioncounter(int32_t* numFunctions, int32_t* functionCounts, char** functionNames){
    int32_t i;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
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

int32_t blockcounter(int32_t* numBlocks, int32_t* blockCounts, int64_t* blockAddrs, int32_t* lineNumbers, char** fileNames){
    int32_t i;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "There are %d basic blocks in the code:\n", *numBlocks);
    fprintf(stdout, "Printing blocks with at least %d executions\n", PRINT_MINIMUM);

    fprintf(stdout, "Index\t\tAddress\t\tLine\t\tCounter\n");
    for (i = 0; i < *numBlocks; i++){
        if (blockCounts[i] >= PRINT_MINIMUM){
            fprintf(stdout, "%d\t0x%016llx\t%s:%d\t%d\n", i, blockAddrs[i], fileNames[i], lineNumbers[i], blockCounts[i]);
        }
    }
    fflush(stdout);
    return 0;
}


