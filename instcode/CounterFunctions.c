#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#define PRINT_MINIMUM 1

#define PRINT_INSTR(...) fprintf(stdout, "-[p%d]- ", getpid()); \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
    fflush(stdout);

int32_t numberOfBasicBlocks;
int32_t* lineNumbers;
char** fileNames;
char** functionNames;
int64_t* hashValues;

int32_t dummywrap(){
    int32_t x = initcounter(1,2,3,4,5) +
        functioncounter(1,2,3) + 
        blockcounter(1,2,3);
    return x;
}

int32_t functioncounter(int32_t* numFunctions, int32_t* functionCounts, char** functionNames){
    int32_t i;

    PRINT_INSTR("*** Instrumentation Summary ****");
    PRINT_INSTR("raw args: %x %x %x", numFunctions, functionCounts, functionNames);
    PRINT_INSTR("There are %d functions in the code:", *numFunctions);
    PRINT_INSTR("Printing functions with at least %d executions", PRINT_MINIMUM);

    for (i = 0; i < *numFunctions; i++){
        if (functionCounts[i] >= PRINT_MINIMUM){
            PRINT_INSTR("\tFunction(%d) %.24s executed %d times", i, functionNames[i], functionCounts[i]);
        }
    }
    return i;
}

int32_t initcounter(int32_t* numBlocks, int32_t* lineNums, char** fileNms, char** functionNms, int64_t* hashVals){
    //    PRINT_INSTR("raw init args: %x %x %x %x %llx", numBlocks, lineNums, fileNms, functionNms, hashVals);

    numberOfBasicBlocks = *numBlocks;
    lineNumbers = lineNums;
    fileNames = fileNms;
    functionNames = functionNms;
    hashValues = hashVals;

    return numberOfBasicBlocks;
}

int32_t blockcounter(int32_t* blockCounts, char* appName, char* instExt){
    int32_t i;

    PRINT_INSTR("raw fini args: %x %x %x", blockCounts, appName, instExt);
    PRINT_INSTR("*** Instrumentation Summary ****");
    PRINT_INSTR("There are %d basic blocks in the code:", numberOfBasicBlocks);
    PRINT_INSTR("Printing blocks with at least %d executions", PRINT_MINIMUM);

    char* outFileName = malloc(sizeof(char)* 80);
    sprintf(outFileName, "%s.%d.%s", appName, getpid(), instExt);
    PRINT_INSTR("Writing output file %s", outFileName);
    FILE* outFile = fopen(outFileName, "w");
    free(outFileName);
    if (!outFile){
        fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
        fflush(stderr);
        exit(-1);
    }

    fprintf(outFile, "#hash\tcount\t#file:line\tfunc\n");
    for (i = 0; i < numberOfBasicBlocks; i++){
        if (blockCounts[i] >= PRINT_MINIMUM){
            fprintf(outFile, "%#lld\t%d\t#%s:%d\t%s\n", hashValues[i], blockCounts[i], fileNames[i], lineNumbers[i], functionNames[i]);
        }
    }
    fflush(outFile);
    fclose(outFile);

    return i;
}

