/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    if (memloc == 0){
        memval = 0;
    } else {
        memval = *((DINT_TYPE*)memloc);
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

    return numberOfBasicBlocks;
}

int32_t blockcounter(uint64_t* blockCounts, char* appName, char* instExt){
    int32_t i;

#ifndef HAVE_MPI
    taskid = getpid();
#endif

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
            fprintf(outFile, "%llu\t#", blockCounts[i]);
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

