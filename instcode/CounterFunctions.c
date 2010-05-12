#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

#define PRINT_MINIMUM 1
#define FILTER 1

int32_t numberOfBasicBlocks;
int32_t* lineNumbers;
char** fileNames;
char** functionNames;
int64_t* hashValues;

int32_t filter = 0;

DINT_TYPE printmemory(DINT_TYPE* memory, DINT_TYPE* base, DINT_TYPE* offset, DINT_TYPE* index, DINT_TYPE* scale){
    DINT_TYPE memloc = *memory;
    DINT_TYPE memval;
    //    PRINT_INSTR(stdout, "raw args: m[%x]=%#x m[%x]=%d m[%x]=%d m[%x]=%d m[%x]=%x", memory, *memory, base, *base, offset, *offset, index, *index, scale, *scale);
    if (memloc == 0){
        //        PRINT_INSTR(stdout, "raw args: m[%x]=%d m[%x]=%d m[%x]=%d m[%x]=%d m[%x]=%d", memory, memval, base, *base, offset, *offset, index, *index, scale, *scale);
        memval = 0;
    } else {
        memval = *((DINT_TYPE*)memloc);
    }
    if (filter % FILTER == 0){
        //        PRINT_INSTR(stdout, "iteration %d; mem[%#llx]\t%#llx", filter, memloc, memval);
    }
    filter++;
    return 0;
}


int32_t functioncounter(int32_t* numFunctions, int32_t* functionCounts, char** functionNames){
    int32_t i;

    PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
    PRINT_INSTR(stdout, "raw args: %x %x %x", numFunctions, functionCounts, functionNames);
    PRINT_INSTR(stdout, "There are %d functions in the code:", *numFunctions);
    PRINT_INSTR(stdout, "Printing functions with at least %d executions", PRINT_MINIMUM);

    for (i = 0; i < *numFunctions; i++){
        if (functionCounts[i] >= PRINT_MINIMUM){
            PRINT_INSTR(stdout, "\tFunction(%d) %.80s executed %d times", i, functionNames[i], functionCounts[i]);
        }
    }

    return i;
}

int32_t initcounter(int32_t* numBlocks, int32_t* lineNums, char** fileNms, char** functionNms, int64_t* hashVals){
    numberOfBasicBlocks = *numBlocks;
    lineNumbers = lineNums;
    fileNames = fileNms;
    functionNames = functionNms;
    hashValues = hashVals;

#ifdef USING_MPI_WRAPPERS
    // use an unlikely value, so if we see this value we know there was
    // a problem getting task id
    taskid = 0xdeadbeef;
#else
    taskid = 0;
#endif

    return numberOfBasicBlocks;
}

int32_t blockcounter(int32_t* blockCounts, char* appName, char* instExt){
    int32_t i;

    PRINT_INSTR(stdout, "raw fini args: %x %x %x", blockCounts, appName, instExt);
    PRINT_INSTR(stdout, "actual fini args: %x %s %s", blockCounts, appName, instExt);
    PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
    PRINT_INSTR(stdout, "There are %d basic blocks in the code:", numberOfBasicBlocks);

    char* outFileName = malloc(sizeof(char) * __MAX_STRING_SIZE);
    sprintf(outFileName, "%s.meta_%04d.%s", appName, __taskid, instExt);
    PRINT_INSTR(stdout, "Printing blocks with at least %d executions to file %s", PRINT_MINIMUM, outFileName);
    FILE* outFile = fopen(outFileName, "w");
    free(outFileName);
    if (!outFile){
        fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
        fflush(stderr);
        exit(-1);
    }

    fprintf(outFile, "#id\tcount\t#file:line\tfunc\thash\n");
    fflush(outFile);
    for (i = 0; i < numberOfBasicBlocks; i++){
        if (blockCounts[i] >= PRINT_MINIMUM){
            fprintf(outFile, "%#d\t", i);
            fprintf(outFile, "%d\t#", blockCounts[i]);
            fprintf(outFile, "%s:", fileNames[i]);
            fprintf(outFile, "%d\t", lineNumbers[i]);
            fprintf(outFile, "%s\t", functionNames[i]);
            fprintf(outFile, "%#lld\n", hashValues[i]);
            fflush(outFile);
        }
    }
    fflush(outFile);
    fclose(outFile);

    return i;
}

